#include "Framework.hpp"
#include "MapStorage.hpp"

#include "../opengl/androidoglcontextfactory.hpp"

#include "../core/jni_helper.hpp"

#include "../country/country_helper.hpp"

#include "../platform/Platform.hpp"

#include "map/information_display.hpp"
#include "map/user_mark.hpp"

#include "drape_frontend/visual_params.hpp"

#include "coding/file_container.hpp"
#include "coding/file_name_utils.hpp"

#include "geometry/angles.hpp"

#include "platform/country_file.hpp"
#include "platform/local_country_file.hpp"
#include "platform/local_country_file_utils.hpp"
#include "platform/location.hpp"
#include "platform/measurement_utils.hpp"
#include "platform/platform.hpp"
#include "platform/preferred_languages.hpp"
#include "platform/settings.hpp"

#include "base/math.hpp"
#include "base/logging.hpp"

#include "std/chrono.hpp"

namespace
{
const unsigned LONG_TOUCH_MS = 1000;
const unsigned SHORT_TOUCH_MS = 250;
const double DOUBLE_TOUCH_S = SHORT_TOUCH_MS / 1000.0;
}

android::Framework * g_framework = 0;

using namespace storage;
using platform::CountryFile;
using platform::LocalCountryFile;

namespace
{

::Framework * frm()
{
  return g_framework->NativeFramework();
}

}

