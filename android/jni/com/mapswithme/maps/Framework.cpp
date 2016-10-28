#include "Framework.hpp"
#include "UserMarkHelper.hpp"
#include "com/mapswithme/core/jni_helper.hpp"
#include "com/mapswithme/maps/bookmarks/data/BookmarkManager.hpp"
#include "com/mapswithme/opengl/androidoglcontextfactory.hpp"
#include "com/mapswithme/platform/Platform.hpp"

#include "map/chart_generator.hpp"
#include "map/user_mark.hpp"

#include "search/everywhere_search_params.hpp"

#include "storage/storage_helpers.hpp"

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

static_assert(sizeof(int) >= 4, "Size of jint in less than 4 bytes.");

namespace
{
::Framework * frm()
{
  return g_framework->NativeFramework();
}

jobject g_mapObjectListener;
}  // namespace

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
  , m_isContextDestroyed(false)
  , m_currentMode(location::PendingPosition)
  , m_isCurrentModeInitialized(false)
  , m_isChoosePositionMode(false)
{
  ASSERT_EQUAL ( g_framework, 0, () );
  g_framework = this;
}

void Framework::OnLocationError(int errorCode)
{
  m_work.OnLocationError(static_cast<location::TLocationError>(errorCode));
}

void Framework::OnLocationUpdated(location::GpsInfo const & info)
{
  m_work.OnLocationUpdate(info);

  if (!IsDrapeEngineCreated())
  {
    location::EMyPositionMode const mode = GetMyPositionMode();
    if (mode == location::PendingPosition)
      SetMyPositionMode(location::Follow);
  }
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

void Framework::MyPositionModeChanged(location::EMyPositionMode mode, bool routingActive)
{
  if (m_myPositionModeSignal)
    m_myPositionModeSignal(mode, routingActive);
}

void Framework::SetMyPositionMode(location::EMyPositionMode mode)
{
    OnMyPositionModeChanged(mode);
    settings::Set(settings::kLocationStateMode, m_currentMode);
    MyPositionModeChanged(m_currentMode, false /* routingActive, does not matter */);
}

bool Framework::CreateDrapeEngine(JNIEnv * env, jobject jSurface, int densityDpi, bool firstLaunch)
{
  m_contextFactory = make_unique_dp<dp::ThreadSafeFactory>(new AndroidOGLContextFactory(env, jSurface));
  AndroidOGLContextFactory const * factory = m_contextFactory->CastFactory<AndroidOGLContextFactory>();
  if (!factory->IsValid())
  {
    LOG(LWARNING, ("Invalid GL context."));
    return false;
  }

  ::Framework::DrapeCreationParams p;
  p.m_surfaceWidth = factory->GetWidth();
  p.m_surfaceHeight = factory->GetHeight();
  p.m_visualScale = dp::VisualScale(densityDpi);
  p.m_hasMyPositionState = m_isCurrentModeInitialized;
  p.m_initialMyPositionState = m_currentMode;
  p.m_isChoosePositionMode = m_isChoosePositionMode;
  p.m_isFirstLaunch = firstLaunch;
  ASSERT(!m_guiPositions.empty(), ("GUI elements must be set-up before engine is created"));
  p.m_widgetsInitInfo = m_guiPositions;

  m_work.LoadBookmarks();
  m_work.SetMyPositionModeListener(bind(&Framework::MyPositionModeChanged, this, _1, _2));

  m_work.CreateDrapeEngine(make_ref(m_contextFactory), move(p));
  m_work.EnterForeground();

  return true;
}

bool Framework::IsDrapeEngineCreated()
{
  return m_work.IsDrapeEngineCreated();
}

void Framework::Resize(int w, int h)
{
  m_contextFactory->CastFactory<AndroidOGLContextFactory>()->UpdateSurfaceSize();
  m_work.OnSize(w, h);

  //TODO: remove after correct visible rect calculation.
  frm()->SetVisibleViewport(m2::RectD(0, 0, w, h));
}

void Framework::DetachSurface(bool destroyContext)
{
  LOG(LINFO, ("Detach surface."));
  if (destroyContext)
  {
    LOG(LINFO, ("Destroy context."));
    m_isContextDestroyed = true;
    m_work.EnterBackground();
  }
  m_work.SetRenderingDisabled(destroyContext);

  ASSERT(m_contextFactory != nullptr, ());
  AndroidOGLContextFactory * factory = m_contextFactory->CastFactory<AndroidOGLContextFactory>();
  factory->ResetSurface();
}

bool Framework::AttachSurface(JNIEnv * env, jobject jSurface)
{
  LOG(LINFO, ("Attach surface."));

  ASSERT(m_contextFactory != nullptr, ());
  AndroidOGLContextFactory * factory = m_contextFactory->CastFactory<AndroidOGLContextFactory>();
  factory->SetSurface(env, jSurface);

  if (!factory->IsValid())
  {
    LOG(LWARNING, ("Invalid GL context."));
    return false;
  }

  ASSERT(!m_guiPositions.empty(), ("GUI elements must be set-up before engine is created"));

  m_work.SetRenderingEnabled(factory);

  if (m_isContextDestroyed)
  {
    LOG(LINFO, ("Recover GL resources, viewport size:", factory->GetWidth(), factory->GetHeight()));
    m_work.UpdateDrapeEngine(factory->GetWidth(), factory->GetHeight());
    m_isContextDestroyed = false;

    m_work.EnterForeground();
  }

  return true;
}

void Framework::SetMapStyle(MapStyle mapStyle)
{
  m_work.SetMapStyle(mapStyle);
}

void Framework::MarkMapStyle(MapStyle mapStyle)
{
  m_work.MarkMapStyle(mapStyle);
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

void Framework::SetChoosePositionMode(bool isChoosePositionMode, bool isBusiness,
                                      bool hasPosition, m2::PointD const & position)
{
  m_isChoosePositionMode = isChoosePositionMode;
  m_work.BlockTapEvents(isChoosePositionMode);
  m_work.EnableChoosePositionMode(isChoosePositionMode, isBusiness, hasPosition, position);
}

bool Framework::GetChoosePositionMode()
{
  return m_isChoosePositionMode;
}

Storage & Framework::GetStorage()
{
  return m_work.GetStorage();
}

void Framework::ShowNode(TCountryId const & idx, bool zoomToDownloadButton)
{
  if (zoomToDownloadButton)
  {
    m2::RectD const rect = CalcLimitRect(idx, m_work.GetStorage(), m_work.GetCountryInfoGetter());
    m_work.SetViewportCenter(rect.Center(), 10);
  }
  else
  {
    m_work.ShowNode(idx);
  }
}

void Framework::Touch(int action, Finger const & f1, Finger const & f2, uint8_t maskedPointer)
{
  MultiTouchAction eventType = static_cast<MultiTouchAction>(action);
  df::TouchEvent event;

  switch(eventType)
  {
  case MULTITOUCH_DOWN:
    event.SetTouchType(df::TouchEvent::TOUCH_DOWN);
    break;
  case MULTITOUCH_MOVE:
    event.SetTouchType(df::TouchEvent::TOUCH_MOVE);
    break;
  case MULTITOUCH_UP:
    event.SetTouchType(df::TouchEvent::TOUCH_UP);
    break;
  case MULTITOUCH_CANCEL:
    event.SetTouchType(df::TouchEvent::TOUCH_CANCEL);
    break;
  default:
    return;
  }

  df::Touch touch;
  touch.m_location = m2::PointD(f1.m_x, f1.m_y);
  touch.m_id = f1.m_id;
  event.SetFirstTouch(touch);

  touch.m_location = m2::PointD(f2.m_x, f2.m_y);
  touch.m_id = f2.m_id;
  event.SetSecondTouch(touch);

  event.SetFirstMaskedPointer(maskedPointer);
  m_work.TouchEvent(event);
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

void Framework::Scale(m2::PointD const & centerPt, int targetZoom, bool animate)
{
  ref_ptr<df::DrapeEngine> engine = m_work.GetDrapeEngine();
  if (engine)
    engine->SetModelViewCenter(centerPt, targetZoom, animate);
}

::Framework * Framework::NativeFramework()
{
  return &m_work;
}

bool Framework::Search(search::EverywhereSearchParams const & params)
{
  m_searchQuery = params.m_query;
  return m_work.SearchEverywhere(params);
}

void Framework::AddLocalMaps()
{
  m_work.RegisterAllMaps();
}

void Framework::RemoveLocalMaps()
{
  m_work.DeregisterAllMaps();
}

void Framework::ReplaceBookmark(BookmarkAndCategory const & ind, BookmarkData & bm)
{
  m_work.ReplaceBookmark(ind.m_categoryIndex, ind.m_bookmarkIndex, bm);
}

size_t Framework::ChangeBookmarkCategory(BookmarkAndCategory const & ind, size_t newCat)
{
  return m_work.MoveBookmark(ind.m_bookmarkIndex, ind.m_categoryIndex, newCat);
}

bool Framework::ShowMapForURL(string const & url)
{
  return m_work.ShowMapForURL(url);
}

void Framework::DeactivatePopup()
{
  m_work.DeactivateMapSelection(false);
}

string Framework::GetOutdatedCountriesString()
{
  vector<Country const *> countries;
  class Storage const & storage = GetStorage();
  storage.GetOutdatedCountries(countries);

  string res;
  NodeAttrs attrs;

  for (size_t i = 0; i < countries.size(); ++i)
  {
    storage.GetNodeAttrs(countries[i]->Name(), attrs);

    if (i > 0)
      res += ", ";

    res += attrs.m_nodeLocalName;
  }

  return res;
}

void Framework::ShowTrack(int category, int track)
{
  Track const * nTrack = NativeFramework()->GetBmCategory(category)->GetTrack(track);
  NativeFramework()->ShowTrack(*nTrack);
}

void Framework::SetMyPositionModeListener(location::TMyPositionModeChanged const & fn)
{
  m_myPositionModeSignal = fn;
}

location::EMyPositionMode Framework::GetMyPositionMode()
{
  if (!m_isCurrentModeInitialized)
  {
    m_currentMode = location::NotFollowNoPosition;
    settings::Get(settings::kLocationStateMode, m_currentMode);
    m_isCurrentModeInitialized = true;
  }

  return m_currentMode;
}

void Framework::OnMyPositionModeChanged(location::EMyPositionMode mode)
{
  m_currentMode = mode;
  m_isCurrentModeInitialized = true;
}

void Framework::SwitchMyPositionNextMode()
{
  if (IsDrapeEngineCreated())
  {
    m_work.SwitchMyPositionNextMode();
    return;
  }

  // Engine is not available, but the client requests to change mode.
  if (GetMyPositionMode() == location::NotFollowNoPosition)
    SetMyPositionMode(location::PendingPosition);
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

void Framework::SetPlacePageInfo(place_page::Info const & info)
{
  m_info = info;
}

place_page::Info & Framework::GetPlacePageInfo()
{
  return m_info;
}

void Framework::RequestBookingMinPrice(string const & hotelId, string const & currencyCode, function<void(string const &, string const &)> const & callback)
{
  return m_work.GetBookingApi().GetMinPrice(hotelId, currencyCode, callback);
}

void Framework::RequestBookingInfo(string const & hotelId, string const & lang,
                                   function<void(BookingApi::HotelInfo const &)> const & callback)
{
  return m_work.GetBookingApi().GetHotelInfo(hotelId, lang, callback);
}

bool Framework::HasSpaceForMigration()
{
  return m_work.IsEnoughSpaceForMigrate();
}

void Framework::Migrate(bool keepOldMaps)
{
  m_work.Migrate(keepOldMaps);
}

storage::TCountryId Framework::PreMigrate(ms::LatLon const & position, Storage::TChangeCountryFunction const & statusChangeListener,
                                                                       Storage::TProgressFunction const & progressListener)
{
  return m_work.PreMigrate(position, statusChangeListener, progressListener);
}

bool Framework::IsAutoRetryDownloadFailed()
{
  return m_work.GetDownloadingPolicy().IsAutoRetryDownloadFailed();
}

bool Framework::IsDownloadOn3gEnabled()
{
  return m_work.GetDownloadingPolicy().IsCellularDownloadEnabled();
}

void Framework::EnableDownloadOn3g()
{
  m_work.GetDownloadingPolicy().EnableCellularDownload(true);
}

uint64_t Framework::RequestUberProducts(ms::LatLon const & from, ms::LatLon const & to,
                                    uber::ProductsCallback const & callback)
{
  auto const errorCallback = [](uber::ErrorCode const code, uint64_t const requestId) {};
  return m_work.GetUberApi().GetAvailableProducts(from, to, callback, errorCallback);
}

uber::RideRequestLinks Framework::GetUberLinks(string const & productId, ms::LatLon const & from, ms::LatLon const & to)
{
  return uber::Api::GetRideRequestLinks(productId, from, to);
}


}  // namespace android

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
void CallRoutingListener(shared_ptr<jobject> listener, int errorCode, vector<storage::TCountryId> const & absentMaps)
{
  JNIEnv * env = jni::GetEnv();
  jmethodID const method = jni::GetMethodID(env, *listener, "onRoutingEvent", "(I[Ljava/lang/String;)V");
  ASSERT(method, ());

  env->CallVoidMethod(*listener, method, errorCode, jni::TScopedLocalObjectArrayRef(env, jni::ToJavaStringArray(env, absentMaps)).get());
}

void CallRouteProgressListener(shared_ptr<jobject> listener, float progress)
{
  JNIEnv * env = jni::GetEnv();
  jmethodID const methodId = jni::GetMethodID(env, *listener, "onRouteBuildingProgress", "(F)V");
  env->CallVoidMethod(*listener, methodId, progress);
}

/// @name JNI EXPORTS
//@{
JNIEXPORT jstring JNICALL
Java_com_mapswithme_maps_Framework_nativeGetNameAndAddress(JNIEnv * env, jclass clazz, jdouble lat, jdouble lon)
{
  search::AddressInfo const info = frm()->GetAddressInfoAtPoint(MercatorBounds::FromLatLon(lat, lon));
  return jni::ToJavaString(env, info.FormatNameAndAddress());
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_Framework_nativeClearApiPoints(JNIEnv * env, jclass clazz)
{
  UserMarkControllerGuard guard(frm()->GetBookmarkManager(), UserMarkType::API_MARK);
  guard.m_controller.Clear();
}

JNIEXPORT jint JNICALL
Java_com_mapswithme_maps_Framework_nativeParseAndSetApiUrl(JNIEnv * env, jclass clazz, jstring url)
{
  return static_cast<jint>(frm()->ParseAndSetApiURL(jni::ToNativeString(env, url)));
}

JNIEXPORT jobject JNICALL
Java_com_mapswithme_maps_Framework_nativeGetParsedRoutingData(JNIEnv * env, jclass clazz)
{
  using namespace url_scheme;
  static jclass const pointClazz = jni::GetGlobalClassRef(env, "com/mapswithme/maps/api/RoutePoint");
  // Java signature : RoutePoint(double lat, double lon, String name)
  static jmethodID const pointConstructor = jni::GetConstructorID(env, pointClazz, "(DDLjava/lang/String;)V");

  static jclass const routeDataClazz = jni::GetGlobalClassRef(env, "com/mapswithme/maps/api/ParsedRoutingData");
  // Java signature : ParsedRoutingData(RoutePoint[] points, int routerType) {
  static jmethodID const routeDataConstructor = jni::GetConstructorID(env, routeDataClazz, "([Lcom/mapswithme/maps/api/RoutePoint;I)V");

  auto const & routingData = frm()->GetParsedRoutingData();
  jobjectArray points = jni::ToJavaArray(env, pointClazz, routingData.m_points,
                                         [](JNIEnv * env, RoutePoint const & point)
                                         {
                                           jni::TScopedLocalRef const name(env, jni::ToJavaString(env, point.m_name));
                                           return env->NewObject(pointClazz, pointConstructor,
                                                                 MercatorBounds::YToLat(point.m_org.y),
                                                                 MercatorBounds::XToLon(point.m_org.x), name.get());
                                         });

  return env->NewObject(routeDataClazz, routeDataConstructor, points, routingData.m_type);
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_Framework_nativeSetMapObjectListener(JNIEnv * env, jclass clazz, jobject jListener)
{
  g_mapObjectListener = env->NewGlobalRef(jListener);
  // void onMapObjectActivated(MapObject object);
  jmethodID const activatedId = jni::GetMethodID(env, g_mapObjectListener, "onMapObjectActivated",
                                                 "(Lcom/mapswithme/maps/bookmarks/data/MapObject;)V");
  // void onDismiss(boolean switchFullScreenMode);
  jmethodID const dismissId = jni::GetMethodID(env, g_mapObjectListener, "onDismiss", "(Z)V");
  frm()->SetMapSelectionListeners([activatedId](place_page::Info const & info)
  {
    JNIEnv * env = jni::GetEnv();
    g_framework->SetPlacePageInfo(info);
    jni::TScopedLocalRef mapObject(env, usermark_helper::CreateMapObject(env, info));
    env->CallVoidMethod(g_mapObjectListener, activatedId, mapObject.get());
  }, [dismissId](bool switchFullScreenMode)
  {
    JNIEnv * env = jni::GetEnv();
    g_framework->SetPlacePageInfo({});
    env->CallVoidMethod(g_mapObjectListener, dismissId, switchFullScreenMode);
  });
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_Framework_nativeRemoveMapObjectListener(JNIEnv * env, jclass)
{
  frm()->SetMapSelectionListeners({}, {});
  env->DeleteGlobalRef(g_mapObjectListener);
}

JNIEXPORT jstring JNICALL
Java_com_mapswithme_maps_Framework_nativeGetGe0Url(JNIEnv * env, jclass, jdouble lat, jdouble lon, jdouble zoomLevel, jstring name)
{
  ::Framework * fr = frm();
  double const scale = (zoomLevel > 0 ? zoomLevel : fr->GetDrawScale());
  string const url = fr->CodeGe0url(lat, lon, scale, jni::ToNativeString(env, name));
  return jni::ToJavaString(env, url);
}

JNIEXPORT jobject JNICALL
Java_com_mapswithme_maps_Framework_nativeGetDistanceAndAzimuth(
    JNIEnv * env, jclass, jdouble merX, jdouble merY, jdouble cLat, jdouble cLon, jdouble north)
{
  string distance;
  double azimut = -1.0;
  frm()->GetDistanceAndAzimut(m2::PointD(merX, merY), cLat, cLon, north, distance, azimut);

  static jclass const daClazz = jni::GetGlobalClassRef(env, "com/mapswithme/maps/bookmarks/data/DistanceAndAzimut");
  // Java signature : DistanceAndAzimut(String distance, double azimuth)
  static jmethodID const methodID = jni::GetConstructorID(env, daClazz, "(Ljava/lang/String;D)V");

  return env->NewObject(daClazz, methodID,
                        jni::ToJavaString(env, distance.c_str()),
                        static_cast<jdouble>(azimut));
}

JNIEXPORT jobject JNICALL
Java_com_mapswithme_maps_Framework_nativeGetDistanceAndAzimuthFromLatLon(
    JNIEnv * env, jclass clazz, jdouble lat, jdouble lon, jdouble cLat, jdouble cLon, jdouble north)
{
  double const merY = MercatorBounds::LatToY(lat);
  double const merX = MercatorBounds::LonToX(lon);
  return Java_com_mapswithme_maps_Framework_nativeGetDistanceAndAzimuth(env, clazz, merX, merY, cLat, cLon, north);
}

JNIEXPORT jobject JNICALL
Java_com_mapswithme_maps_Framework_nativeFormatLatLon(JNIEnv * env, jclass, jdouble lat, jdouble lon, jboolean useDMSFormat)
{
  return jni::ToJavaString(env, (useDMSFormat ? measurement_utils::FormatLatLonAsDMS(lat, lon, 2)
                                              : measurement_utils::FormatLatLon(lat, lon, 6)));
}

JNIEXPORT jobjectArray JNICALL
Java_com_mapswithme_maps_Framework_nativeFormatLatLonToArr(JNIEnv * env, jclass, jdouble lat, jdouble lon, jboolean useDMSFormat)
{
  string slat, slon;
  if (useDMSFormat)
    measurement_utils::FormatLatLonAsDMS(lat, lon, slat, slon, 2);
  else
    measurement_utils::FormatLatLon(lat, lon, slat, slon, 6);

  static jclass const klass = jni::GetGlobalClassRef(env, "java/lang/String");
  jobjectArray arr = env->NewObjectArray(2, klass, 0);

  env->SetObjectArrayElement(arr, 0, jni::ToJavaString(env, slat));
  env->SetObjectArrayElement(arr, 1, jni::ToJavaString(env, slon));

  return arr;
}

JNIEXPORT jobject JNICALL
Java_com_mapswithme_maps_Framework_nativeFormatAltitude(JNIEnv * env, jclass, jdouble alt)
{
  return jni::ToJavaString(env, measurement_utils::FormatAltitude(alt));
}

JNIEXPORT jobject JNICALL
Java_com_mapswithme_maps_Framework_nativeFormatSpeed(JNIEnv * env, jclass, jdouble speed)
{
  return jni::ToJavaString(env, measurement_utils::FormatSpeedWithDeviceUnits(speed));
}

JNIEXPORT jobject JNICALL
Java_com_mapswithme_maps_Framework_nativeGetOutdatedCountriesString(JNIEnv * env, jclass)
{
  return jni::ToJavaString(env, g_framework->GetOutdatedCountriesString());
}

JNIEXPORT jboolean JNICALL
Java_com_mapswithme_maps_Framework_nativeIsDataVersionChanged(JNIEnv * env, jclass)
{
  return frm()->IsDataVersionUpdated() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_Framework_nativeUpdateSavedDataVersion(JNIEnv * env, jclass)
{
  frm()->UpdateSavedDataVersion();
}

JNIEXPORT jlong JNICALL
Java_com_mapswithme_maps_Framework_nativeGetDataVersion(JNIEnv * env, jclass)
{
  return frm()->GetCurrentDataVersion();
}

JNIEXPORT jint JNICALL
Java_com_mapswithme_maps_Framework_nativeGetDrawScale(JNIEnv * env, jclass)
{
  return static_cast<jint>(frm()->GetDrawScale());
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_Framework_nativeUpdateUserViewportChanged(JNIEnv * env, jclass)
{
  frm()->UpdateUserViewportChanged();
}

JNIEXPORT jdoubleArray JNICALL
Java_com_mapswithme_maps_Framework_nativeGetScreenRectCenter(JNIEnv * env, jclass)
{
  m2::PointD const center = frm()->GetViewportCenter();

  double latlon[] = {MercatorBounds::YToLat(center.y), MercatorBounds::XToLon(center.x)};
  jdoubleArray jLatLon = env->NewDoubleArray(2);
  env->SetDoubleArrayRegion(jLatLon, 0, 2, latlon);

  return jLatLon;
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_Framework_nativeShowTrackRect(JNIEnv * env, jclass, jint cat, jint track)
{
  g_framework->ShowTrack(cat, track);
}

JNIEXPORT jstring JNICALL
Java_com_mapswithme_maps_Framework_nativeGetBookmarkDir(JNIEnv * env, jclass)
{
  return jni::ToJavaString(env, GetPlatform().SettingsDir().c_str());
}

JNIEXPORT jstring JNICALL
Java_com_mapswithme_maps_Framework_nativeGetWritableDir(JNIEnv * env, jclass)
{
  return jni::ToJavaString(env, GetPlatform().WritableDir().c_str());
}

JNIEXPORT jstring JNICALL
Java_com_mapswithme_maps_Framework_nativeGetSettingsDir(JNIEnv * env, jclass)
{
  return jni::ToJavaString(env, GetPlatform().SettingsDir().c_str());
}

JNIEXPORT jobjectArray JNICALL
Java_com_mapswithme_maps_Framework_nativeGetMovableFilesExts(JNIEnv * env, jclass)
{
  vector<string> exts = { DATA_FILE_EXTENSION, FONT_FILE_EXTENSION, ROUTING_FILE_EXTENSION };
  platform::CountryIndexes::GetIndexesExts(exts);
  return jni::ToJavaStringArray(env, exts);
}

JNIEXPORT jstring JNICALL
Java_com_mapswithme_maps_Framework_nativeGetBookmarksExt(JNIEnv * env, jclass)
{
  return jni::ToJavaString(env, BOOKMARKS_FILE_EXTENSION);
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_Framework_nativeSetWritableDir(JNIEnv * env, jclass, jstring jNewPath)
{
  string newPath = jni::ToNativeString(env, jNewPath);
  g_framework->RemoveLocalMaps();
  android::Platform::Instance().SetWritableDir(newPath);
  g_framework->AddLocalMaps();
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_Framework_nativeLoadBookmarks(JNIEnv * env, jclass)
{
  frm()->LoadBookmarks();
}

JNIEXPORT jboolean JNICALL
Java_com_mapswithme_maps_Framework_nativeIsRoutingActive(JNIEnv * env, jclass)
{
  return frm()->IsRoutingActive();
}

JNIEXPORT jboolean JNICALL
Java_com_mapswithme_maps_Framework_nativeIsRouteBuilding(JNIEnv * env, jclass)
{
  return frm()->IsRouteBuilding();
}

JNIEXPORT jboolean JNICALL
Java_com_mapswithme_maps_Framework_nativeIsRouteBuilt(JNIEnv * env, jclass)
{
  return frm()->IsRouteBuilt();
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_Framework_nativeCloseRouting(JNIEnv * env, jclass)
{
  frm()->CloseRouting();
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_Framework_nativeBuildRoute(JNIEnv * env, jclass,
                                                    jdouble startLat,  jdouble startLon,
                                                    jdouble finishLat, jdouble finishLon)
{
  frm()->BuildRoute(MercatorBounds::FromLatLon(startLat, startLon),
                    MercatorBounds::FromLatLon(finishLat, finishLon), 0 /* timeoutSec */);

}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_Framework_nativeFollowRoute(JNIEnv * env, jclass)
{
  frm()->FollowRoute();
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_Framework_nativeDisableFollowing(JNIEnv * env, jclass)
{
  frm()->DisableFollowMode();
}

JNIEXPORT jobjectArray JNICALL
Java_com_mapswithme_maps_Framework_nativeGenerateTurnNotifications(JNIEnv * env, jclass)
{
  ::Framework * fr = frm();
  if (!fr->IsRoutingActive())
    return nullptr;

  vector<string> turnNotifications;
  fr->GenerateTurnNotifications(turnNotifications);
  if (turnNotifications.empty())
    return nullptr;

  return jni::ToJavaStringArray(env, turnNotifications);
}

JNIEXPORT jobject JNICALL
Java_com_mapswithme_maps_Framework_nativeGetRouteFollowingInfo(JNIEnv * env, jclass)
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
  static jmethodID const ctorRouteInfoID = jni::GetConstructorID(env, klass,
                                               "(Ljava/lang/String;Ljava/lang/String;"
                                               "Ljava/lang/String;Ljava/lang/String;"
                                               "Ljava/lang/String;Ljava/lang/String;DIIIDDII"
                                               "[Lcom/mapswithme/maps/routing/SingleLaneInfo;)V");

  vector<location::FollowingInfo::SingleLaneInfoClient> const & lanes = info.m_lanes;
  jobjectArray jLanes = nullptr;
  if (!lanes.empty())
  {
    static jclass const laneClass = jni::GetGlobalClassRef(env, "com/mapswithme/maps/routing/SingleLaneInfo");
    size_t const lanesSize = lanes.size();
    jLanes = env->NewObjectArray(lanesSize, laneClass, nullptr);
    ASSERT(jLanes, (jni::DescribeException()));
    static jmethodID const ctorSingleLaneInfoID = jni::GetConstructorID(env, laneClass, "([BZ)V");

    for (size_t j = 0; j < lanesSize; ++j)
    {
      size_t const laneSize = lanes[j].m_lane.size();
      jni::TScopedLocalByteArrayRef singleLane(env, env->NewByteArray(laneSize));
      ASSERT(singleLane.get(), (jni::DescribeException()));
      env->SetByteArrayRegion(singleLane.get(), 0, laneSize, lanes[j].m_lane.data());

      jni::TScopedLocalRef singleLaneInfo(env, env->NewObject(laneClass, ctorSingleLaneInfoID, singleLane.get(), lanes[j].m_isRecommended));
      ASSERT(singleLaneInfo.get(), (jni::DescribeException()));
      env->SetObjectArrayElement(jLanes, j, singleLaneInfo.get());
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

JNIEXPORT jintArray JNICALL
Java_com_mapswithme_maps_Framework_nativeGenerateRouteAltitudeChartBits(JNIEnv * env, jclass, jint width, jint height, jobject routeAltitudeLimits)
{
  ::Framework * fr = frm();
  ASSERT(fr, ());

  vector<uint8_t> imageRGBAData;
  int32_t minRouteAltitude = 0;
  int32_t maxRouteAltitude = 0;
  measurement_utils::Units units = measurement_utils::Units::Metric;
  if (!fr->GenerateRouteAltitudeChart(width, height, imageRGBAData, minRouteAltitude, maxRouteAltitude, units))
  {
    LOG(LWARNING, ("Can't generate route altitude image."));
    return nullptr;
  }

  // Passing route limits.
  jclass const routeAltitudeLimitsClass = env->GetObjectClass(routeAltitudeLimits);
  ASSERT(routeAltitudeLimitsClass, ());

  static jfieldID const minRouteAltitudeField = env->GetFieldID(routeAltitudeLimitsClass, "minRouteAltitude", "I");
  ASSERT(minRouteAltitudeField, ());
  env->SetIntField(routeAltitudeLimits, minRouteAltitudeField, minRouteAltitude);

  static jfieldID const maxRouteAltitudeField = env->GetFieldID(routeAltitudeLimitsClass, "maxRouteAltitude", "I");
  ASSERT(maxRouteAltitudeField, ());
  env->SetIntField(routeAltitudeLimits, maxRouteAltitudeField, maxRouteAltitude);

  static jfieldID const isMetricUnitsField = env->GetFieldID(routeAltitudeLimitsClass, "isMetricUnits", "Z");
  ASSERT(isMetricUnitsField, ());
  env->SetBooleanField(routeAltitudeLimits, isMetricUnitsField, units == measurement_utils::Units::Metric);

  size_t const imageRGBADataSize = imageRGBAData.size();
  ASSERT_NOT_EQUAL(imageRGBADataSize, 0, ("GenerateRouteAltitudeChart returns true but the vector with altitude image bits is empty."));

  size_t const pxlCount = width * height;
  if (maps::kAltitudeChartBPP * pxlCount != imageRGBADataSize)
  {
    LOG(LWARNING, ("Wrong size of vector with altitude image bits. Expected size:", pxlCount, ". Real size:", imageRGBADataSize));
    return nullptr;
  }

  jintArray imageRGBADataArray = env->NewIntArray(pxlCount);
  ASSERT(imageRGBADataArray, ());
  jint * arrayElements = env->GetIntArrayElements(imageRGBADataArray, 0);
  ASSERT(arrayElements, ());

  for (size_t i = 0; i < pxlCount; ++i)
  {
    size_t const shiftInBytes = i * maps::kAltitudeChartBPP;
    // Type of |imageRGBAData| elements is uint8_t. But uint8_t is promoted to unsinged int in code below before shifting.
    // So there's no data lost in code below.
    arrayElements[i] = (imageRGBAData[shiftInBytes + 3] << 24) /* alpha */
        | (imageRGBAData[shiftInBytes] << 16) /* red */
        | (imageRGBAData[shiftInBytes + 1] << 8) /* green */
        | (imageRGBAData[shiftInBytes + 2]); /* blue */
  }
  env->ReleaseIntArrayElements(imageRGBADataArray, arrayElements, 0);

  return imageRGBADataArray;
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_Framework_nativeShowCountry(JNIEnv * env, jclass, jstring countryId, jboolean zoomToDownloadButton)
{
  g_framework->ShowNode(jni::ToNativeString(env, countryId), (bool) zoomToDownloadButton);
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_Framework_nativeSetRoutingListener(JNIEnv * env, jclass, jobject listener)
{
  CHECK(g_framework, ("Framework isn't created yet!"));
  frm()->SetRouteBuildingListener(bind(&CallRoutingListener, jni::make_global_ref(listener), _1, _2));
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_Framework_nativeSetRouteProgressListener(JNIEnv * env, jclass, jobject listener)
{
  CHECK(g_framework, ("Framework isn't created yet!"));
  frm()->SetRouteProgressListener(bind(&CallRouteProgressListener, jni::make_global_ref(listener), _1));
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_Framework_nativeDeactivatePopup(JNIEnv * env, jclass)
{
  return g_framework->DeactivatePopup();
}

JNIEXPORT jdoubleArray JNICALL
Java_com_mapswithme_maps_Framework_nativePredictLocation(JNIEnv * env, jclass, jdouble lat, jdouble lon, jdouble accuracy,
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
Java_com_mapswithme_maps_Framework_nativeSetMapStyle(JNIEnv * env, jclass, jint mapStyle)
{
  MapStyle const val = static_cast<MapStyle>(mapStyle);
  if (val != g_framework->GetMapStyle())
    g_framework->SetMapStyle(val);
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_Framework_nativeMarkMapStyle(JNIEnv * env, jclass, jint mapStyle)
{
  MapStyle const val = static_cast<MapStyle>(mapStyle);
  if (val != g_framework->GetMapStyle())
    g_framework->MarkMapStyle(val);
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_Framework_nativeSetRouter(JNIEnv * env, jclass, jint routerType)
{
  g_framework->SetRouter(static_cast<routing::RouterType>(routerType));
}

JNIEXPORT jint JNICALL
Java_com_mapswithme_maps_Framework_nativeGetRouter(JNIEnv * env, jclass)
{
  return static_cast<jint>(g_framework->GetRouter());
}

JNIEXPORT jint JNICALL
Java_com_mapswithme_maps_Framework_nativeGetLastUsedRouter(JNIEnv * env, jclass)
{
  return static_cast<jint>(g_framework->GetLastUsedRouter());
}

JNIEXPORT jint JNICALL
Java_com_mapswithme_maps_Framework_nativeGetBestRouter(JNIEnv * env, jclass, jdouble srcLat, jdouble srcLon, jdouble dstLat, jdouble dstLon)
{
  return static_cast<jint>(frm()->GetBestRouter(MercatorBounds::FromLatLon(srcLat, srcLon), MercatorBounds::FromLatLon(dstLat, dstLon)));
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_Framework_nativeSetRouteStartPoint(JNIEnv * env, jclass, jdouble lat, jdouble lon, jboolean valid)
{
  frm()->SetRouteStartPoint(m2::PointD(MercatorBounds::FromLatLon(lat, lon)), static_cast<bool>(valid));
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_Framework_nativeSetRouteEndPoint(JNIEnv * env, jclass, jdouble lat, jdouble lon, jboolean valid)
{
  frm()->SetRouteFinishPoint(m2::PointD(MercatorBounds::FromLatLon(lat, lon)), static_cast<bool>(valid));
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_Framework_nativeRegisterMaps(JNIEnv * env, jclass)
{
  frm()->RegisterAllMaps();
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_Framework_nativeDeregisterMaps(JNIEnv * env, jclass)
{
  frm()->DeregisterAllMaps();
}

JNIEXPORT jboolean JNICALL
Java_com_mapswithme_maps_Framework_nativeIsDayTime(JNIEnv * env, jclass, jlong utcTimeSeconds, jdouble lat, jdouble lon)
{
  DayTimeType const dt = GetDayTime(static_cast<time_t>(utcTimeSeconds), lat, lon);
  return (dt == DayTimeType::Day || dt == DayTimeType::PolarDay);
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_Framework_nativeSet3dMode(JNIEnv * env, jclass, jboolean allow, jboolean allowBuildings)
{
  bool const allow3d = static_cast<bool>(allow);
  bool const allow3dBuildings = static_cast<bool>(allowBuildings);

  g_framework->Save3dMode(allow3d, allow3dBuildings);
  g_framework->Set3dMode(allow3d, allow3dBuildings);
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_Framework_nativeGet3dMode(JNIEnv * env, jclass, jobject result)
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

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_Framework_nativeSetAutoZoomEnabled(JNIEnv * env, jclass, jboolean enabled)
{
  bool const autoZoomEnabled = static_cast<bool>(enabled);
  frm()->SaveAutoZoom(autoZoomEnabled);
  frm()->AllowAutoZoom(autoZoomEnabled);
}

JNIEXPORT jboolean JNICALL
Java_com_mapswithme_maps_Framework_nativeGetAutoZoomEnabled(JNIEnv * env, jclass)
{
  return frm()->LoadAutoZoom();
}

// static void nativeZoomToPoint(double lat, double lon, int zoom, boolean animate);
JNIEXPORT void JNICALL
Java_com_mapswithme_maps_Framework_nativeZoomToPoint(JNIEnv * env, jclass, jdouble lat, jdouble lon, jint zoom, jboolean animate)
{
  g_framework->Scale(m2::PointD(MercatorBounds::FromLatLon(lat, lon)), zoom, animate);
}

JNIEXPORT jobject JNICALL
Java_com_mapswithme_maps_Framework_nativeDeleteBookmarkFromMapObject(JNIEnv * env, jclass)
{
  place_page::Info & info = g_framework->GetPlacePageInfo();
  bookmarks_helper::RemoveBookmark(info.m_bac.m_categoryIndex, info.m_bac.m_bookmarkIndex);
  info.m_bac = {};
  return usermark_helper::CreateMapObject(env, info);
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_Framework_nativeOnBookmarkCategoryChanged(JNIEnv * env, jclass, jint cat, jint bmk)
{
  place_page::Info & info = g_framework->GetPlacePageInfo();
  info.m_bac.m_categoryIndex = cat;
  info.m_bac.m_bookmarkIndex = bmk;
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_Framework_nativeTurnOnChoosePositionMode(JNIEnv *, jclass, jboolean isBusiness, jboolean applyPosition)
{
  g_framework->SetChoosePositionMode(true, isBusiness, applyPosition, applyPosition ? g_framework->GetPlacePageInfo().GetMercator() : m2::PointD());
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_Framework_nativeTurnOffChoosePositionMode(JNIEnv *, jclass)
{
  g_framework->SetChoosePositionMode(false, false, false, m2::PointD());
}

JNIEXPORT jboolean JNICALL
Java_com_mapswithme_maps_Framework_nativeIsInChoosePositionMode(JNIEnv *, jclass)
{
  return g_framework->GetChoosePositionMode();
}

JNIEXPORT jboolean JNICALL
Java_com_mapswithme_maps_Framework_nativeIsDownloadedMapAtScreenCenter(JNIEnv *, jclass)
{
  ::Framework * fr = frm();
  return storage::IsPointCoveredByDownloadedMaps(fr->GetViewportCenter(), fr->GetStorage(), fr->GetCountryInfoGetter());
}

JNIEXPORT jstring JNICALL
Java_com_mapswithme_maps_Framework_nativeGetActiveObjectFormattedCuisine(JNIEnv * env, jclass)
{
  return jni::ToJavaString(env, g_framework->GetPlacePageInfo().FormatCuisines());
}

JNIEXPORT void JNICALL
Java_com_mapswithme_maps_Framework_nativeSetVisibleRect(JNIEnv * env, jclass, jint left, jint top, jint right, jint bottom)
{
  frm()->SetVisibleViewport(m2::RectD(left, top, right, bottom));
}
}  // extern "C"
