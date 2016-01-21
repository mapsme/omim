#include "Framework.hpp"
#include "MapStorage.hpp"
#include "UserMarkHelper.hpp"
#include "com/mapswithme/core/jni_helper.hpp"
#include "com/mapswithme/country/country_helper.hpp"
#include "com/mapswithme/opengl/androidoglcontextfactory.hpp"
#include "com/mapswithme/platform/Platform.hpp"

#include "map/user_mark.hpp"

#include "drape_frontend/visual_params.hpp"
#include "drape_frontend/user_event_stream.hpp"
#include "drape/pointers.hpp"
#include "drape/visual_scale.hpp"

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
#include "base/sunrise_sunset.hpp"

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

} // namespace

namespace android
{

enum MultiTouchAction
{
  MULTITOUCH_UP    =   0x00000001,
  MULTITOUCH_DOWN  =   0x00000002,
  MULTITOUCH_MOVE  =   0x00000003,
  MULTITOUCH_CANCEL =  0x00000004
};

Framework::Framework()
  : m_lastCompass(0.0)
  , m_currentMode(location::MODE_UNKNOWN_POSITION)
  , m_isCurrentModeInitialized(false)
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
  m_work.OnLocationUpdate(info);
}

void Framework::OnCompassUpdated(location::CompassInfo const & info, bool forceRedraw)
{
  static double const COMPASS_THRESHOLD = my::DegToRad(1.0);

  /// @todo Do not emit compass bearing too often.
  /// Need to make more experiments in future.
  if (forceRedraw || fabs(ang::GetShortestDistance(m_lastCompass, info.m_bearing)) >= COMPASS_THRESHOLD)
  {
    m_lastCompass = info.m_bearing;
    m_work.OnCompassUpdate(info);
  }
}

void Framework::UpdateCompassSensor(int ind, float * arr)
{
  m_sensors[ind].Next(arr);
}

void Framework::MyPositionModeChanged(location::EMyPositionMode mode)
{
  if (m_myPositionModeSignal != nullptr)
    m_myPositionModeSignal(mode);
}

bool Framework::CreateDrapeEngine(JNIEnv * env, jobject jSurface, int densityDpi)
{
  m_contextFactory = make_unique_dp<dp::ThreadSafeFactory>(new AndroidOGLContextFactory(env, jSurface));
  AndroidOGLContextFactory const * factory = m_contextFactory->CastFactory<AndroidOGLContextFactory>();
  if (!factory->IsValid())
    return false;

  ::Framework::DrapeCreationParams p;
  p.m_surfaceWidth = factory->GetWidth();
  p.m_surfaceHeight = factory->GetHeight();
  p.m_visualScale = dp::VisualScale(densityDpi);
  p.m_hasMyPositionState = m_isCurrentModeInitialized;
  p.m_initialMyPositionState = m_currentMode;
  ASSERT(!m_guiPositions.empty(), ("GUI elements must be set-up before engine is created"));
  p.m_widgetsInitInfo = m_guiPositions;

  m_work.LoadBookmarks();
  m_work.SetMyPositionModeListener(bind(&Framework::MyPositionModeChanged, this, _1));

  m_work.CreateDrapeEngine(make_ref(m_contextFactory), move(p));
  m_work.EnterForeground();

  // Load initial state of the map and execute drape tasks which set up custom state.
  LoadState();
  {
    lock_guard<mutex> lock(m_drapeQueueMutex);
    if (!m_drapeTasksQueue.empty())
      ExecuteDrapeTasks();
  }

  return true;
}

void Framework::DeleteDrapeEngine()
{
  SaveState();
  m_work.DestroyDrapeEngine();
}

bool Framework::IsDrapeEngineCreated()
{
  return m_work.GetDrapeEngine() != nullptr;
}

void Framework::Resize(int w, int h)
{
  m_contextFactory->CastFactory<AndroidOGLContextFactory>()->UpdateSurfaceSize();
  m_work.OnSize(w, h);
}

void Framework::DetachSurface()
{
  m_work.EnterBackground();

  ASSERT(m_contextFactory != nullptr, ());
  AndroidOGLContextFactory * factory = m_contextFactory->CastFactory<AndroidOGLContextFactory>();
  factory->ResetSurface();
}