namespace android
{

enum MultiTouchAction
{
  MULTITOUCH_UP    =   0x00000001,
  MULTITOUCH_DOWN  =   0x00000002,
  MULTITOUCH_MOVE  =   0x00000003,
  MULTITOUCH_CANCEL =  0x00000004
};

void ShowAllSearchResultsImpl()
{
  frm()->ShowAllSearchResults();
}

Framework::Framework()
 : m_mask(0),
   m_isCleanSingleClick(false),
   m_doLoadState(true),
   m_lastCompass(0.0),
   m_wasLongClick(false)
{
  ASSERT_EQUAL ( g_framework, 0, () );
  g_framework = this;
  m_activeMapsConnectionID = m_work.GetCountryTree().GetActiveMapLayout().AddListener(this);
}

Framework::~Framework()
{
  m_work.GetCountryTree().GetActiveMapLayout().RemoveListener(m_activeMapsConnectionID);
}

void Framework::OnLocationError(int errorCode)
{
  m_work.OnLocationError(static_cast<location::TLocationError>(errorCode));
}

void Framework::OnLocationUpdated(location::GpsInfo const & info)
{
  Platform::RunOnGuiThreadImpl(bind(&::Framework::OnLocationUpdate, ref(m_work), info));
}

void Framework::OnCompassUpdated(location::CompassInfo const & info)
{
  static double const COMPASS_THRASHOLD = my::DegToRad(1.0);

  /// @todo Do not emit compass bearing too often while we are passing it through nv-queue.
  /// Need to make more experiments in future.
  if (fabs(ang::GetShortestDistance(m_lastCompass, info.m_bearing)) >= COMPASS_THRASHOLD)
  {
    m_lastCompass = info.m_bearing;
    Platform::RunOnGuiThreadImpl(bind(&::Framework::OnCompassUpdate, ref(m_work), info));
  }
}

void Framework::UpdateCompassSensor(int ind, float * arr)
{
  m_sensors[ind].Next(arr);
}

float Framework::GetBestDensity(int densityDpi)
{
  typedef pair<int, float> P;
  P dens[] = {
      P(120, 0.75f),
      P(160, 1.0f),
      P(240, 1.5f),
      P(320, 2.0f),
      P(480, 3.0f)
  };

  int prevRange = numeric_limits<int>::max();
  int bestRangeIndex = 0;
  for (int i = 0; i < ARRAY_SIZE(dens); i++)
  {
    int currRange = abs(densityDpi - dens[i].first);
    if (currRange <= prevRange)
    {
      // it is better, take index
      bestRangeIndex = i;
      prevRange = currRange;
    }
    else
      break;
  }

  return dens[bestRangeIndex].second;
}

bool Framework::CreateDrapeEngine(JNIEnv * env, jobject jSurface, int densityDpi)
{
  m_contextFactory.Reset(new dp::ThreadSafeFactory(new AndroidOGLContextFactory(env, jSurface)));
  AndroidOGLContextFactory const * factory = m_contextFactory->CastFactory<AndroidOGLContextFactory>();
  if (!factory->IsValid())
    return false;

  float visualScale = GetBestDensity(densityDpi);
  m_work.CreateDrapeEngine(m_contextFactory.GetRefPointer(), visualScale, factory->GetWidth(), factory->GetHeight());
  m_work.SetUpdatesEnabled(true);
  m_work.EnterForeground();
  LoadState();
  m_work.LoadBookmarks();

  return true;
}

void Framework::DeleteDrapeEngine()
{
  SaveState();
  m_work.DestroyDrapeEngine();
}

void Framework::Resize(int w, int h)
{
  m_contextFactory->CastFactory<AndroidOGLContextFactory>()->UpdateSurfaceSize();
  m_work.OnSize(w, h);
}

void Framework::SetMapStyle(MapStyle mapStyle)
{
  //@TODO UVR
}

Storage & Framework::Storage()
{
  return m_work.Storage();
}

void Framework::ShowCountry(TIndex const & idx, bool zoomToDownloadButton)
{
  m_doLoadState = false;

  if (zoomToDownloadButton)
  {
      m2::RectD const rect = m_work.GetCountryBounds(idx);
      double const lon = MercatorBounds::XToLon(rect.Center().x);
      double const lat = MercatorBounds::YToLat(rect.Center().y);
      m_work.ShowRect(lat, lon, 10);
  }
  else
    m_work.ShowCountry(idx);
}

TStatus Framework::GetCountryStatus(TIndex const & idx) const
{
  return m_work.GetCountryStatus(idx);
}

void Framework::Move(int mode, double x, double y)
{
  DragEvent e(x, y);
  switch (mode)
  {
  case 0: m_work.StartDrag(e); break;
  case 1: m_work.DoDrag(e); break;
  case 2: m_work.StopDrag(e); break;
  }
}

void Framework::Zoom(int mode, double x1, double y1, double x2, double y2)
{
  ScaleEvent e(x1, y1, x2, y2);
  switch (mode)
  {
  case 0: m_work.StartScale(e); break;
  case 1: m_work.DoScale(e); break;
  case 2: m_work.StopScale(e); break;
  }
}

void Framework::StartTouchTask(double x, double y, unsigned ms)
{
  KillTouchTask();
  m_deferredTask.reset(new DeferredTask(
      bind(&android::Framework::OnProcessTouchTask, this, x, y, ms), milliseconds(ms)));
}

void Framework::KillTouchTask() { m_deferredTask.reset(); }

/// @param[in] mask Active pointers bits : 0x0 - no, 0x1 - (x1, y1), 0x2 - (x2, y2), 0x3 - (x1, y1)(x2, y2).
void Framework::Touch(int action, int mask, double x1, double y1, double x2, double y2)
{
  MultiTouchAction eventType = static_cast<MultiTouchAction>(action);

  // Check if we touch is canceled or we get coordinates NOT from the first pointer.
  if ((mask != 0x1) || (eventType == MULTITOUCH_CANCEL))
  {
    ///@TODO UVR
    //if (mask == 0x1)
    //  m_work.GetGuiController()->OnTapCancelled(m2::PointD(x1, y1));

    m_isCleanSingleClick = false;
    KillTouchTask();
  }
  else
  {
    ASSERT_EQUAL(mask, 0x1, ());

    if (eventType == MULTITOUCH_DOWN)
    {
      KillTouchTask();

      m_wasLongClick = false;
      m_isCleanSingleClick = true;
      m_lastX1 = x1;
      m_lastY1 = y1;

      ///@TODO UVR
      //if (m_work.GetGuiController()->OnTapStarted(m2::PointD(x1, y1)))
      //  return;

      StartTouchTask(x1, y1, LONG_TOUCH_MS);
    }

    if (eventType == MULTITOUCH_MOVE)
    {
      double const minDist = df::VisualParams::Instance().GetVisualScale() * 10.0;
      if ((fabs(x1 - m_lastX1) > minDist) || (fabs(y1 - m_lastY1) > minDist))
      {
        m_isCleanSingleClick = false;
        KillTouchTask();
      }

      ///@TODO UVR
      //if (m_work.GetGuiController()->OnTapMoved(m2::PointD(x1, y1)))
      //  return;
    }

    if (eventType == MULTITOUCH_UP)
    {
      KillTouchTask();

      ///@TODO UVR
      //if (m_work.GetGuiController()->OnTapEnded(m2::PointD(x1, y1)))
      //  return;

      if (!m_wasLongClick && m_isCleanSingleClick)
      {
        if (m_doubleClickTimer.ElapsedSeconds() <= DOUBLE_TOUCH_S)
        {
          // performing double-click
          m_work.ScaleToPoint(ScaleToPointEvent(x1, y1, 1.5));
        }
        else
        {
          // starting single touch task
          StartTouchTask(x1, y1, SHORT_TOUCH_MS);

          // starting double click
          m_doubleClickTimer.Reset();
        }
      }
      else
        m_wasLongClick = false;
    }
  }

  // general case processing
  if (m_mask != mask)
  {
    if (m_mask == 0x0)
    {
      if (mask == 0x1)
        m_work.StartDrag(DragEvent(x1, y1));

      if (mask == 0x2)
        m_work.StartDrag(DragEvent(x2, y2));

      if (mask == 0x3)
        m_work.StartScale(ScaleEvent(x1, y1, x2, y2));
    }

    if (m_mask == 0x1)
    {
      m_work.StopDrag(DragEvent(x1, y1));

      if (mask == 0x0)
      {
        if ((eventType != MULTITOUCH_UP) && (eventType != MULTITOUCH_CANCEL))
          LOG(LWARNING, ("should be MULTITOUCH_UP or MULTITOUCH_CANCEL"));
      }

      if (m_mask == 0x2)
        m_work.StartDrag(DragEvent(x2, y2));

      if (mask == 0x3)
        m_work.StartScale(ScaleEvent(x1, y1, x2, y2));
    }

    if (m_mask == 0x2)
    {
      m_work.StopDrag(DragEvent(x2, y2));

      if (mask == 0x0)
      {
        if ((eventType != MULTITOUCH_UP) && (eventType != MULTITOUCH_CANCEL))
          LOG(LWARNING, ("should be MULTITOUCH_UP or MULTITOUCH_CANCEL"));
      }

      if (mask == 0x1)
        m_work.StartDrag(DragEvent(x1, y1));

      if (mask == 0x3)
        m_work.StartScale(ScaleEvent(x1, y1, x2, y2));
    }

    if (m_mask == 0x3)
    {
      m_work.StopScale(ScaleEvent(m_x1, m_y1, m_x2, m_y2));

      if (eventType == MULTITOUCH_MOVE)
      {
        if (mask == 0x1)
          m_work.StartDrag(DragEvent(x1, y1));

        if (mask == 0x2)
          m_work.StartDrag(DragEvent(x2, y2));
      }
      else
        mask = 0;
    }
  }
  else
  {
    if (eventType == MULTITOUCH_MOVE)
    {
      if (m_mask == 0x1)
        m_work.DoDrag(DragEvent(x1, y1));
      if (m_mask == 0x2)
        m_work.DoDrag(DragEvent(x2, y2));
      if (m_mask == 0x3)
        m_work.DoScale(ScaleEvent(x1, y1, x2, y2));
    }

    if ((eventType == MULTITOUCH_CANCEL) || (eventType == MULTITOUCH_UP))
    {
      if (m_mask == 0x1)
        m_work.StopDrag(DragEvent(x1, y1));
      if (m_mask == 0x2)
        m_work.StopDrag(DragEvent(x2, y2));
      if (m_mask == 0x3)
        m_work.StopScale(ScaleEvent(m_x1, m_y1, m_x2, m_y2));
      mask = 0;
    }
  }

  m_x1 = x1;
  m_y1 = y1;
  m_x2 = x2;
  m_y2 = y2;
  m_mask = mask;
}

void Framework::ShowSearchResult(search::Result const & r)
{
  m_doLoadState = false;
  m_work.ShowSearchResult(r);
}

void Framework::ShowAllSearchResults()
{
  m_doLoadState = false;
  Platform::RunOnGuiThreadImpl(bind(&ShowAllSearchResultsImpl));
}

bool Framework::Search(search::SearchParams const & params)
{
  m_searchQuery = params.m_query;
  return m_work.Search(params);
}

void Framework::LoadState()
{
  if (!m_work.LoadState())
    m_work.ShowAll();
}

void Framework::SaveState()
{
  m_work.SaveState();
}

void Framework::Invalidate()
{
  ///@TODO UVR
  //m_work.Invalidate();
}

void Framework::SetupMeasurementSystem()
{
  m_work.SetupMeasurementSystem();
}

void Framework::AddLocalMaps()
{
  m_work.RegisterAllMaps();
}

void Framework::RemoveLocalMaps()
{
  m_work.DeregisterAllMaps();
}

void Framework::GetMapsWithoutSearch(vector<string> & out) const
{
  // Actually, this routing is obsolete and comes from ancient times
  // when mwm was without search index.
  if (!Settings::IsFirstLaunchForDate(150101))
    return;

  ASSERT(out.empty(), ());

  ::Platform const & pl = GetPlatform();

  vector<LocalCountryFile> localFiles;
  platform::FindAllLocalMaps(localFiles);

  for (LocalCountryFile const & localFile : localFiles)
  {
    CountryFile const countryFile = localFile.GetCountryFile();
    // skip World and WorldCoast
    if (countryFile.GetNameWithoutExt() == WORLD_FILE_NAME ||
        countryFile.GetNameWithoutExt() == WORLD_COASTS_FILE_NAME)
    {
      continue;
    }
    try
    {
      FilesContainerR cont(platform::GetCountryReader(localFile, MapOptions::Map));
      if (!cont.IsExist(SEARCH_INDEX_FILE_TAG))
        out.push_back(countryFile.GetNameWithoutExt());
    }
    catch (RootException const & ex)
    {
      // sdcard can contain dummy _*.mwm files. Suppress these errors.
      LOG(LWARNING, ("Bad mwm file:", countryFile.GetNameWithoutExt(), "Error:", ex.Msg()));
    }
  }
}

TIndex Framework::GetCountryIndex(double lat, double lon) const
{
  return m_work.GetCountryIndex(MercatorBounds::FromLatLon(lat, lon));
}

string Framework::GetCountryCode(double lat, double lon) const
{
  return m_work.GetCountryCode(MercatorBounds::FromLatLon(lat, lon));
}

string Framework::GetCountryNameIfAbsent(m2::PointD const & pt) const
{
  TIndex const idx = m_work.GetCountryIndex(pt);
  TStatus const status = m_work.GetCountryStatus(idx);
  if (status != TStatus::EOnDisk && status != TStatus::EOnDiskOutOfDate)
    return m_work.GetCountryName(idx);
  else
    return string();
}

m2::PointD Framework::GetViewportCenter() const
{
  return m_work.GetViewportCenter();
}

void Framework::AddString(string const & name, string const & value)
{
  m_work.AddString(name, value);
}

void Framework::Scale(double k)
{
  m_work.Scale(k);
}

::Framework * Framework::NativeFramework()
{
  return &m_work;
}

void Framework::OnProcessTouchTask(double x, double y, unsigned ms)
{
  m_wasLongClick = (ms == LONG_TOUCH_MS);
  GetPinClickManager().OnShowMark(m_work.GetUserMark(m2::PointD(x, y), m_wasLongClick));
}

BookmarkAndCategory Framework::AddBookmark(size_t cat, m2::PointD const & pt, BookmarkData & bm)
{
  return BookmarkAndCategory(cat, m_work.AddBookmark(cat, pt, bm));
}

void Framework::ReplaceBookmark(BookmarkAndCategory const & ind, BookmarkData & bm)
{
  m_work.ReplaceBookmark(ind.first, ind.second, bm);
}

size_t Framework::ChangeBookmarkCategory(BookmarkAndCategory const & ind, size_t newCat)
{
  return m_work.MoveBookmark(ind.second, ind.first, newCat);
}

bool Framework::ShowMapForURL(string const & url)
{
  /// @todo this is weird hack, we should reconsider Android lifecycle handling design
  m_doLoadState = false;

  return m_work.ShowMapForURL(url);
}

void Framework::DeactivatePopup()
{
  GetPinClickManager().RemovePin();
}

string Framework::GetOutdatedCountriesString()
{
  vector<Country const *> countries;
  Storage().GetOutdatedCountries(countries);

  string res;
  for (size_t i = 0; i < countries.size(); ++i)
  {
    res += countries[i]->Name();
    if (i < countries.size() - 1)
      res += ", ";
  }
  return res;
}

void Framework::ShowTrack(int category, int track)
{
  Track const * nTrack = NativeFramework()->GetBmCategory(category)->GetTrack(track);
  m_doLoadState = false;
  NativeFramework()->ShowTrack(*nTrack);
}

void Framework::SetCountryTreeListener(shared_ptr<jobject> objPtr)
{
  m_javaCountryListener = objPtr;
  m_work.GetCountryTree().SetListener(this);
}

void Framework::ResetCountryTreeListener()
{
  m_work.GetCountryTree().ResetListener();
  m_javaCountryListener.reset();
}

int Framework::AddActiveMapsListener(shared_ptr<jobject> obj)
{
  m_javaActiveMapListeners[m_currentSlotID] = obj;
  return m_currentSlotID++;
}

void Framework::RemoveActiveMapsListener(int slotID)
{
  m_javaActiveMapListeners.erase(slotID);
}

//////////////////////////////////////////////////////////////////////////////////////////
void Framework::ItemStatusChanged(int childPosition)
{
  if (m_javaCountryListener == NULL)
    return;

  JNIEnv * env = jni::GetEnv();
  static jmethodID const methodID = jni::GetJavaMethodID(env,
                                                  *m_javaCountryListener,
                                                  "onItemStatusChanged",
                                                  "(I)V");
  ASSERT ( methodID, () );

  env->CallVoidMethod(*m_javaCountryListener, methodID, childPosition);
}

void Framework::ItemProgressChanged(int childPosition, LocalAndRemoteSizeT const & sizes)
{
  if (m_javaCountryListener == NULL)
    return;

  JNIEnv * env = jni::GetEnv();
  static jmethodID const methodID = jni::GetJavaMethodID(env,
                                                  *m_javaCountryListener,
                                                  "onItemProgressChanged",
                                                  "(I[J)V");
  ASSERT ( methodID, () );

  env->CallVoidMethod(*m_javaCountryListener, methodID, childPosition, storage_utils::ToArray(env, sizes));
}

void Framework::CountryGroupChanged(ActiveMapsLayout::TGroup const & oldGroup, int oldPosition,
                                    ActiveMapsLayout::TGroup const & newGroup, int newPosition)
{
  JNIEnv * env = jni::GetEnv();
  for (TListenerMap::const_iterator it = m_javaActiveMapListeners.begin(); it != m_javaActiveMapListeners.end(); ++it)
  {
    jmethodID const methodID = jni::GetJavaMethodID(env, *(it->second), "onCountryGroupChanged", "(IIII)V");
    ASSERT ( methodID, () );

    env->CallVoidMethod(*(it->second), methodID, oldGroup, oldPosition, newGroup, newPosition);
  }
}

void Framework::CountryStatusChanged(ActiveMapsLayout::TGroup const & group, int position,
                                     TStatus const & oldStatus, TStatus const & newStatus)
{
  JNIEnv * env = jni::GetEnv();
  for (TListenerMap::const_iterator it = m_javaActiveMapListeners.begin(); it != m_javaActiveMapListeners.end(); ++it)
  {
    jmethodID const methodID = jni::GetJavaMethodID(env, *(it->second), "onCountryStatusChanged", "(IIII)V");
    ASSERT ( methodID, () );

    env->CallVoidMethod(*(it->second), methodID, group, position,
                           static_cast<jint>(oldStatus), static_cast<jint>(newStatus));
  }
}

void Framework::CountryOptionsChanged(ActiveMapsLayout::TGroup const & group, int position,
                                      MapOptions const & oldOpt, MapOptions const & newOpt)
{
  JNIEnv * env = jni::GetEnv();
  for (TListenerMap::const_iterator it = m_javaActiveMapListeners.begin(); it != m_javaActiveMapListeners.end(); ++it)
  {
    jmethodID const methodID = jni::GetJavaMethodID(env, *(it->second), "onCountryOptionsChanged", "(IIII)V");
    ASSERT ( methodID, () );

    env->CallVoidMethod(*(it->second), methodID, group, position,
                            static_cast<jint>(oldOpt), static_cast<jint>(newOpt));
  }
}

void Framework::DownloadingProgressUpdate(ActiveMapsLayout::TGroup const & group, int position,
                                          LocalAndRemoteSizeT const & progress)
{
  JNIEnv * env = jni::GetEnv();
  for (TListenerMap::const_iterator it = m_javaActiveMapListeners.begin(); it != m_javaActiveMapListeners.end(); ++it)
  {
    jmethodID const methodID = jni::GetJavaMethodID(env, *(it->second), "onCountryProgressChanged", "(II[J)V");
    ASSERT ( methodID, () );

    env->CallVoidMethod(*(it->second), methodID, group, position, storage_utils::ToArray(env, progress));
  }
}

// Fills mapobject's metadata from UserMark
void Framework::InjectMetadata(JNIEnv * env, jclass const clazz, jobject const mapObject, UserMark const * userMark)
{
  feature::FeatureMetadata metadata;
  frm()->FindClosestPOIMetadata(userMark->GetOrg(), metadata);

  static jmethodID const addId = env->GetMethodID(clazz, "addMetadata", "(ILjava/lang/String;)V");
  ASSERT ( addId, () );

  for (feature::Metadata::EType t : metadata.GetPresentTypes())
  {
    jstring metaString = jni::ToJavaString(env, metadata.Get(t));
    env->CallVoidMethod(mapObject, addId, t, metaString);
    // TODO use unique_ptrs for autoallocation of local refs
    env->DeleteLocalRef(metaString);
  }
}

} // namespace android

template <class T>
T const * CastMark(UserMark const * data)
{
  return static_cast<T const *>(data);
}


//============ GLUE CODE for com.mapswithme.maps.Framework class =============//
/*            ____
 *          _ |||| _
 *          \\    //
 *           \\  //
 *            \\//
 *             \/
 */

namespace
{
pair<jintArray, jobjectArray> NativeMetadataToJavaMetadata(JNIEnv * env, feature::Metadata const & metadata)
{
  vector<feature::Metadata::EType> const metaTypes = metadata.GetPresentTypes();
  // FIXME arrays, allocated through New<Type>Array should be deleted manually in the method.
  // refactor that to delete refs locally or pass arrays from outside context
  const jintArray j_metaTypes = env->NewIntArray(metadata.Size());
  jint * arr = env->GetIntArrayElements(j_metaTypes, 0);
  const jobjectArray j_metaValues = env->NewObjectArray(metadata.Size(), jni::GetStringClass(env), 0);

  for (size_t i = 0; i < metaTypes.size(); i++)
  {
    arr[i] = metaTypes[i];
    jstring metaString = jni::ToJavaString(env, metadata.Get(metaTypes[i]));
    env->SetObjectArrayElement(j_metaValues, i, metaString);
    env->DeleteLocalRef(metaString);
  }
  env->ReleaseIntArrayElements(j_metaTypes, arr, 0);

  return make_pair(j_metaTypes, j_metaValues);
}
} // namespace