void Framework::AttachSurface(JNIEnv * env, jobject jSurface)
{
  ASSERT(m_contextFactory != nullptr, ());
  AndroidOGLContextFactory * factory = m_contextFactory->CastFactory<AndroidOGLContextFactory>();
  factory->SetSurface(env, jSurface);

  m_work.EnterForeground();
}

void Framework::SetMapStyle(MapStyle mapStyle)
{
  m_work.SetMapStyle(mapStyle);
}

MapStyle Framework::GetMapStyle() const
{
  return m_work.GetMapStyle();
}

void Framework::Save3dMode(bool allow3d, bool allow3dBuildings)
{
  m_work.Save3dMode(allow3d, allow3dBuildings);
}

void Framework::Set3dMode(bool allow3d, bool allow3dBuildings)
{
  m_work.Allow3dMode(allow3d, allow3dBuildings);
}

void Framework::Get3dMode(bool & allow3d, bool & allow3dBuildings)
{
  m_work.Load3dMode(allow3d, allow3dBuildings);
}

Storage & Framework::Storage()
{
  return m_work.Storage();
}

void Framework::ShowCountry(TIndex const & idx, bool zoomToDownloadButton)
{
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

void Framework::Touch(int action, Finger const & f1, Finger const & f2, uint8_t maskedPointer)
{
  MultiTouchAction eventType = static_cast<MultiTouchAction>(action);
  df::TouchEvent event;

  switch(eventType)
  {
  case MULTITOUCH_DOWN:
    event.m_type = df::TouchEvent::TOUCH_DOWN;
    break;
  case MULTITOUCH_MOVE:
    event.m_type = df::TouchEvent::TOUCH_MOVE;
    break;
  case MULTITOUCH_UP:
    event.m_type = df::TouchEvent::TOUCH_UP;
    break;
  case MULTITOUCH_CANCEL:
    event.m_type = df::TouchEvent::TOUCH_CANCEL;
    break;
  default:
    return;
  }

  event.m_touches[0].m_location = m2::PointD(f1.m_x, f1.m_y);
  event.m_touches[0].m_id = f1.m_id;
  event.m_touches[1].m_location = m2::PointD(f2.m_x, f2.m_y);
  event.m_touches[1].m_id = f2.m_id;

  event.SetFirstMaskedPointer(maskedPointer);
  m_work.TouchEvent(event);
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

void Framework::Scale(::Framework::EScaleMode mode)
{
  m_work.Scale(mode, true);
}

::Framework * Framework::NativeFramework()
{
  return &m_work;
}

bool Framework::Search(search::SearchParams const & params)
{
  m_searchQuery = params.m_query;
  return m_work.Search(params);
}

void Framework::LoadState()
{
  m_work.LoadState();
}

void Framework::SaveState()
{
  m_work.SaveState();
}

void Framework::AddLocalMaps()
{
  m_work.RegisterAllMaps();
}

void Framework::RemoveLocalMaps()
{
  m_work.DeregisterAllMaps();
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
  return m_work.ShowMapForURL(url);
}

void Framework::DeactivatePopup()
{
  m_work.ResetLastTapEvent();
  m_work.DeactivateUserMark();
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

void Framework::SetMyPositionModeListener(location::TMyPositionModeChanged const & fn)
{
  m_myPositionModeSignal = fn;
}

location::EMyPositionMode Framework::GetMyPositionMode() const
{
  if (!m_isCurrentModeInitialized)
    return location::MODE_UNKNOWN_POSITION;

  return m_currentMode;
}

void Framework::SetMyPositionMode(location::EMyPositionMode mode)
{
  m_currentMode = mode;
  m_isCurrentModeInitialized = true;
}

void Framework::SetupWidget(gui::EWidget widget, float x, float y, dp::Anchor anchor)
{
  m_guiPositions[widget] = gui::Position(m2::PointF(x, y), anchor);
}

void Framework::ApplyWidgets()
{
  gui::TWidgetsLayoutInfo layout;
  for (auto const & widget : m_guiPositions)
    layout[widget.first] = widget.second.m_pixelPivot;

  m_work.SetWidgetLayout(move(layout));
}

void Framework::CleanWidgets()
{
  m_guiPositions.clear();
}

void Framework::SetupMeasurementSystem()
{
  m_work.SetupMeasurementSystem();
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

void Framework::PostDrapeTask(TDrapeTask && task)
{
  ASSERT(task != nullptr, ());
  lock_guard<mutex> lock(m_drapeQueueMutex);
  if (IsDrapeEngineCreated())
    task();
  else
    m_drapeTasksQueue.push_back(move(task));
}

void Framework::ExecuteDrapeTasks()
{
  for (auto & task : m_drapeTasksQueue)
    task();
  m_drapeTasksQueue.clear();
}

void Framework::SetActiveUserMark(UserMark const * mark)
{
  m_activeUserMark = mark;
}

UserMark const * Framework::GetActiveUserMark()
{
  return m_activeUserMark;
}

} // namespace android

//============ GLUE CODE for com.mapswithme.maps.Framework class =============//
/*            ____
 *          _ |||| _
 *          \\    //
 *           \\  //
 *            \\//
 *             \/
 */

extern "C"
{
  void CallOnMapObjectActivatedListener(shared_ptr<jobject> listener, jobject mapObject)
  {
    JNIEnv * env = jni::GetEnv();
    static jmethodID const methodId =
        jni::GetJavaMethodID(env, *listener.get(), "onMapObjectActivated",
                             "(Lcom/mapswithme/maps/bookmarks/data/MapObject;)V");
    ASSERT(methodId, ());
    //public MapObject(@MapObjectType int mapObjectType, String name, double lat, double lon, String typeName)
    env->CallVoidMethod(*listener.get(), methodId, mapObject);
  }

  void CallOnDismissListener(shared_ptr<jobject> obj)
  {
    JNIEnv * env = jni::GetEnv();
    static jmethodID const methodId = jni::GetJavaMethodID(env, *obj.get(), "onDismiss", "()V");
    ASSERT(methodId, ());
    env->CallVoidMethod(*obj.get(), methodId);
  }

  void CallOnUserMarkActivated(shared_ptr<jobject> obj, unique_ptr<UserMarkCopy> markCopy)
  {
    if (markCopy == nullptr)
    {
      g_framework->SetActiveUserMark(nullptr);
      CallOnDismissListener(obj);
      return;
    }

    UserMark const * mark = markCopy->GetUserMark();
    g_framework->SetActiveUserMark(mark);
    auto mapObject = jni::ScopedLocalRef(usermark_helper::CreateMapObject(mark));
    CallOnMapObjectActivatedListener(obj, *mapObject);
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
    search::AddressInfo const info = frm()->GetMercatorAddressInfo(MercatorBounds::FromLatLon(lat, lon));
    return jni::ToJavaString(env, info.FormatNameAndAddress());
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_Framework_nativeClearApiPoints(JNIEnv * env, jclass clazz)
  {
    UserMarkControllerGuard guard(frm()->GetBookmarkManager(), UserMarkType::API_MARK);
    guard.m_controller.Clear();
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_Framework_nativeSetMapObjectListener(JNIEnv * env, jclass clazz, jobject l)
  {
    frm()->SetUserMarkActivationListener(bind(&CallOnUserMarkActivated, jni::make_global_ref(l), _1));
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_Framework_nativeRemoveMapObjectListener(JNIEnv * env, jobject thiz)
  {
    frm()->SetUserMarkActivationListener(::Framework::TActivateCallbackFn());
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

  JNIEXPORT jlong JNICALL
  Java_com_mapswithme_maps_Framework_nativeGetDataVersion(JNIEnv * env, jclass clazz)
  {
    return frm()->GetCurrentDataVersion();
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
    frm()->LoadBookmarks();
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
    frm()->CloseRouting();
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_Framework_nativeBuildRoute(JNIEnv * env, jclass thiz, jdouble startLat,
                                                      jdouble startLon,  jdouble finishLat,
                                                      jdouble finishLon)
  {
    frm()->BuildRoute(MercatorBounds::FromLatLon(startLat, startLon),
                      MercatorBounds::FromLatLon(finishLat, finishLon), 0 /* timeoutSec */);
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_Framework_nativeFollowRoute(JNIEnv * env, jclass thiz)
  {
    frm()->FollowRoute();
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_Framework_nativeDisableFollowing(JNIEnv * env, jclass thiz)
  {
    frm()->DisableFollowMode();
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
    return usermark_helper::CreateMapObject(frm()->GetAddressMark(MercatorBounds::FromLatLon(lat, lon)));
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
  Java_com_mapswithme_maps_Framework_nativeSetMapStyle(JNIEnv * env, jclass thiz, jint mapStyle)
  {
    MapStyle const val = static_cast<MapStyle>(mapStyle);
    if (val != g_framework->GetMapStyle())
      g_framework->SetMapStyle(val);
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_Framework_nativeSetRouter(JNIEnv * env, jclass thiz, jint routerType)
  {
    g_framework->SetRouter(static_cast<routing::RouterType>(routerType));
  }

  JNIEXPORT jint JNICALL
  Java_com_mapswithme_maps_Framework_nativeGetRouter(JNIEnv * env, jclass thiz)
  {
    return static_cast<jint>(g_framework->GetRouter());
  }

  JNIEXPORT jint JNICALL
  Java_com_mapswithme_maps_Framework_nativeGetLastUsedRouter(JNIEnv * env, jclass thiz)
  {
    return static_cast<jint>(g_framework->GetLastUsedRouter());
  }

  JNIEXPORT jint JNICALL
  Java_com_mapswithme_maps_Framework_nativeGetBestRouter(JNIEnv * env, jclass thiz, jdouble srcLat, jdouble srcLon, jdouble dstLat, jdouble dstLon)
  {
    return static_cast<jint>(frm()->GetBestRouter(MercatorBounds::FromLatLon(srcLat, srcLon), MercatorBounds::FromLatLon(dstLat, dstLon)));
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_Framework_nativeSetRouteStartPoint(JNIEnv * env, jclass thiz, jdouble lat, jdouble lon, jboolean valid)
  {
    frm()->SetRouteStartPoint(m2::PointD(MercatorBounds::FromLatLon(lat, lon)), static_cast<bool>(valid));
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_Framework_nativeSetRouteEndPoint(JNIEnv * env, jclass thiz, jdouble lat, jdouble lon, jboolean valid)
  {
    frm()->SetRouteFinishPoint(m2::PointD(MercatorBounds::FromLatLon(lat, lon)), static_cast<bool>(valid));
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

  JNIEXPORT jboolean JNICALL
  Java_com_mapswithme_maps_Framework_nativeIsDayTime(JNIEnv * env, jclass thiz, jlong utcTimeSeconds, jdouble lat, jdouble lon)
  {
    DayTimeType const dt = GetDayTime(static_cast<time_t>(utcTimeSeconds), lat, lon);
    return (dt == DayTimeType::Day || dt == DayTimeType::PolarDay);
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_Framework_nativeSet3dMode(JNIEnv * env, jclass thiz, jboolean allow, jboolean allowBuildings)
  {
    bool const allow3d = static_cast<bool>(allow);
    bool const allow3dBuildings = static_cast<bool>(allowBuildings);

    g_framework->Save3dMode(allow3d, allow3dBuildings);
    g_framework->PostDrapeTask([allow3d, allow3dBuildings]()
    {
      g_framework->Set3dMode(allow3d, allow3dBuildings);
    });
  }

  JNIEXPORT void JNICALL
  Java_com_mapswithme_maps_Framework_nativeGet3dMode(JNIEnv * env, jclass thiz, jobject result)
  {
    bool enabled;
    bool buildings;
    g_framework->Get3dMode(enabled, buildings);

    jclass const resultClass = env->GetObjectClass(result);

    static jfieldID const enabledField = env->GetFieldID(resultClass, "enabled", "Z");
    env->SetBooleanField(result, enabledField, enabled);

    static jfieldID const buildingsField = env->GetFieldID(resultClass, "buildings", "Z");
    env->SetBooleanField(result, buildingsField, buildings);
  }

  JNIEXPORT jobject JNICALL
  Java_com_mapswithme_maps_Framework_nativeGetActiveMapObject(JNIEnv * env, jclass thiz)
  {
    return usermark_helper::CreateMapObject(g_framework->GetActiveUserMark());
  }
} // extern "C"