extern "C"
{
  // API
  void CallOnApiPointActivatedListener(shared_ptr<jobject> obj, ApiMarkPoint const * data, double lat, double lon)
  {
    JNIEnv * env = jni::GetEnv();
    static jmethodID const methodID = jni::GetJavaMethodID(env,
                                                    *obj.get(),
                                                   "onApiPointActivated",
                                                   "(DDLjava/lang/String;Ljava/lang/String;)V");

    jstring j_name = jni::ToJavaString(env, data->GetName());
    jstring j_id = jni::ToJavaString(env, data->GetID());

    env->CallVoidMethod(*obj.get(), methodID, lat, lon, j_name, j_id);

    // TODO use unique_ptrs for autoallocation of local refs
    env->DeleteLocalRef(j_id);
    env->DeleteLocalRef(j_name);
  }

  // Additional layer
  void CallOnAdditionalLayerActivatedListener(shared_ptr<jobject> obj, m2::PointD const & globalPoint,
      search::AddressInfo const & addrInfo, feature::Metadata const & metadata)
  {
    JNIEnv * env = jni::GetEnv();

    const jstring j_name = jni::ToJavaString(env, addrInfo.GetPinName());
    const jstring j_type = jni::ToJavaString(env, addrInfo.GetPinType());
    const double lon = MercatorBounds::XToLon(globalPoint.x);
    const double lat = MercatorBounds::YToLat(globalPoint.y);

    pair<jintArray, jobjectArray> const meta = NativeMetadataToJavaMetadata(env, metadata);

    const char * signature = "(Ljava/lang/String;Ljava/lang/String;DD[I[Ljava/lang/String;)V";
    static jmethodID const methodId = jni::GetJavaMethodID(env, *obj.get(),
                                                      "onAdditionalLayerActivated", signature);
    env->CallVoidMethod(*obj.get(), methodId, j_name, j_type, lat, lon, meta.first, meta.second);

    env->DeleteLocalRef(j_type);
    env->DeleteLocalRef(j_name);

    env->DeleteLocalRef(meta.second);
    env->DeleteLocalRef(meta.first);
  }

  // POI
  void CallOnPoiActivatedListener(shared_ptr<jobject> obj, m2::PointD const & globalPoint,
      search::AddressInfo const & addrInfo, feature::Metadata const & metadata)
  {
    JNIEnv * env = jni::GetEnv();

    const jstring j_name = jni::ToJavaString(env, addrInfo.GetPinName());
    const jstring j_type = jni::ToJavaString(env, addrInfo.GetPinType());
    const jstring j_address = jni::ToJavaString(env, addrInfo.FormatAddress());
    const double lon = MercatorBounds::XToLon(globalPoint.x);
    const double lat = MercatorBounds::YToLat(globalPoint.y);

    pair<jintArray, jobjectArray> const meta = NativeMetadataToJavaMetadata(env, metadata);

    const char * signature = "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;DD[I[Ljava/lang/String;)V";
    static jmethodID const methodId = jni::GetJavaMethodID(env, *obj.get(),
                                                      "onPoiActivated", signature);
    env->CallVoidMethod(*obj.get(), methodId, j_name, j_type, j_address, lat, lon, meta.first, meta.second);

    env->DeleteLocalRef(meta.second);
    env->DeleteLocalRef(meta.first);
  }

  // Bookmark
  void CallOnBookmarkActivatedListener(shared_ptr<jobject> obj, BookmarkAndCategory const & bmkAndCat)
  {
    JNIEnv * env = jni::GetEnv();
    static jmethodID const methodId = jni::GetJavaMethodID(env, *obj.get(),
                                                    "onBookmarkActivated", "(II)V");
    env->CallVoidMethod(*obj.get(), methodId, bmkAndCat.first, bmkAndCat.second);
  }

  // My position
  void CallOnMyPositionActivatedListener(shared_ptr<jobject> obj, double lat, double lon)
  {
    JNIEnv * env = jni::GetEnv();
    static jmethodID const methodId = jni::GetJavaMethodID(env, *obj.get(),
                                                    "onMyPositionActivated", "(DD)V");
    env->CallVoidMethod(*obj.get(), methodId, lat, lon);
  }

  void CallOnUserMarkActivated(shared_ptr<jobject> obj, unique_ptr<UserMarkCopy> markCopy)
  {
    ::Framework * fm = frm();
    UserMark const * mark = markCopy->GetUserMark();
    fm->ActivateUserMark(mark);

    switch (mark->GetMarkType())
    {
    case UserMark::Type::API:
      {
        double lat, lon;
        mark->GetLatLon(lat, lon);
        CallOnApiPointActivatedListener(obj, CastMark<ApiMarkPoint>(mark), lat, lon);
        break;
      }

    case UserMark::Type::BOOKMARK:
      {
        BookmarkAndCategory bmAndCat = fm->FindBookmark(mark);
        if (IsValid(bmAndCat))
          CallOnBookmarkActivatedListener(obj, bmAndCat);
        break;
      }

    case UserMark::Type::POI:
      {
        PoiMarkPoint const * poiMark = CastMark<PoiMarkPoint>(mark);
        CallOnPoiActivatedListener(obj, mark->GetOrg(), poiMark->GetInfo(), poiMark->GetMetadata());
        break;
      }

    case UserMark::Type::SEARCH:
      {
        SearchMarkPoint const * searchMark = CastMark<SearchMarkPoint>(mark);
        feature::Metadata metadata;
        fm->FindClosestPOIMetadata(mark->GetOrg(), metadata);
        CallOnAdditionalLayerActivatedListener(obj, searchMark->GetOrg(), searchMark->GetInfo(), metadata);
        break;
      }

    case UserMark::Type::MY_POSITION:
      {
        double lat, lon;
        mark->GetLatLon(lat, lon);
        CallOnMyPositionActivatedListener(obj, lat, lon);
        break;
      }
    case UserMark::Type::DEBUG_MARK:
      // Ignore clicks to debug marks.
      break;
    }
  }

  // Dismiss information box
  void CallOnDismissListener(shared_ptr<jobject> obj)
  {
    JNIEnv * env = jni::GetEnv();
    static jmethodID const methodId = jni::GetJavaMethodID(env, *obj.get(), "onDismiss", "()V");
    ASSERT(methodId, ());
    env->CallVoidMethod(*obj.get(), methodId);
  }

  void CallRoutingListener(shared_ptr<jobject> obj, int errorCode, vector<storage::TIndex> const & absentCountries, vector<storage::TIndex> const & absentRoutes)
  {
    JNIEnv * env = jni::GetEnv();
    // cache methodID - it cannot change after class is loaded.
    // http://developer.android.com/training/articles/perf-jni.html#jclass_jmethodID_and_jfieldID more details here
    static jmethodID const methodId = jni::GetJavaMethodID(env, *obj.get(), "onRoutingEvent",
                                                           "(I[Lcom/mapswithme/maps/MapStorage$Index;[Lcom/mapswithme/maps/MapStorage$Index;)V");
    ASSERT(methodId, ());

    jobjectArray const countriesJava = env->NewObjectArray(absentCountries.size(), g_indexClazz, 0);
    for (size_t i = 0; i < absentCountries.size(); i++)
    {
      jobject country = storage::ToJava(absentCountries[i]);
      env->SetObjectArrayElement(countriesJava, i, country);
      env->DeleteLocalRef(country);
    }

    jobjectArray const routesJava = env->NewObjectArray(absentRoutes.size(), g_indexClazz, 0);
    for (size_t i = 0; i < absentRoutes.size(); i++)
    {
      jobject route = storage::ToJava(absentRoutes[i]);
      env->SetObjectArrayElement(routesJava, i, route);
      env->DeleteLocalRef(route);
    }

    env->CallVoidMethod(*obj.get(), methodId, errorCode, countriesJava, routesJava);

    env->DeleteLocalRef(countriesJava);
  }

  void CallRouteProgressListener(shared_ptr<jobject> sharedListener, float progress)
  {
    JNIEnv * env = jni::GetEnv();
    jobject listener = *sharedListener.get();
    static jmethodID const methodId = jni::GetJavaMethodID(env, listener, "onRouteBuildingProgress", "(F)V");
    env->CallVoidMethod(listener, methodId, progress);
  }

  /// @name JNI EXPORTS
  //@{
  JNIEXPORT jstring JNICALL
  Java_com_mapswithme_maps_Framework_nativeGetNameAndAddress4Point(JNIEnv * env, jclass clazz, jdouble lat, jdouble lon)
  {
    search::AddressInfo info;

    frm()->GetAddressInfoForGlobalPoint(MercatorBounds::FromLatLon(lat, lon), info);

    return jni::ToJavaString(env, info.FormatNameAndAddress());
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_Framework_nativeClearApiPoints(JNIEnv * env, jclass clazz)
  {
    UserMarkControllerGuard guard(frm()->GetBookmarkManager(), UserMarkType::API_MARK);
    guard.m_controller.Clear();
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_Framework_nativeSetBalloonListener(JNIEnv * env, jclass clazz, jobject l)
  {
    PinClickManager & manager = g_framework->GetPinClickManager();
    shared_ptr<jobject> obj = jni::make_global_ref(l);

    manager.ConnectUserMarkListener(bind(&CallOnUserMarkActivated, obj, _1));
    manager.ConnectDismissListener(bind(&CallOnDismissListener, obj));
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_Framework_nativeRemoveBalloonListener(JNIEnv * env, jobject thiz)
  {
    g_framework->GetPinClickManager().ClearListeners();
  }

  JNIEXPORT jstring JNICALL
  Java_com_mapswithme_maps_Framework_nativeGetGe0Url(JNIEnv * env, jclass clazz, jdouble lat, jdouble lon, jdouble zoomLevel, jstring name)
  {
    ::Framework * fr = frm();
    double const scale = (zoomLevel > 0 ? zoomLevel : fr->GetDrawScale());
    const string url = fr->CodeGe0url(lat, lon, scale, jni::ToNativeString(env, name));
    return jni::ToJavaString(env, url);
  }

  JNIEXPORT jobject JNICALL
  Java_com_mapswithme_maps_Framework_nativeGetDistanceAndAzimut(
      JNIEnv * env, jclass clazz, jdouble merX, jdouble merY, jdouble cLat, jdouble cLon, jdouble north)
  {
    string distance;
    double azimut = -1.0;
    frm()->GetDistanceAndAzimut(m2::PointD(merX, merY), cLat, cLon, north, distance, azimut);

    static jclass const daClazz = jni::GetGlobalClassRef(env, "com/mapswithme/maps/bookmarks/data/DistanceAndAzimut");
    // Java signature : DistanceAndAzimut(String distance, double azimuth)
    static jmethodID const methodID = env->GetMethodID(daClazz, "<init>", "(Ljava/lang/String;D)V");
    ASSERT ( methodID, () );

    return env->NewObject(daClazz, methodID,
                          jni::ToJavaString(env, distance.c_str()),
                          static_cast<jdouble>(azimut));
  }

  JNIEXPORT jobject JNICALL
  Java_com_mapswithme_maps_Framework_nativeGetDistanceAndAzimutFromLatLon(
      JNIEnv * env, jclass clazz, jdouble lat, jdouble lon, jdouble cLat, jdouble cLon, jdouble north)
  {
    const double merY = MercatorBounds::LatToY(lat);
    const double merX = MercatorBounds::LonToX(lon);
    return Java_com_mapswithme_maps_Framework_nativeGetDistanceAndAzimut(env, clazz, merX, merY, cLat, cLon, north);
  }

  JNIEXPORT jobject JNICALL
  Java_com_mapswithme_maps_Framework_nativeFormatLatLon(JNIEnv * env, jclass clazz, jdouble lat, jdouble lon, jboolean useDMSFormat)
  {
    if (useDMSFormat)
      return jni::ToJavaString(env,  MeasurementUtils::FormatLatLonAsDMS(lat, lon, 2));
    else
      return jni::ToJavaString(env,  MeasurementUtils::FormatLatLon(lat, lon, 6));
  }

  JNIEXPORT jobjectArray JNICALL
  Java_com_mapswithme_maps_Framework_nativeFormatLatLonToArr(JNIEnv * env, jclass clazz, jdouble lat, jdouble lon, jboolean useDMSFormat)
  {
    string slat, slon;
    if (useDMSFormat)
      MeasurementUtils::FormatLatLonAsDMS(lat, lon, slat, slon, 2);
    else
      MeasurementUtils::FormatLatLon(lat, lon, slat, slon, 6);

    static jclass const klass = jni::GetGlobalClassRef(env, "java/lang/String");
    jobjectArray arr = env->NewObjectArray(2, klass, 0);

    env->SetObjectArrayElement(arr, 0, jni::ToJavaString(env, slat));
    env->SetObjectArrayElement(arr, 1, jni::ToJavaString(env, slon));

    return arr;
  }

  JNIEXPORT jobject JNICALL
  Java_com_mapswithme_maps_Framework_nativeFormatAltitude(JNIEnv * env, jclass clazz, jdouble alt)
  {
    return jni::ToJavaString(env,  MeasurementUtils::FormatAltitude(alt));
  }

  JNIEXPORT jobject JNICALL
  Java_com_mapswithme_maps_Framework_nativeFormatSpeed(JNIEnv * env, jclass clazz, jdouble speed)
  {
    return jni::ToJavaString(env,  MeasurementUtils::FormatSpeed(speed));
  }

  JNIEXPORT jobject JNICALL
  Java_com_mapswithme_maps_Framework_nativeGetOutdatedCountriesString(JNIEnv * env, jclass clazz)
  {
    return jni::ToJavaString(env, g_framework->GetOutdatedCountriesString());
  }

  JNIEXPORT jboolean JNICALL
  Java_com_mapswithme_maps_Framework_nativeIsDataVersionChanged(JNIEnv * env, jclass clazz)
  {
    return frm()->IsDataVersionUpdated() ? JNI_TRUE : JNI_FALSE;
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_Framework_nativeUpdateSavedDataVersion(JNIEnv * env, jclass clazz)
  {
    frm()->UpdateSavedDataVersion();
  }

  JNIEXPORT jint JNICALL
  Java_com_mapswithme_maps_Framework_getDrawScale(JNIEnv * env, jclass clazz)
  {
    return static_cast<jint>(frm()->GetDrawScale());
  }

  JNIEXPORT jdoubleArray JNICALL
  Java_com_mapswithme_maps_Framework_getScreenRectCenter(JNIEnv * env, jclass clazz)
  {
    const m2::PointD center = frm()->GetViewportCenter();

    double latlon[] = {MercatorBounds::YToLat(center.y), MercatorBounds::XToLon(center.x)};
    jdoubleArray jLatLon = env->NewDoubleArray(2);
    env->SetDoubleArrayRegion(jLatLon, 0, 2, latlon);

    return jLatLon;
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_Framework_nativeShowTrackRect(JNIEnv * env, jclass clazz, jint cat, jint track)
  {
    g_framework->ShowTrack(cat, track);
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_Framework_injectData(JNIEnv * env, jclass clazz, jobject jsearchResult, jlong index)
  {
    const size_t nIndex = static_cast<size_t>(index);

    UserMarkControllerGuard guard(frm()->GetBookmarkManager(), UserMarkType::SEARCH_MARK);
    ASSERT_LESS(nIndex , guard.m_controller.GetUserMarkCount(), ("Invalid index", nIndex));
    UserMark const * mark = guard.m_controller.GetUserMark(nIndex);
    search::AddressInfo const & info= CastMark<SearchMarkPoint>(mark)->GetInfo();

    jclass const javaClazz = env->GetObjectClass(jsearchResult);

    static jfieldID const nameId = env->GetFieldID(javaClazz, "mName", "Ljava/lang/String;");
    env->SetObjectField(jsearchResult, nameId, jni::ToJavaString(env, info.GetPinName()));

    static jfieldID const typeId = env->GetFieldID(javaClazz, "mTypeName", "Ljava/lang/String;");
    env->SetObjectField(jsearchResult, typeId, jni::ToJavaString(env, info.GetPinType()));

    static jfieldID const latId = env->GetFieldID(javaClazz, "mLat", "D");
    env->SetDoubleField(jsearchResult, latId, MercatorBounds::YToLat(mark->GetOrg().y));

    static jfieldID const lonId = env->GetFieldID(javaClazz, "mLon", "D");
    env->SetDoubleField(jsearchResult, lonId, MercatorBounds::XToLon(mark->GetOrg().x));

    g_framework->InjectMetadata(env, javaClazz, jsearchResult, mark);
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_Framework_invalidate(JNIEnv * env, jclass clazz)
  {
    g_framework->Invalidate();
  }

  JNIEXPORT jstring JNICALL
  Java_com_mapswithme_maps_Framework_nativeGetBookmarkDir(JNIEnv * env, jclass thiz)
  {
    return jni::ToJavaString(env, GetPlatform().SettingsDir().c_str());
  }

  JNIEXPORT jstring JNICALL
  Java_com_mapswithme_maps_Framework_nativeGetWritableDir(JNIEnv * env, jclass thiz)
  {
    return jni::ToJavaString(env, GetPlatform().WritableDir().c_str());
  }

  JNIEXPORT jstring JNICALL
  Java_com_mapswithme_maps_Framework_nativeGetSettingsDir(JNIEnv * env, jclass thiz)
  {
    return jni::ToJavaString(env, GetPlatform().SettingsDir().c_str());
  }

  JNIEXPORT jobjectArray JNICALL
  Java_com_mapswithme_maps_Framework_nativeGetMovableFilesExts(JNIEnv * env, jclass thiz)
  {
    jclass stringClass = jni::GetStringClass(env);

    vector<string> exts = {DATA_FILE_EXTENSION, FONT_FILE_EXTENSION, ROUTING_FILE_EXTENSION};
    platform::CountryIndexes::GetIndexesExts(exts);
    jobjectArray resultArray = env->NewObjectArray(exts.size(), stringClass, NULL);

    for (size_t i = 0; i < exts.size(); ++i)
      env->SetObjectArrayElement(resultArray, i, jni::ToJavaString(env, exts[i]));

    return resultArray;
  }

  JNIEXPORT jstring JNICALL
  Java_com_mapswithme_maps_Framework_nativeGetBookmarksExt(JNIEnv * env, jclass thiz)
  {
    return jni::ToJavaString(env, BOOKMARKS_FILE_EXTENSION);
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_Framework_nativeSetWritableDir(JNIEnv * env, jclass thiz, jstring jNewPath)
  {
    string newPath = jni::ToNativeString(env, jNewPath);
    g_framework->RemoveLocalMaps();
    android::Platform::Instance().SetStoragePath(newPath);
    g_framework->AddLocalMaps();
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_Framework_nativeLoadBookmarks(JNIEnv * env, jclass thiz)
  {
    android::Platform::RunOnGuiThreadImpl(bind(&::Framework::LoadBookmarks, frm()));
  }

  JNIEXPORT jboolean JNICALL
  Java_com_mapswithme_maps_Framework_nativeIsRoutingActive(JNIEnv * env, jclass thiz)
  {
    return frm()->IsRoutingActive();
  }

  JNIEXPORT jboolean JNICALL
  Java_com_mapswithme_maps_Framework_nativeIsRouteBuilding(JNIEnv * env, jclass thiz)
  {
    return frm()->IsRouteBuilding();
  }

  JNIEXPORT jboolean JNICALL
  Java_com_mapswithme_maps_Framework_nativeIsRouteBuilt(JNIEnv * env, jclass thiz)
  {
    return frm()->IsRouteBuilt();
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_Framework_nativeCloseRouting(JNIEnv * env, jclass thiz)
  {
    android::Platform::RunOnGuiThreadImpl(bind(&::Framework::CloseRouting, frm()));
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_Framework_nativeBuildRoute(JNIEnv * env, jclass thiz, jdouble startLat,
                                                      jdouble startLon,  jdouble finishLat,
                                                      jdouble finishLon)
  {
    android::Platform::RunOnGuiThreadImpl(bind(&::Framework::BuildRoute, frm(),
          MercatorBounds::FromLatLon(startLat, startLon),
          MercatorBounds::FromLatLon(finishLat, finishLon), 0 /* timeoutSec */ ));
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_Framework_nativeFollowRoute(JNIEnv * env, jclass thiz)
  {
    android::Platform::RunOnGuiThreadImpl(bind(&::Framework::FollowRoute, frm()));
  }

  JNIEXPORT jobjectArray JNICALL
  Java_com_mapswithme_maps_Framework_nativeGenerateTurnNotifications(JNIEnv * env, jclass thiz)
  {
    ::Framework * fr = frm();
    if (!fr->IsRoutingActive())
      return nullptr;

    vector<string> turnNotifications;
    fr->GenerateTurnNotifications(turnNotifications);
    if (turnNotifications.empty())
      return nullptr;

    // A new java array of Strings for TTS information is allocated here.
    // Then it will be passed to client and then removed by java GC.
    size_t const notificationsSize = turnNotifications.size();
    jobjectArray jNotificationTexts = env->NewObjectArray(notificationsSize, jni::GetStringClass(env), nullptr);

    for (size_t i = 0; i < notificationsSize; ++i)
    {
      jstring const jNotificationText = jni::ToJavaString(env, turnNotifications[i]);
      env->SetObjectArrayElement(jNotificationTexts, i, jNotificationText);
      env->DeleteLocalRef(jNotificationText);
    }

    return jNotificationTexts;
  }

  JNIEXPORT jobject JNICALL
  Java_com_mapswithme_maps_Framework_nativeGetRouteFollowingInfo(JNIEnv * env, jclass thiz)
  {
    ::Framework * fr = frm();
    if (!fr->IsRoutingActive())
      return nullptr;

    location::FollowingInfo info;
    fr->GetRouteFollowingInfo(info);
    if (!info.IsValid())
      return nullptr;

    static jclass const klass = jni::GetGlobalClassRef(env, "com/mapswithme/maps/routing/RoutingInfo");
    // Java signature : RoutingInfo(String distToTarget, String units, String distTurn, String turnSuffix, String currentStreet, String nextStreet,
    //                              double completionPercent, int vehicleTurnOrdinal, int vehicleNextTurnOrdinal, int pedestrianTurnOrdinal,
    //                              double pedestrianDirectionLat, double pedestrianDirectionLon, int exitNum, int totalTime, SingleLaneInfo[] lanes)
    static jmethodID const ctorRouteInfoID =
        env->GetMethodID(klass, "<init>",
                         "(Ljava/lang/String;Ljava/lang/String;"
                         "Ljava/lang/String;Ljava/lang/String;"
                         "Ljava/lang/String;Ljava/lang/String;DIIIDDII"
                         "[Lcom/mapswithme/maps/routing/SingleLaneInfo;)V");
    ASSERT(ctorRouteInfoID, (jni::DescribeException()));

    vector<location::FollowingInfo::SingleLaneInfoClient> const & lanes = info.m_lanes;
    jobjectArray jLanes = nullptr;
    if (!lanes.empty())
    {
      static jclass const laneClass = jni::GetGlobalClassRef(env, "com/mapswithme/maps/routing/SingleLaneInfo");
      size_t const lanesSize = lanes.size();
      jLanes = env->NewObjectArray(lanesSize, laneClass, nullptr);
      ASSERT(jLanes, (jni::DescribeException()));
      static jmethodID const ctorSingleLaneInfoID = env->GetMethodID(laneClass, "<init>", "([BZ)V");
      ASSERT(ctorSingleLaneInfoID, (jni::DescribeException()));

      jbyteArray singleLane = nullptr;
      jobject singleLaneInfo = nullptr;

      for (size_t j = 0; j < lanesSize; ++j)
      {
        size_t const laneSize = lanes[j].m_lane.size();
        singleLane = env->NewByteArray(laneSize);
        ASSERT(singleLane, (jni::DescribeException()));
        env->SetByteArrayRegion(singleLane, 0, laneSize, lanes[j].m_lane.data());
        singleLaneInfo = env->NewObject(laneClass, ctorSingleLaneInfoID, singleLane, lanes[j].m_isRecommended);
        ASSERT(singleLaneInfo, (jni::DescribeException()));
        env->SetObjectArrayElement(jLanes, j, singleLaneInfo);
        env->DeleteLocalRef(singleLaneInfo);
        env->DeleteLocalRef(singleLane);
      }
    }

    jobject const result = env->NewObject(
        klass, ctorRouteInfoID, jni::ToJavaString(env, info.m_distToTarget),
        jni::ToJavaString(env, info.m_targetUnitsSuffix), jni::ToJavaString(env, info.m_distToTurn),
        jni::ToJavaString(env, info.m_turnUnitsSuffix), jni::ToJavaString(env, info.m_sourceName),
        jni::ToJavaString(env, info.m_targetName), info.m_completionPercent, info.m_turn, info.m_nextTurn, info.m_pedestrianTurn,
        info.m_pedestrianDirectionPos.lat, info.m_pedestrianDirectionPos.lon, info.m_exitNum, info.m_time, jLanes);
    ASSERT(result, (jni::DescribeException()));
    return result;
  }

  JNIEXPORT jobject JNICALL
  Java_com_mapswithme_maps_Framework_nativeGetMapObjectForPoint(JNIEnv * env, jclass clazz, jdouble lat, jdouble lon)
  {
    PoiMarkPoint const * poiMark = frm()->GetAddressMark(MercatorBounds::FromLatLon(lat, lon));

    static jclass const klass = jni::GetGlobalClassRef(env, "com/mapswithme/maps/bookmarks/data/MapObject$Poi");
    // Java signature : Poi(String name, double lat, double lon, String typeName)
    static jmethodID const methodID = env->GetMethodID(klass, "<init>", "(Ljava/lang/String;DDLjava/lang/String;)V");

    jobject const mapObject = env->NewObject(klass, methodID, jni::ToJavaString(env, poiMark->GetInfo().GetPinName()),
                                             lat, lon, jni::ToJavaString(env, poiMark->GetInfo().GetPinType()));
    ASSERT(mapObject, ());


    g_framework->InjectMetadata(env, klass, mapObject, poiMark);

    return mapObject;
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_Framework_nativeActivateUserMark(JNIEnv * env, jclass clazz, jdouble lat, jdouble lon)
  {
    ::Framework * fr = frm();
    m2::PointD pxPoint = fr->GtoP(MercatorBounds::FromLatLon(lat, lon));
    UserMark const * mark = fr->GetUserMark(pxPoint, true);
    fr->GetBalloonManager().OnShowMark(mark);
  }

  JNIEXPORT jstring JNICALL
  Java_com_mapswithme_maps_Framework_nativeGetCountryNameIfAbsent(JNIEnv * env, jobject thiz,
      jdouble lat, jdouble lon)
  {
    string const name = g_framework->GetCountryNameIfAbsent(MercatorBounds::FromLatLon(lat, lon));

    return (name.empty() ? 0 : jni::ToJavaString(env, name));
  }

  JNIEXPORT jstring JNICALL
  Java_com_mapswithme_maps_Framework_nativeGetViewportCountryNameIfAbsent(JNIEnv * env, jobject thiz)
  {
    string const name = g_framework->GetCountryNameIfAbsent(g_framework->GetViewportCenter());
    return (name.empty() ? 0 : jni::ToJavaString(env, name));
  }

  JNIEXPORT jobject JNICALL
  Java_com_mapswithme_maps_Framework_nativeGetCountryIndex(JNIEnv * env, jobject thiz,
      jdouble lat, jdouble lon)
  {
    TIndex const idx = g_framework->GetCountryIndex(lat, lon);

    // Return 0 if no any country.
    if (idx.IsValid())
      return ToJava(idx);
    else
      return 0;
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_Framework_nativeShowCountry(JNIEnv * env, jobject thiz, jobject idx, jboolean zoomToDownloadButton)
  {
    g_framework->ShowCountry(ToNative(idx), (bool) zoomToDownloadButton);
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_Framework_nativeSetRoutingListener(JNIEnv * env, jobject thiz, jobject listener)
  {
    frm()->SetRouteBuildingListener(bind(&CallRoutingListener, jni::make_global_ref(listener), _1, _2, _3));
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_Framework_nativeSetRouteProgressListener(JNIEnv * env, jobject thiz, jobject listener)
  {
    frm()->SetRouteProgressListener(bind(&CallRouteProgressListener, jni::make_global_ref(listener), _1));
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_Framework_downloadCountry(JNIEnv * env, jobject thiz, jobject idx)
  {
    storage_utils::GetMapLayout().DownloadMap(storage::ToNative(idx), MapOptions::Map);
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_Framework_deactivatePopup(JNIEnv * env, jobject thiz)
  {
    return g_framework->DeactivatePopup();
  }

  JNIEXPORT jdoubleArray JNICALL
  Java_com_mapswithme_maps_Framework_predictLocation(JNIEnv * env, jobject thiz, jdouble lat, jdouble lon, jdouble accuracy,
                                                     jdouble bearing, jdouble speed, jdouble elapsedSeconds)
  {
    double latitude = lat;
    double longitude = lon;
    ::Framework::PredictLocation(lat, lon, accuracy, bearing, speed, elapsedSeconds);
    double latlon[] = { lat, lon };
    jdoubleArray jLatLon = env->NewDoubleArray(2);
    env->SetDoubleArrayRegion(jLatLon, 0, 2, latlon);

    return jLatLon;
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_Framework_setMapStyle(JNIEnv * env, jclass thiz, jint mapStyle)
  {
    MapStyle const val = static_cast<MapStyle>(mapStyle);
    android::Platform::RunOnGuiThreadImpl(bind(&android::Framework::SetMapStyle, g_framework, val));
  }

  JNIEXPORT jint JNICALL
  Java_com_mapswithme_maps_Framework_getMapStyle(JNIEnv * env, jclass thiz)
  {
    return static_cast<jint>(g_framework->GetMapStyle());
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_Framework_setRouter(JNIEnv * env, jclass thiz, jint routerType)
  {
    routing::RouterType const val = static_cast<routing::RouterType>(routerType);
    android::Platform::RunOnGuiThreadImpl(bind(&android::Framework::SetRouter, g_framework, val));
  }

  JNIEXPORT jint JNICALL
  Java_com_mapswithme_maps_Framework_getRouter(JNIEnv * env, jclass thiz)
  {
    return static_cast<jint>(g_framework->GetRouter());
  }

  JNIEXPORT jint JNICALL
  Java_com_mapswithme_maps_Framework_nativeGetBestRouter(JNIEnv * env, jclass thiz, jdouble srcLat, jdouble srcLon, jdouble dstLat, jdouble dstLon)
  {
    return static_cast<jint>(frm()->GetBestRouter(MercatorBounds::FromLatLon(srcLat, srcLon), MercatorBounds::FromLatLon(dstLat, dstLon)));
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_Framework_setWidgetPivot(JNIEnv * env, jclass thiz, jint widget, jint pivotX, jint pivotY)
  {
    using WidgetType = InformationDisplay::WidgetType;
    WidgetType const widgetType = static_cast<WidgetType>(widget);
    m2::PointD const pivot = m2::PointD(pivotX, pivotY);
    android::Platform::RunOnGuiThreadImpl(bind(&Framework::SetWidgetPivot, frm(), widgetType, pivot));
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_Framework_nativeRegisterMaps(JNIEnv * env, jclass thiz)
  {
    frm()->RegisterAllMaps();
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_Framework_nativeDeregisterMaps(JNIEnv * env, jclass thiz)
  {
    frm()->DeregisterAllMaps();
  }
} // extern "C"
