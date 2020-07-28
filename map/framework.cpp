#include "map/framework.hpp"
#include "map/benchmark_tools.hpp"
#include "map/booking_utils.hpp"
#include "map/catalog_headers_provider.hpp"
#include "map/chart_generator.hpp"
#include "map/displayed_categories_modifiers.hpp"
#include "map/download_on_map_ads_delegate.hpp"
#include "map/everywhere_search_params.hpp"
#include "map/gps_tracker.hpp"
#include "map/guides_on_map_delegate.hpp"
#include "map/notifications/notification_manager_delegate.hpp"
#include "map/notifications/notification_queue.hpp"
#include "map/promo_catalog_poi_checker.hpp"
#include "map/promo_delegate.hpp"
#include "map/taxi_delegate.hpp"
#include "map/tips_api_delegate.hpp"
#include "map/user_mark.hpp"
#include "map/utils.hpp"
#include "map/viewport_search_params.hpp"

#include "ge0/parser.hpp"
#include "ge0/url_generator.hpp"

#include "generator/borders.hpp"

#include "routing/city_roads.hpp"
#include "routing/index_router.hpp"
#include "routing/online_absent_fetcher.hpp"
#include "routing/route.hpp"
#include "routing/routing_helpers.hpp"

#include "routing_common/num_mwm_id.hpp"

#include "search/cities_boundaries_table.hpp"
#include "search/downloader_search_callback.hpp"
#include "search/editor_delegate.hpp"
#include "search/engine.hpp"
#include "search/geometry_utils.hpp"
#include "search/intermediate_result.hpp"
#include "search/locality_finder.hpp"

#include "storage/country_info_getter.hpp"
#include "storage/downloader_search_params.hpp"
#include "storage/routing_helpers.hpp"
#include "storage/storage_helpers.hpp"

#include "drape_frontend/color_constants.hpp"
#include "drape_frontend/gps_track_point.hpp"
#include "drape_frontend/route_renderer.hpp"
#include "drape_frontend/visual_params.hpp"

#include "drape/constants.hpp"

#include "editor/editable_data_source.hpp"

#include "descriptions/loader.hpp"

#include "indexer/categories_holder.hpp"
#include "indexer/classificator.hpp"
#include "indexer/classificator_loader.hpp"
#include "indexer/drawing_rules.hpp"
#include "indexer/editable_map_object.hpp"
#include "indexer/feature.hpp"
#include "indexer/feature_algo.hpp"
#include "indexer/feature_source.hpp"
#include "indexer/feature_utils.hpp"
#include "indexer/feature_visibility.hpp"
#include "indexer/ftypes_sponsored.hpp"
#include "indexer/map_style_reader.hpp"
#include "indexer/scales.hpp"
#include "indexer/transliteration_loader.hpp"

#include "metrics/eye.hpp"

#include "platform/local_country_file_utils.hpp"
#include "platform/localization.hpp"
#include "platform/measurement_utils.hpp"
#include "platform/mwm_traits.hpp"
#include "platform/mwm_version.hpp"
#include "platform/network_policy.hpp"
#include "platform/platform.hpp"
#include "platform/preferred_languages.hpp"
#include "platform/settings.hpp"
#include "platform/socket.hpp"

#include "coding/endianness.hpp"
#include "coding/point_coding.hpp"
#include "coding/string_utf8_multilang.hpp"
#include "coding/transliteration.hpp"
#include "coding/url.hpp"
#include "coding/zip_reader.hpp"

#include "geometry/angles.hpp"
#include "geometry/any_rect2d.hpp"
#include "geometry/distance_on_sphere.hpp"
#include "geometry/latlon.hpp"
#include "geometry/mercator.hpp"
#include "geometry/rect2d.hpp"
#include "geometry/tree4d.hpp"
#include "geometry/triangle2d.hpp"

#include "partners_api/ads/ads_engine.hpp"
#include "partners_api/opentable_api.hpp"
#include "partners_api/partners.hpp"

#include "base/file_name_utils.hpp"
#include "base/logging.hpp"
#include "base/math.hpp"
#include "base/scope_guard.hpp"
#include "base/stl_helpers.hpp"
#include "base/string_utils.hpp"
#include "base/timer.hpp"

#include <algorithm>
#include <sstream>

#include "defines.hpp"
#include "private.h"

#include "3party/Alohalytics/src/alohalytics.h"

using namespace location;
using namespace notifications;
using namespace routing;
using namespace storage;
using namespace std::chrono;
using namespace std::placeholders;
using namespace std;

using platform::CountryFile;
using platform::LocalCountryFile;

#ifdef FIXED_LOCATION
Framework::FixedPosition::FixedPosition()
{
  m_fixedLatLon = Settings::Get("FixPosition", m_latlon);
  m_fixedDir = Settings::Get("FixDirection", m_dirFromNorth);
}
#endif

namespace
{
char const kMapStyleKey[] = "MapStyleKeyV1";
char const kAllow3dKey[] = "Allow3d";
char const kAllow3dBuildingsKey[] = "Buildings3d";
char const kAllowAutoZoom[] = "AutoZoom";
char const kTrafficEnabledKey[] = "TrafficEnabled";
char const kTransitSchemeEnabledKey[] = "TransitSchemeEnabled";
char const kIsolinesEnabledKey[] = "IsolinesEnabled";
char const kGuidesEnabledKey[] = "GuidesEnabledKey";
char const kTrafficSimplifiedColorsKey[] = "TrafficSimplifiedColors";
char const kLargeFontsSize[] = "LargeFontsSize";
char const kTranslitMode[] = "TransliterationMode";
char const kPreferredGraphicsAPI[] = "PreferredGraphicsAPI";
char const kShowDebugInfo[] = "DebugInfo";

auto constexpr kLargeFontsScaleFactor = 1.6;
auto constexpr kGuidesEnabledInBackgroundMaxHours = 8;
size_t constexpr kMaxTrafficCacheSizeBytes = 64 /* Mb */ * 1024 * 1024;

// TODO!
// To adjust GpsTrackFilter was added secret command "?gpstrackaccuracy:xxx;"
// where xxx is a new value for horizontal accuracy.
// This is temporary solution while we don't have a good filter.
bool ParseSetGpsTrackMinAccuracyCommand(string const & query)
{
  const char kGpsAccuracy[] = "?gpstrackaccuracy:";
  if (!strings::StartsWith(query, kGpsAccuracy))
    return false;

  size_t const end = query.find(';', sizeof(kGpsAccuracy) - 1);
  if (end == string::npos)
    return false;

  string s(query.begin() + sizeof(kGpsAccuracy) - 1, query.begin() + end);
  double value;
  if (!strings::to_double(s, value))
    return false;

  GpsTrackFilter::StoreMinHorizontalAccuracy(value);
  return true;
}

string MakeSearchBookingUrl(booking::Api const & bookingApi, search::CityFinder & cityFinder,
                            FeatureType & ft)
{
  string name;
  auto const & info = ft.GetID().m_mwmId.GetInfo();
  ASSERT(info, ());

  int8_t lang = feature::GetNameForSearchOnBooking(info->GetRegionData(), ft.GetNames(), name);

  if (lang == StringUtf8Multilang::kUnsupportedLanguageCode)
    return {};

  string city = cityFinder.GetCityName(feature::GetCenter(ft), lang);

  return bookingApi.GetSearchUrl(city, name);
}

void OnRouteStartBuild(DataSource const & dataSource,
                       std::vector<RouteMarkData> const & routePoints, m2::PointD const & userPos)
{
  using eye::MapObject;

  if (routePoints.size() < 2)
    return;

  for (auto const & pt : routePoints)
  {
    if (pt.m_isMyPosition || pt.m_pointType == RouteMarkType::Start)
      continue;

    m2::RectD rect = mercator::RectByCenterXYAndOffset(pt.m_position, kMwmPointAccuracy);
    bool found = false;
    dataSource.ForEachInRect([&userPos, &pt, &found](FeatureType & ft)
    {
      if (found || !feature::GetCenter(ft).EqualDxDy(pt.m_position, kMwmPointAccuracy))
        return;
      auto const & editor = osm::Editor::Instance();
      auto const mapObject = utils::MakeEyeMapObject(ft, editor);
      if (!mapObject.IsEmpty())
      {
        eye::Eye::Event::MapObjectEvent(mapObject, MapObject::Event::Type::RouteToCreated, userPos);
        found = true;
      }
    },
    rect, scales::GetUpperScale());
  }
}
}  // namespace

pair<MwmSet::MwmId, MwmSet::RegResult> Framework::RegisterMap(
    LocalCountryFile const & localFile)
{
  LOG(LINFO, ("Loading map:", localFile.GetCountryName()));
  return m_featuresFetcher.RegisterMap(localFile);
}

void Framework::OnLocationError(TLocationError /*error*/)
{
  m_trafficManager.UpdateMyPosition(TrafficManager::MyPosition());
  if (m_drapeEngine != nullptr)
    m_drapeEngine->LoseLocation();
}

void Framework::OnLocationUpdate(GpsInfo const & info)
{
#ifdef FIXED_LOCATION
  GpsInfo rInfo(info);

  // get fixed coordinates
  m_fixedPos.GetLon(rInfo.m_longitude);
  m_fixedPos.GetLat(rInfo.m_latitude);

  // pretend like GPS position
  rInfo.m_horizontalAccuracy = 5.0;

  if (m_fixedPos.HasNorth())
  {
    // pass compass value (for devices without compass)
    CompassInfo compass;
    m_fixedPos.GetNorth(compass.m_bearing);
    OnCompassUpdate(compass);
  }

#else
  GpsInfo rInfo(info);
#endif

  m_routingManager.OnLocationUpdate(rInfo);
  m_localAdsManager.OnLocationUpdate(rInfo, GetDrawScale());
}

void Framework::OnCompassUpdate(CompassInfo const & info)
{
#ifdef FIXED_LOCATION
  CompassInfo rInfo(info);
  m_fixedPos.GetNorth(rInfo.m_bearing);
#else
  CompassInfo const & rInfo = info;
#endif

  if (m_drapeEngine != nullptr)
    m_drapeEngine->SetCompassInfo(rInfo);
}

void Framework::SwitchMyPositionNextMode()
{
  if (m_drapeEngine != nullptr)
    m_drapeEngine->SwitchMyPositionNextMode();
}

void Framework::SetMyPositionModeListener(TMyPositionModeChanged && fn)
{
  m_myPositionListener = move(fn);
}

void Framework::SetMyPositionPendingTimeoutListener(df::DrapeEngine::UserPositionPendingTimeoutHandler && fn)
{
  m_myPositionPendingTimeoutListener = move(fn);
}

TrafficManager & Framework::GetTrafficManager()
{
  return m_trafficManager;
}

LocalAdsManager & Framework::GetLocalAdsManager()
{
  return m_localAdsManager;
}

TransitReadManager & Framework::GetTransitManager()
{
  return m_transitManager;
}

IsolinesManager & Framework::GetIsolinesManager()
{
  return m_isolinesManager;
}

IsolinesManager const & Framework::GetIsolinesManager() const
{
  return m_isolinesManager;
}

GuidesManager & Framework::GetGuidesManager()
{
  return m_guidesManager;
}

void Framework::OnUserPositionChanged(m2::PointD const & position, bool hasPosition)
{
  GetBookmarkManager().MyPositionMark().SetUserPosition(position, hasPosition);
  if (m_currentPlacePageInfo && m_currentPlacePageInfo->GetTrackId() != kml::kInvalidTrackId)
    GetBookmarkManager().UpdateElevationMyPosition(m_currentPlacePageInfo->GetTrackId());

  m_routingManager.SetUserCurrentPosition(position);
  m_trafficManager.UpdateMyPosition(TrafficManager::MyPosition(position));
}

void Framework::OnViewportChanged(ScreenBase const & screen)
{
  // Drape engine may spuriously call OnViewportChanged. Filter out the calls that
  // change the viewport from the drape engine's point of view but leave it almost
  // the same from the point of view of the framework and all its subsystems such as search api.
  // Additional filtering may be done by each subsystem.
  auto const isSameViewport = m2::IsEqual(screen.ClipRect(), m_currentModelView.ClipRect(),
                                          kMwmPointAccuracy, kMwmPointAccuracy);
  if (isSameViewport)
    return;

  m_currentModelView = screen;

  GetSearchAPI().OnViewportChanged(GetCurrentViewport());

  GetBookmarkManager().UpdateViewport(m_currentModelView);
  m_trafficManager.UpdateViewport(m_currentModelView);
  m_localAdsManager.UpdateViewport(m_currentModelView);
  m_transitManager.UpdateViewport(m_currentModelView);
  m_isolinesManager.UpdateViewport(m_currentModelView);
  m_guidesManager.UpdateViewport(m_currentModelView);

  if (m_viewportChangedFn != nullptr)
    m_viewportChangedFn(screen);
}

Framework::Framework(FrameworkParams const & params)
  : m_localAdsManager(bind(&Framework::GetMwmsByRect, this, _1, true /* rough */),
                      bind(&Framework::GetMwmIdByName, this, _1),
                      bind(&Framework::ReadFeatures, this, _1, _2),
                      bind(&Framework::GetMapObjectByID, this, _1))
  , m_enabledDiffs(params.m_enableDiffs)
  , m_isRenderingEnabled(true)
  , m_transitManager(m_featuresFetcher.GetDataSource(),
                     [this](FeatureCallback const & fn, vector<FeatureID> const & features) {
                       return m_featuresFetcher.ReadFeatures(fn, features);
                     },
                     bind(&Framework::GetMwmsByRect, this, _1, false /* rough */))
  , m_isolinesManager(m_featuresFetcher.GetDataSource(),
                      bind(&Framework::GetMwmsByRect, this, _1, false /* rough */))
  , m_guidesManager([this]()
                    {
                      if (m_currentPlacePageInfo && m_currentPlacePageInfo->IsGuide())
                        DeactivateMapSelection(true /* notifyUI */);
                    })
  , m_routingManager(
        RoutingManager::Callbacks(
            [this]() -> DataSource & { return m_featuresFetcher.GetDataSource(); },
            [this]() -> storage::CountryInfoGetter const & { return GetCountryInfoGetter(); },
            [this](string const & id) -> string { return m_storage.GetParentIdFor(id); },
            [this]() -> StringsBundle const & { return m_stringsBundle; },
            [this]() -> power_management::PowerManager const & { return m_powerManager; }),
        static_cast<RoutingManager::Delegate &>(*this))
  , m_trafficManager(bind(&Framework::GetMwmsByRect, this, _1, false /* rough */),
                     kMaxTrafficCacheSizeBytes, m_routingManager.RoutingSession())
  , m_bookingFilterProcessor(m_featuresFetcher.GetDataSource(), *m_bookingApi)
  , m_displacementModeManager([this](bool show) {
    int const mode = show ? dp::displacement::kHotelMode : dp::displacement::kDefaultMode;
    if (m_drapeEngine != nullptr)
      m_drapeEngine->SetDisplacementMode(mode);
  })
  , m_lastReportedCountry(kInvalidCountryId)
  , m_popularityLoader(m_featuresFetcher.GetDataSource(), POPULARITY_RANKS_FILE_TAG)
  , m_descriptionsLoader(std::make_unique<descriptions::Loader>(m_featuresFetcher.GetDataSource()))
  , m_purchase(std::make_unique<Purchase>([this] { m_user.ResetAccessToken(); }))
  , m_tipsApi(std::make_unique<TipsApiDelegate>(*this))
{
  CHECK(IsLittleEndian(), ("Only little-endian architectures are supported."));

  // Editor should be initialized from the main thread to set its ThreadChecker.
  // However, search calls editor upon initialization thus setting the lazy editor's ThreadChecker
  // to a wrong thread. So editor should be initialiazed before serach.
  osm::Editor & editor = osm::Editor::Instance();

  // Restore map style before classificator loading
  MapStyle mapStyle = kDefaultMapStyle;
  string mapStyleStr;
  if (settings::Get(kMapStyleKey, mapStyleStr))
    mapStyle = MapStyleFromSettings(mapStyleStr);
  GetStyleReader().SetCurrentStyle(mapStyle);
  df::LoadTransitColors();

  m_connectToGpsTrack = GpsTracker::Instance().IsEnabled();
  
  // Init strings bundle.
  // @TODO. There are hardcoded strings below which are defined in strings.txt as well.
  // It's better to use strings from strings.txt instead of hardcoding them here.
  m_stringsBundle.SetDefaultString("core_entrance", "Entrance");
  m_stringsBundle.SetDefaultString("core_exit", "Exit");
  m_stringsBundle.SetDefaultString("core_placepage_unknown_place", "Unknown Place");
  m_stringsBundle.SetDefaultString("core_my_places", "My Places");
  m_stringsBundle.SetDefaultString("core_my_position", "My Position");
  m_stringsBundle.SetDefaultString("postal_code", "Postal Code");
  // Wi-Fi string is used in categories that's why does not have core_ prefix
  m_stringsBundle.SetDefaultString("wifi", "WiFi");

  m_featuresFetcher.InitClassificator();
  m_featuresFetcher.SetOnMapDeregisteredCallback(bind(&Framework::OnMapDeregistered, this, _1));
  LOG(LDEBUG, ("Classificator initialized"));

  m_displayedCategories = make_unique<search::DisplayedCategories>(GetDefaultCategories());

  // To avoid possible races - init country info getter in constructor.
  InitCountryInfoGetter();
  LOG(LDEBUG, ("Country info getter initialized"));

  InitUGC();
  LOG(LDEBUG, ("UGC initialized"));

  InitSearchAPI(params.m_numSearchAPIThreads);
  LOG(LDEBUG, ("Search API initialized"));

  auto const catalogHeadersProvider = make_shared<CatalogHeadersProvider>(*this, m_storage);

  m_bmManager = make_unique<BookmarkManager>(m_user, BookmarkManager::Callbacks(
      [this]() -> StringsBundle const & { return m_stringsBundle; },
      [this]() -> SearchAPI & { return GetSearchAPI(); },
      [catalogHeadersProvider]() { return catalogHeadersProvider->GetHeaders(); },
      [this](vector<BookmarkInfo> const & marks) { GetSearchAPI().OnBookmarksCreated(marks); },
      [this](vector<BookmarkInfo> const & marks) { GetSearchAPI().OnBookmarksUpdated(marks); },
      [this](vector<kml::MarkId> const & marks) { GetSearchAPI().OnBookmarksDeleted(marks); },
      [this](vector<BookmarkGroupInfo> const & marks) { GetSearchAPI().OnBookmarksAttached(marks); },
      [this](vector<BookmarkGroupInfo> const & marks) { GetSearchAPI().OnBookmarksDetached(marks); }));

  m_bmManager->InitRegionAddressGetter(m_featuresFetcher.GetDataSource(), *m_infoGetter);

  catalogHeadersProvider->SetBookmarkCatalog(&m_bmManager->GetCatalog());
  m_parsedMapApi.SetBookmarkManager(m_bmManager.get());
  m_routingManager.SetBookmarkManager(m_bmManager.get());
  m_guidesManager.SetBookmarkManager(m_bmManager.get());
  m_searchMarks.SetBookmarkManager(m_bmManager.get());

  m_user.AddSubscriber(m_bmManager->GetUserSubscriber());

  m_routingManager.SetTransitManager(&m_transitManager);
  m_routingManager.SetRouteStartBuildListener([this](std::vector<RouteMarkData> const & points)
  {
    auto const userPos = GetCurrentPosition();
    if (userPos)
      OnRouteStartBuild(m_featuresFetcher.GetDataSource(), points, *userPos);
  });

  InitCityFinder();
  InitDiscoveryManager();
  InitTaxiEngine();

  RegisterAllMaps();
  LOG(LDEBUG, ("Maps initialized"));

  // Need to reload cities boundaries because maps in indexer were updated.
  GetSearchAPI().LoadCitiesBoundaries();
  GetSearchAPI().CacheWorldLocalities();

  // Init storage with needed callback.
  m_storage.Init(
                 bind(&Framework::OnCountryFileDownloaded, this, _1, _2),
                 bind(&Framework::OnCountryFileDelete, this, _1, _2));
  m_storage.SetDownloadingPolicy(&m_storageDownloadingPolicy);
  m_storage.SetStartDownloadingCallback([this]() { UpdatePlacePageInfoForCurrentSelection(); });
  LOG(LDEBUG, ("Storage initialized"));

  // Local ads manager should be initialized after storage initialization.
  if (params.m_enableLocalAds)
  {
    auto const isActive = m_purchase->IsSubscriptionActive(SubscriptionType::RemoveAds);
    m_localAdsManager.Startup(m_bmManager.get(), !isActive);
    m_purchase->RegisterSubscription(&m_localAdsManager);
  }

  m_routingManager.SetRouterImpl(RouterType::Vehicle);

  UpdateMinBuildingsTapZoom();

  LOG(LDEBUG, ("Routing engine initialized"));

  LOG(LINFO, ("System languages:", languages::GetPreferred()));

  editor.SetDelegate(make_unique<search::EditorDelegate>(m_featuresFetcher.GetDataSource()));
  editor.SetInvalidateFn([this](){ InvalidateRect(GetCurrentViewport()); });
  editor.LoadEdits();

  m_featuresFetcher.GetDataSource().AddObserver(editor);

  LOG(LINFO, ("Editor initialized"));

  m_trafficManager.SetCurrentDataVersion(m_storage.GetCurrentDataVersion());
  m_trafficManager.SetSimplifiedColorScheme(LoadTrafficSimplifiedColors());
  m_trafficManager.SetEnabled(LoadTrafficEnabled());

  m_isolinesManager.SetEnabled(LoadIsolinesEnabled());

  m_guidesManager.SetApiDelegate(make_unique<GuidesOnMapDelegate>(catalogHeadersProvider));
  m_guidesManager.SetEnabled(LoadGuidesEnabled());

  m_adsEngine = make_unique<ads::Engine>(
      make_unique<ads::DownloadOnMapDelegate>(*m_infoGetter, m_storage, *m_promoApi, *m_purchase));

  InitTransliteration();
  LOG(LDEBUG, ("Transliterators initialized"));

  m_notificationManager.SetDelegate(
    std::make_unique<NotificationManagerDelegate>(m_featuresFetcher.GetDataSource(), *m_cityFinder,
                                                  m_addressGetter, *m_ugcApi, m_storage,
                                                  *m_infoGetter));
  m_notificationManager.Load();
  m_notificationManager.TrimExpired();

  alohalytics::Stats::Instance().LogEvent("UGC_ReviewNotification_queue",
    {{"unshown", std::to_string(m_notificationManager.GetCandidatesCount())}});

  eye::Eye::Instance().TrimExpired();
  eye::Eye::Instance().Subscribe(&m_notificationManager);

  GetPowerManager().Subscribe(this);
  GetPowerManager().Load();

  m_promoApi->SetDelegate(make_unique<PromoDelegate>(m_featuresFetcher.GetDataSource(),
                          *m_cityFinder, catalogHeadersProvider));
  eye::Eye::Instance().Subscribe(m_promoApi.get());

  // Clean the no longer used key from old devices.
  // Remove this line after April 2020 (assuming the majority of devices
  // will have updated by then).
  GetPlatform().RunTask(Platform::Thread::Gui, [] { settings::Delete("LastMigration"); });
}

Framework::~Framework()
{
  eye::Eye::Instance().UnsubscribeAll();
  GetPowerManager().UnsubscribeAll();

  m_threadRunner.reset();

  // Must be destroyed implicitly at the start of destruction,
  // since it stores raw pointers to other subsystems.
  m_adsEngine.reset();
  m_purchase.reset();

  osm::Editor & editor = osm::Editor::Instance();

  editor.SetDelegate({});
  editor.SetInvalidateFn({});

  GetBookmarkManager().Teardown();
  m_trafficManager.Teardown();
  DestroyDrapeEngine();
  m_featuresFetcher.SetOnMapDeregisteredCallback(nullptr);

  m_user.ClearSubscribers();
  // Must be destroyed implicitly, since it stores reference to m_user.
  m_bmManager.reset();
}

booking::Api * Framework::GetBookingApi(platform::NetworkPolicy const & policy)
{
  ASSERT(m_bookingApi, ());
  if (policy.CanUse())
    return m_bookingApi.get();

  return nullptr;
}

booking::Api const * Framework::GetBookingApi(platform::NetworkPolicy const & policy) const
{
  ASSERT(m_bookingApi, ());
  if (policy.CanUse())
    return m_bookingApi.get();

  return nullptr;
}

taxi::Engine * Framework::GetTaxiEngine(platform::NetworkPolicy const & policy)
{
  ASSERT(m_taxiEngine, ());
  if (policy.CanUse())
    return m_taxiEngine.get();

  return nullptr;
}

locals::Api * Framework::GetLocalsApi(platform::NetworkPolicy const & policy)
{
  ASSERT(m_localsApi, ());
  if (policy.CanUse())
    return m_localsApi.get();

  return nullptr;
}

promo::Api * Framework::GetPromoApi(platform::NetworkPolicy const & policy) const
{
  ASSERT(m_promoApi, ());
  if (policy.CanUse())
    return m_promoApi.get();

  return nullptr;
}

void Framework::ShowNode(storage::CountryId const & countryId)
{
  StopLocationFollow();

  ShowRect(CalcLimitRect(countryId, GetStorage(), GetCountryInfoGetter()));
}

void Framework::OnCountryFileDownloaded(storage::CountryId const & countryId,
                                        storage::LocalFilePtr const localFile)
{
  // Soft reset to signal that mwm file may be out of date in routing caches.
  m_routingManager.ResetRoutingSession();

  m2::RectD rect = mercator::Bounds::FullRect();

  if (localFile && localFile->OnDisk(MapFileType::Map))
  {
    // Add downloaded map.
    auto p = m_featuresFetcher.RegisterMap(*localFile);
    MwmSet::MwmId const & id = p.first;
    if (id.IsAlive())
      rect = id.GetInfo()->m_bordersRect;
  }
  m_trafficManager.Invalidate();
  m_transitManager.Invalidate();
  m_isolinesManager.Invalidate();
  m_localAdsManager.OnDownloadCountry(countryId);
  InvalidateRect(rect);
  GetSearchAPI().ClearCaches();
}

bool Framework::OnCountryFileDelete(storage::CountryId const & countryId,
                                    storage::LocalFilePtr const localFile)
{
  // Soft reset to signal that mwm file may be out of date in routing caches.
  m_routingManager.ResetRoutingSession();

  if (countryId == m_lastReportedCountry)
    m_lastReportedCountry = kInvalidCountryId;

  GetSearchAPI().CancelAllSearches();

  m2::RectD rect = mercator::Bounds::FullRect();

  bool deferredDelete = false;
  if (localFile)
  {
    rect = m_infoGetter->GetLimitRectForLeaf(countryId);
    m_featuresFetcher.DeregisterMap(platform::CountryFile(countryId));
    deferredDelete = true;
  }
  InvalidateRect(rect);

  GetSearchAPI().ClearCaches();
  return deferredDelete;
}

void Framework::OnMapDeregistered(platform::LocalCountryFile const & localFile)
{
  auto action = [this, localFile]
  {
    m_localAdsManager.OnMwmDeregistered(localFile);
    m_transitManager.OnMwmDeregistered(localFile);
    m_isolinesManager.OnMwmDeregistered(localFile);
    m_trafficManager.OnMwmDeregistered(localFile);
    m_popularityLoader.OnMwmDeregistered(localFile);

    m_storage.DeleteCustomCountryVersion(localFile);
  };

  // Call action on thread in which the framework was created
  // For more information look at comment for Observer class in mwm_set.hpp
  if (m_storage.GetThreadChecker().CalledOnOriginalThread())
    action();
  else
    GetPlatform().RunTask(Platform::Thread::Gui, action);
}

bool Framework::HasUnsavedEdits(storage::CountryId const & countryId)
{
  bool hasUnsavedChanges = false;
  auto const forEachInSubtree = [&hasUnsavedChanges, this](storage::CountryId const & fileName,
                                                           bool groupNode) {
    if (groupNode)
      return;
    hasUnsavedChanges |= osm::Editor::Instance().HaveMapEditsToUpload(
          m_featuresFetcher.GetDataSource().GetMwmIdByCountryFile(platform::CountryFile(fileName)));
  };
  GetStorage().ForEachInSubtree(countryId, forEachInSubtree);
  return hasUnsavedChanges;
}

void Framework::RegisterAllMaps()
{
  ASSERT(!m_storage.IsDownloadInProgress(),
         ("Registering maps while map downloading leads to removing downloading maps from "
          "ActiveMapsListener::m_items."));

  m_storage.RegisterAllLocalMaps(m_enabledDiffs);

  int minFormat = numeric_limits<int>::max();

  char const * kLastDownloadedMapsCheck = "LastDownloadedMapsCheck";
  auto const updateInterval = hours(24 * 7); // One week.
  uint32_t timestamp;
  bool const rc = settings::Get(kLastDownloadedMapsCheck, timestamp);
  auto const lastCheck = time_point<system_clock>(seconds(timestamp));
  bool const needStatisticsUpdate = !rc || lastCheck < system_clock::now() - updateInterval;
  stringstream listRegisteredMaps;

  vector<shared_ptr<LocalCountryFile>> maps;
  m_storage.GetLocalMaps(maps);
  for (auto const & localFile : maps)
  {
    auto p = RegisterMap(*localFile);
    if (p.second != MwmSet::RegResult::Success)
      continue;

    MwmSet::MwmId const & id = p.first;
    ASSERT(id.IsAlive(), ());
    minFormat = min(minFormat, static_cast<int>(id.GetInfo()->m_version.GetFormat()));
    if (needStatisticsUpdate)
    {
      listRegisteredMaps << localFile->GetCountryName() << ":" << id.GetInfo()->GetVersion() << ";";
    }
  }

  if (needStatisticsUpdate)
  {
    alohalytics::Stats::Instance().LogEvent("Downloader_Map_list",
    {{"AvailableStorageSpace", strings::to_string(GetPlatform().GetWritableStorageSpace())},
      {"DownloadedMaps", listRegisteredMaps.str()}});
    settings::Set(kLastDownloadedMapsCheck,
                  static_cast<uint64_t>(duration_cast<seconds>(
                                          system_clock::now().time_since_epoch()).count()));
  }
}

void Framework::DeregisterAllMaps()
{
  m_featuresFetcher.Clear();
  m_storage.Clear();
}

void Framework::LoadBookmarks()
{
  GetBookmarkManager().LoadBookmarks();
}

kml::MarkGroupId Framework::AddCategory(string const & categoryName)
{
  return GetBookmarkManager().CreateBookmarkCategory(categoryName);
}

void Framework::FillPointInfoForBookmark(Bookmark const & bmk, place_page::Info & info) const
{
  auto types = feature::TypesHolder::FromTypesIndexes(bmk.GetData().m_featureTypes);
  FillPointInfo(info, bmk.GetPivot(), {}, [&types](FeatureType & ft)
  {
    return !types.Empty() && feature::TypesHolder(ft).Equals(types);
  });
}

void Framework::FillBookmarkInfo(Bookmark const & bmk, place_page::Info & info) const
{
  info.SetBookmarkCategoryName(GetBookmarkManager().GetCategoryName(bmk.GetGroupId()));
  info.SetBookmarkData(bmk.GetData());
  info.SetBookmarkId(bmk.GetId());
  info.SetBookmarkCategoryId(bmk.GetGroupId());
  auto const description = GetPreferredBookmarkStr(info.GetBookmarkData().m_description);
  auto const openingMode = m_routingManager.IsRoutingActive() || description.empty()
                     ? place_page::OpeningMode::Preview
                     : place_page::OpeningMode::PreviewPlus;
  info.SetOpeningMode(openingMode);
  FillPointInfoForBookmark(bmk, info);
}

void Framework::FillTrackInfo(Track const & track, m2::PointD const & trackPoint,
                              place_page::Info & info) const
{
  info.SetTrackId(track.GetId());
  info.SetBookmarkCategoryId(track.GetGroupId());
  info.SetMercator(trackPoint);
}

search::ReverseGeocoder::Address Framework::GetAddressAtPoint(m2::PointD const & pt) const
{
  return m_addressGetter.GetAddressAtPoint(m_featuresFetcher.GetDataSource(), pt);
}

void Framework::FillFeatureInfo(FeatureID const & fid, place_page::Info & info) const
{
  if (!fid.IsValid())
  {
    LOG(LERROR, ("FeatureID is invalid:", fid));
    return;
  }

  FeaturesLoaderGuard const guard(m_featuresFetcher.GetDataSource(), fid.m_mwmId);
  auto ft = guard.GetFeatureByIndex(fid.m_index);
  if (!ft)
  {
    LOG(LERROR, ("Feature can't be loaded:", fid));
    return;
  }

  FillInfoFromFeatureType(*ft, info);
}

void Framework::FillPointInfo(place_page::Info & info, m2::PointD const & mercator,
                              string const & customTitle /* = {} */,
                              FeatureMatcher && matcher /* = nullptr */) const
{
  auto const fid = GetFeatureAtPoint(mercator, move(matcher));
  if (fid.IsValid())
  {
    m_featuresFetcher.GetDataSource().ReadFeature(
        [&](FeatureType & ft) { FillInfoFromFeatureType(ft, info); }, fid);
    // This line overwrites mercator center from area feature which can be far away.
    info.SetMercator(mercator);
  }
  else
  {
    FillNotMatchedPlaceInfo(info, mercator, customTitle);
  }
}

void Framework::FillNotMatchedPlaceInfo(place_page::Info & info, m2::PointD const & mercator,
                                        std::string const & customTitle /* = {} */) const
{
  if (customTitle.empty())
    info.SetCustomNameWithCoordinates(mercator, m_stringsBundle.GetString("core_placepage_unknown_place"));
  else
    info.SetCustomName(customTitle);
  info.SetCanEditOrAdd(CanEditMap());
  info.SetMercator(mercator);
}

void Framework::FillPostcodeInfo(string const & postcode, m2::PointD const & mercator,
                                 place_page::Info & info) const
{
  info.SetCustomNames(postcode, m_stringsBundle.GetString("postal_code"));
  info.SetMercator(mercator);
}

void Framework::FillInfoFromFeatureType(FeatureType & ft, place_page::Info & info) const
{
  using place_page::SponsoredType;
  auto const featureStatus = osm::Editor::Instance().GetFeatureStatus(ft.GetID());
  ASSERT_NOT_EQUAL(featureStatus, FeatureStatus::Deleted,
                   ("Deleted features cannot be selected from UI."));
  info.SetFeatureStatus(featureStatus);
  info.SetLocalizedWifiString(m_stringsBundle.GetString("wifi"));

  if (ftypes::IsAddressObjectChecker::Instance()(ft))
    info.SetAddress(GetAddressAtPoint(feature::GetCenter(ft)).FormatAddress());

  info.SetFromFeatureType(ft);

  if (ftypes::IsBookingChecker::Instance()(ft))
  {
    ASSERT(m_bookingApi, ());
    info.SetSponsoredType(SponsoredType::Booking);
    auto const & baseUrl = info.GetMetadata().Get(feature::Metadata::FMD_WEBSITE);
    auto const & hotelId = info.GetMetadata().Get(feature::Metadata::FMD_SPONSORED_ID);
    info.SetSponsoredUrl(m_bookingApi->GetBookHotelUrl(baseUrl));
    info.SetSponsoredDeepLink(m_bookingApi->GetDeepLink(hotelId));
    info.SetSponsoredDescriptionUrl(m_bookingApi->GetDescriptionUrl(baseUrl));
    info.SetSponsoredMoreUrl(m_bookingApi->GetMoreUrl(baseUrl));
    info.SetSponsoredReviewUrl(m_bookingApi->GetHotelReviewsUrl(hotelId, baseUrl));
    if (!m_bookingAvailabilityParams.IsEmpty())
    {
      auto const urlSetter = [this](auto const & getter, auto const & setter)
      {
        auto const & url = getter();
        auto const & urlWithParams =
          m_bookingApi->ApplyAvailabilityParams(url, m_bookingAvailabilityParams);
        setter(ref(urlWithParams));
      };
      using place_page::Info;
      urlSetter(bind(&Info::GetSponsoredUrl, &info),
                bind(&Info::SetSponsoredUrl, &info, _1));
      urlSetter(bind(&Info::GetSponsoredDescriptionUrl, &info),
                bind(&Info::SetSponsoredDescriptionUrl, &info, _1));
      urlSetter(bind(&Info::GetSponsoredMoreUrl, &info),
                bind(&Info::SetSponsoredMoreUrl, &info, _1));
      urlSetter(bind(&Info::GetSponsoredReviewUrl, &info),
                bind(&Info::SetSponsoredReviewUrl, &info, _1));
      urlSetter(bind(&Info::GetSponsoredDeepLink, &info),
                bind(&Info::SetSponsoredDeepLink, &info, _1));
    }
  }
  else if (ftypes::IsOpentableChecker::Instance()(ft))
  {
    info.SetSponsoredType(SponsoredType::Opentable);
    auto const & sponsoredId = info.GetMetadata().Get(feature::Metadata::FMD_SPONSORED_ID);
    auto const & url = opentable::Api::GetBookTableUrl(sponsoredId);
    info.SetSponsoredUrl(url);
    info.SetSponsoredDescriptionUrl(url);
  }
  else if (ftypes::IsHotelChecker::Instance()(ft))
  {
    ASSERT(m_cityFinder, ());
    auto const url = MakeSearchBookingUrl(*m_bookingApi, *m_cityFinder, ft);
    info.SetBookingSearchUrl(url);
    LOG(LINFO, (url));
  }
  else if (m_purchase && !m_purchase->IsSubscriptionActive(SubscriptionType::RemoveAds) &&
           PartnerChecker::Instance()(ft))
  {
    info.SetSponsoredType(place_page::SponsoredType::Partner);
    auto const partnerIndex = PartnerChecker::Instance().GetPartnerIndex(ft);
    info.SetPartnerIndex(partnerIndex);
    auto const & partnerInfo = GetPartnerByIndex(partnerIndex);
    if (partnerInfo.m_hasButton && ft.GetID().GetMwmVersion() >= partnerInfo.m_minMapVersion)
    {
      auto url = info.GetMetadata().Get(feature::Metadata::FMD_BANNER_URL);
      if (url.empty())
        url = partnerInfo.m_defaultBannerUrl;
      info.SetSponsoredUrl(url);
      info.SetSponsoredDescriptionUrl(url);
    }
  }
  else if (ftypes::IsHolidayChecker::Instance()(ft) &&
           !info.GetMetadata().Get(feature::Metadata::FMD_RATING).empty())
  {
    info.SetSponsoredType(place_page::SponsoredType::Holiday);
  }
  else if (ftypes::IsPromoCatalogChecker::Instance()(ft))
  {
    info.SetOpeningMode(m_routingManager.IsRoutingActive()
                        ? place_page::OpeningMode::Preview
                        : place_page::OpeningMode::PreviewPlus);
    info.SetSponsoredType(SponsoredType::PromoCatalogCity);
  }
  else if (ftypes::IsPromoCatalogSightseeingsChecker::Instance()(ft))
  {
    info.SetOpeningMode(m_routingManager.IsRoutingActive() || !GetPlatform().IsConnected()
                        ? place_page::OpeningMode::Preview
                        : place_page::OpeningMode::PreviewPlus);
    info.SetSponsoredType(SponsoredType::PromoCatalogSightseeings);
  }
  else if (ftypes::IsPromoCatalogOutdoorChecker::Instance()(ft))
  {
    info.SetOpeningMode(m_routingManager.IsRoutingActive() || !GetPlatform().IsConnected()
                        ? place_page::OpeningMode::Preview
                        : place_page::OpeningMode::PreviewPlus);
    info.SetSponsoredType(SponsoredType::PromoCatalogOutdoor);
  }

  FillLocalExperts(ft, info);
  FillDescription(ft, info);

  auto const mwmInfo = ft.GetID().m_mwmId.GetInfo();
  bool const isMapVersionEditable = mwmInfo && mwmInfo->m_version.IsEditableMap();
  bool const canEditOrAdd = featureStatus != FeatureStatus::Obsolete && CanEditMap() &&
                            !info.IsNotEditableSponsored() && isMapVersionEditable;
  info.SetCanEditOrAdd(canEditOrAdd);

  if (m_localAdsManager.IsSupportedType(info.GetTypes()))
  {
    info.SetLocalAdsUrl(m_localAdsManager.GetCompanyUrl(ft.GetID()));
    auto status = m_localAdsManager.HasAds(ft.GetID()) ? place_page::LocalAdsStatus::Customer
                                                       : place_page::LocalAdsStatus::Candidate;
    if (status == place_page::LocalAdsStatus::Customer && !m_localAdsManager.HasVisualization(ft.GetID()))
      status = place_page::LocalAdsStatus::Hidden;
    info.SetLocalAdsStatus(status);
  }
  else
  {
    info.SetLocalAdsStatus(place_page::LocalAdsStatus::NotAvailable);
  }

  auto const latlon = mercator::ToLatLon(feature::GetCenter(ft));
  ASSERT(m_taxiEngine, ());
  info.SetReachableByTaxiProviders(m_taxiEngine->GetProvidersAtPos(latlon));
  info.SetPopularity(m_popularityLoader.Get(ft.GetID()));

  // Fill countryId for place page info
  uint32_t const placeContinentType = classif().GetTypeByPath({"place", "continent"});
  if (info.GetTypes().Has(placeContinentType))
    return;

  uint32_t const placeCountryType = classif().GetTypeByPath({"place", "country"});
  uint32_t const placeStateType = classif().GetTypeByPath({"place", "state"});

  bool const isState = info.GetTypes().Has(placeStateType);
  bool const isCountry = info.GetTypes().Has(placeCountryType);
  if (isCountry || isState)
  {
    size_t const level = isState ? 1 : 0;
    CountriesVec countries;
    CountryId countryId = m_infoGetter->GetRegionCountryId(info.GetMercator());
    GetStorage().GetTopmostNodesFor(countryId, countries, level);
    if (countries.size() == 1)
      countryId = countries.front();

    info.SetCountryId(countryId);
    info.SetTopmostCountryIds(move(countries));
  }
}

void Framework::FillApiMarkInfo(ApiMarkPoint const & api, place_page::Info & info) const
{
  FillPointInfo(info, api.GetPivot());
  string const & name = api.GetName();
  if (!name.empty())
    info.SetCustomName(name);
  info.SetApiId(api.GetApiID());
  info.SetApiUrl(GenerateApiBackUrl(api));
}

void Framework::FillSearchResultInfo(SearchMarkPoint const & smp, place_page::Info & info) const
{
  info.SetIsSearchMark(true);

  if (smp.GetFeatureID().IsValid())
    FillFeatureInfo(smp.GetFeatureID(), info);
  else
    FillPointInfo(info, smp.GetPivot(), smp.GetMatchedName());
}

void Framework::FillMyPositionInfo(place_page::Info & info, place_page::BuildInfo const & buildInfo) const
{
  auto const position = GetCurrentPosition();
  VERIFY(position, ());
  info.SetMercator(*position);
  info.SetCustomName(m_stringsBundle.GetString("core_my_position"));

  UserMark const * mark = FindUserMarkInTapPosition(buildInfo);
  if (mark != nullptr && mark->GetMarkType() == UserMark::Type::ROUTING)
  {
    auto routingMark = static_cast<RouteMarkPoint const *>(mark);
    info.SetIsRoutePoint();
    info.SetRouteMarkType(routingMark->GetRoutePointType());
    info.SetIntermediateIndex(routingMark->GetIntermediateIndex());
  }
}

void Framework::FillRouteMarkInfo(RouteMarkPoint const & rmp, place_page::Info & info) const
{
  FillPointInfo(info, rmp.GetPivot());
  info.SetIsRoutePoint();
  info.SetRouteMarkType(rmp.GetRoutePointType());
  info.SetIntermediateIndex(rmp.GetIntermediateIndex());
}

void Framework::FillRoadTypeMarkInfo(RoadWarningMark const & roadTypeMark, place_page::Info & info) const
{
  if (roadTypeMark.GetFeatureID().IsValid())
  {
    FeaturesLoaderGuard const guard(m_featuresFetcher.GetDataSource(), roadTypeMark.GetFeatureID().m_mwmId);
    auto ft = guard.GetFeatureByIndex(roadTypeMark.GetFeatureID().m_index);
    if (ft)
    {
      FillInfoFromFeatureType(*ft, info);

      info.SetRoadType(*ft, roadTypeMark.GetRoadWarningType(),
                       RoadWarningMark::GetLocalizedRoadWarningType(roadTypeMark.GetRoadWarningType()),
                       roadTypeMark.GetDistance());
      info.SetMercator(roadTypeMark.GetPivot());
      return;
    }
    else
    {
      LOG(LERROR, ("Feature can't be loaded:", roadTypeMark.GetFeatureID()));
    }
  }

  info.SetRoadType(roadTypeMark.GetRoadWarningType(),
                   RoadWarningMark::GetLocalizedRoadWarningType(roadTypeMark.GetRoadWarningType()),
                   roadTypeMark.GetDistance());
  info.SetMercator(roadTypeMark.GetPivot());
}

void Framework::ShowBookmark(kml::MarkId id)
{
  auto const * mark = m_bmManager->GetBookmark(id);
  ShowBookmark(mark);
}

void Framework::ShowBookmark(Bookmark const * mark)
{
  if (mark == nullptr)
    return;

  StopLocationFollow();

  place_page::BuildInfo info;
  info.m_mercator = mark->GetPivot();
  info.m_userMarkId = mark->GetId();
  m_currentPlacePageInfo = BuildPlacePageInfo(info);

  auto scale = static_cast<int>(mark->GetScale());
  if (scale == 0)
    scale = scales::GetUpperComfortScale();

  auto es = GetBookmarkManager().GetEditSession();
  es.SetIsVisible(mark->GetGroupId(), true /* visible */);

  if (m_drapeEngine != nullptr)
  {
    m_drapeEngine->SetModelViewCenter(mark->GetPivot(), scale, true /* isAnim */,
                                      true /* trackVisibleViewport */);
  }

  ActivateMapSelection(m_currentPlacePageInfo);
}

void Framework::ShowTrack(kml::TrackId trackId)
{
  auto & bm = GetBookmarkManager();
  auto const track = bm.GetTrack(trackId);
  if (track == nullptr)
    return;

  auto rect = track->GetLimitRect();
  ExpandRectForPreview(rect);

  StopLocationFollow();
  ShowRect(rect);

  auto es = GetBookmarkManager().GetEditSession();
  es.SetIsVisible(track->GetGroupId(), true /* visible */);

  if (track->IsInteractive())
    bm.SetDefaultTrackSelection(trackId, true /* showInfoSign */);
}

void Framework::ShowBookmarkCategory(kml::MarkGroupId categoryId, bool animation)
{
  auto & bm = GetBookmarkManager();
  auto rect = bm.GetCategoryRect(categoryId, true /* addIconsSize */);
  if (!rect.IsValid())
    return;

  ExpandRectForPreview(rect);

  StopLocationFollow();
  ShowRect(rect, -1 /* maxScale */, animation);

  auto es = bm.GetEditSession();
  es.SetIsVisible(categoryId, true /* visible */);

  auto const trackIds = bm.GetTrackIds(categoryId);
  for (auto trackId : trackIds)
  {
    if (!bm.GetTrack(trackId)->IsInteractive())
      continue;
    bm.SetDefaultTrackSelection(trackId, true /* showInfoSign */);
    break;
  }
}

void Framework::ShowFeature(FeatureID const & featureId)
{
  StopLocationFollow();

  place_page::BuildInfo info;
  info.m_featureId = featureId;
  info.m_match = place_page::BuildInfo::Match::FeatureOnly;
  m_currentPlacePageInfo = BuildPlacePageInfo(info);

  if (m_drapeEngine != nullptr)
  {
    auto const pt = m_currentPlacePageInfo->GetMercator();
    auto const scale = scales::GetUpperComfortScale();
    m_drapeEngine->SetModelViewCenter(pt, scale, true /* isAnim */, true /* trackVisibleViewport */);
  }
  ActivateMapSelection(m_currentPlacePageInfo);
}

void Framework::AddBookmarksFile(string const & filePath, bool isTemporaryFile)
{
  GetBookmarkManager().LoadBookmark(filePath, isTemporaryFile);
}

void Framework::PrepareToShutdown()
{
  DestroyDrapeEngine();
}

void Framework::SetDisplacementMode(DisplacementModeManager::Slot slot, bool show)
{
  m_displacementModeManager.Set(slot, show);
}

void Framework::SaveViewport()
{
  m2::AnyRectD rect;
  if (m_currentModelView.isPerspective())
  {
    ScreenBase modelView = m_currentModelView;
    modelView.ResetPerspective();
    rect = modelView.GlobalRect();
  }
  else
  {
    rect = m_currentModelView.GlobalRect();
  }
  settings::Set("ScreenClipRect", rect);
}

void Framework::LoadViewport()
{
  m2::AnyRectD rect;
  if (settings::Get("ScreenClipRect", rect) && df::GetWorldRect().IsRectInside(rect.GetGlobalRect()))
  {
    if (m_drapeEngine != nullptr)
      m_drapeEngine->SetModelViewAnyRect(rect, false /* isAnim */, false /* useVisibleViewport */);
  }
  else
  {
    ShowAll();
  }
}

void Framework::ShowAll()
{
  if (m_drapeEngine == nullptr)
    return;
  m_drapeEngine->SetModelViewAnyRect(m2::AnyRectD(m_featuresFetcher.GetWorldRect()), false /* isAnim */,
                                     false /* useVisibleViewport */);
}

m2::PointD Framework::GetPixelCenter() const
{
  return m_currentModelView.PixelRectIn3d().Center();
}

m2::PointD Framework::GetVisiblePixelCenter() const
{
  return m_visibleViewport.Center();
}

m2::PointD const & Framework::GetViewportCenter() const
{
  return m_currentModelView.GetOrg();
}

void Framework::SetViewportCenter(m2::PointD const & pt, int zoomLevel /* = -1 */,
                                  bool isAnim /* = true */)
{
  if (m_drapeEngine != nullptr)
    m_drapeEngine->SetModelViewCenter(pt, zoomLevel, isAnim, false /* trackVisibleViewport */);
}

m2::RectD Framework::GetCurrentViewport() const
{
  return m_currentModelView.ClipRect();
}

void Framework::SetVisibleViewport(m2::RectD const & rect)
{
  if (m_drapeEngine == nullptr)
    return;

  double constexpr kEps = 0.5;
  if (m2::IsEqual(m_visibleViewport, rect, kEps, kEps))
    return;

  double constexpr kMinSize = 100.0;
  if (rect.SizeX() < kMinSize || rect.SizeY() < kMinSize)
    return;

  m_visibleViewport = rect;
  m_drapeEngine->SetVisibleViewport(rect);
}

void Framework::ShowRect(m2::RectD const & rect, int maxScale, bool animation, bool useVisibleViewport)
{
  if (m_drapeEngine == nullptr)
    return;

  m_drapeEngine->SetModelViewRect(rect, true /* applyRotation */, maxScale /* zoom */, animation,
                                  useVisibleViewport);
}

void Framework::ShowRect(m2::AnyRectD const & rect, bool animation, bool useVisibleViewport)
{
  if (m_drapeEngine != nullptr)
    m_drapeEngine->SetModelViewAnyRect(rect, animation, useVisibleViewport);
}

void Framework::GetTouchRect(m2::PointD const & center, uint32_t pxRadius, m2::AnyRectD & rect)
{
  m_currentModelView.GetTouchRect(center, static_cast<double>(pxRadius), rect);
}

void Framework::SetViewportListener(TViewportChangedFn const & fn)
{
  m_viewportChangedFn = fn;
}

#if defined(OMIM_OS_MAC) || defined(OMIM_OS_LINUX)
void Framework::NotifyGraphicsReady(TGraphicsReadyFn const & fn, bool needInvalidate)
{
  if (m_drapeEngine != nullptr)
    m_drapeEngine->NotifyGraphicsReady(fn, needInvalidate);
}
#endif

void Framework::StopLocationFollow()
{
  if (m_drapeEngine != nullptr)
    m_drapeEngine->StopLocationFollow();
}

void Framework::OnSize(int w, int h)
{
  if (m_drapeEngine != nullptr)
    m_drapeEngine->Resize(std::max(w, 2), std::max(h, 2));
}

namespace
{

double ScaleModeToFactor(Framework::EScaleMode mode)
{
  double factors[] = { 2.0, 1.5, 0.5, 0.67 };
  return factors[mode];
}

} // namespace

void Framework::Scale(EScaleMode mode, bool isAnim)
{
  Scale(ScaleModeToFactor(mode), isAnim);
}

void Framework::Scale(Framework::EScaleMode mode, m2::PointD const & pxPoint, bool isAnim)
{
  Scale(ScaleModeToFactor(mode), pxPoint, isAnim);
}

void Framework::Scale(double factor, bool isAnim)
{
  Scale(factor, GetVisiblePixelCenter(), isAnim);
}

void Framework::Scale(double factor, m2::PointD const & pxPoint, bool isAnim)
{
  if (m_drapeEngine != nullptr)
    m_drapeEngine->Scale(factor, pxPoint, isAnim);
}

void Framework::Move(double factorX, double factorY, bool isAnim)
{
  if (m_drapeEngine != nullptr)
    m_drapeEngine->Move(factorX, factorY, isAnim);
}

void Framework::Rotate(double azimuth, bool isAnim)
{
  if (m_drapeEngine != nullptr)
    m_drapeEngine->Rotate(azimuth, isAnim);
}

void Framework::TouchEvent(df::TouchEvent const & touch)
{
  if (m_drapeEngine != nullptr)
    m_drapeEngine->AddTouchEvent(touch);
}

int Framework::GetDrawScale() const
{
  if (m_drapeEngine != nullptr)
    return df::GetDrawTileScale(m_currentModelView);
  
  return 0;
}

void Framework::RunFirstLaunchAnimation()
{
  if (m_drapeEngine != nullptr)
    m_drapeEngine->RunFirstLaunchAnimation();
}

bool Framework::IsCountryLoaded(m2::PointD const & pt) const
{
  // TODO (@gorshenin, @govako): the method's name is quite
  // obfuscating and should be fixed.

  // Correct, but slow version (check country polygon).
  string const fName = m_infoGetter->GetRegionCountryId(pt);
  if (fName.empty())
    return true;

  return m_featuresFetcher.IsLoaded(fName);
}

bool Framework::IsCountryLoadedByName(string const & name) const
{
  return m_featuresFetcher.IsLoaded(name);
}

void Framework::InvalidateRect(m2::RectD const & rect)
{
  if (m_drapeEngine != nullptr)
    m_drapeEngine->InvalidateRect(rect);
}

void Framework::ClearAllCaches()
{
  m_featuresFetcher.ClearCaches();
  m_infoGetter->ClearCaches();
  GetSearchAPI().ClearCaches();
}

void Framework::OnUpdateCurrentCountry(m2::PointD const & pt, int zoomLevel)
{
  storage::CountryId newCountryId;
  if (zoomLevel > scales::GetUpperWorldScale())
    newCountryId = m_infoGetter->GetRegionCountryId(pt);

  if (newCountryId == m_lastReportedCountry)
    return;

  m_lastReportedCountry = newCountryId;

  GetPlatform().RunTask(Platform::Thread::Gui, [this, newCountryId]()
  {
    if (m_currentCountryChanged != nullptr)
      m_currentCountryChanged(newCountryId);
  });
}

void Framework::SetCurrentCountryChangedListener(TCurrentCountryChanged const & listener)
{
  m_currentCountryChanged = listener;
  m_lastReportedCountry = kInvalidCountryId;
}

void Framework::MemoryWarning()
{
  LOG(LINFO, ("MemoryWarning"));
  ClearAllCaches();
  SharedBufferManager::instance().clearReserved();
}

void Framework::EnterBackground()
{
  m_startBackgroundTime = base::Timer::LocalTime();
  settings::Set("LastEnterBackground", m_startBackgroundTime);

  if (m_drapeEngine != nullptr)
    m_drapeEngine->OnEnterBackground();

  SaveViewport();

  m_trafficManager.OnEnterBackground();
  m_routingManager.SetAllowSendingPoints(false);

  ms::LatLon const ll = mercator::ToLatLon(GetViewportCenter());
  alohalytics::Stats::Instance().LogEvent("Framework::EnterBackground", {{"zoom", strings::to_string(GetDrawScale())},
                                          {"foregroundSeconds", strings::to_string(
                                           static_cast<int>(m_startBackgroundTime - m_startForegroundTime))}},
                                          alohalytics::Location::FromLatLon(ll.m_lat, ll.m_lon));
  // Do not clear caches for Android. This function is called when main activity is paused,
  // but at the same time search activity (for example) is enabled.
  // TODO(AlexZ): Use onStart/onStop on Android to correctly detect app background and remove #ifndef.
#ifndef OMIM_OS_ANDROID
  ClearAllCaches();
#endif
}

void Framework::EnterForeground()
{
  m_startForegroundTime = base::Timer::LocalTime();
  if (m_drapeEngine != nullptr && m_startBackgroundTime != 0.0)
  {
    auto const secondsInBackground = m_startForegroundTime - m_startBackgroundTime;
    m_drapeEngine->OnEnterForeground(secondsInBackground);

    if (m_guidesManager.IsEnabled() &&
      secondsInBackground / 60 / 60 > kGuidesEnabledInBackgroundMaxHours)
    {
      auto const shownCount = m_guidesManager.GetShownGuidesCount();
      m_guidesManager.SetEnabled(false);
      SaveGuidesEnabled(false);

      alohalytics::LogEvent("Map_Layers_deactivate",
                            {{"name", "guides"}, {"count", strings::to_string(shownCount)}});
    }
  }

  m_trafficManager.OnEnterForeground();
  m_routingManager.SetAllowSendingPoints(true);
}

void Framework::InitCountryInfoGetter()
{
  ASSERT(!m_infoGetter.get(), ("InitCountryInfoGetter() must be called only once."));

  auto const & platform = GetPlatform();
  m_infoGetter = CountryInfoReader::CreateCountryInfoGetter(platform);
  m_infoGetter->SetAffiliations(&m_storage.GetAffiliations());
}

void Framework::InitUGC()
{
  ASSERT(!m_ugcApi.get(), ("InitUGC() must be called only once."));

  m_ugcApi = make_unique<ugc::Api>(m_featuresFetcher.GetDataSource(), [this](size_t numberOfUnsynchronized) {
    if (numberOfUnsynchronized == 0)
      return;

    alohalytics::Stats::Instance().LogEvent(
        "UGC_unsent", {{"num", strings::to_string(numberOfUnsynchronized)},
                       {"is_authenticated", strings::to_string(m_user.IsAuthenticated())}});
  });

  bool ugcStorageValidationExecuted = false;
  settings::TryGet("WasUgcStorageValidationExecuted", ugcStorageValidationExecuted);

  if (!ugcStorageValidationExecuted)
  {
    m_ugcApi->ValidateStorage();
    settings::Set("WasUgcStorageValidationExecuted", true);
  }
}

void Framework::InitSearchAPI(size_t numThreads)
{
  ASSERT(!m_searchAPI.get(), ("InitSearchAPI() must be called only once."));
  ASSERT(m_infoGetter.get(), ());
  try
  {
    m_searchAPI =
        make_unique<SearchAPI>(m_featuresFetcher.GetDataSource(), m_storage, *m_infoGetter,
                               numThreads, static_cast<SearchAPI::Delegate &>(*this));
  }
  catch (RootException const & e)
  {
    LOG(LCRITICAL, ("Can't load needed resources for SearchAPI:", e.Msg()));
  }
}

void Framework::InitDiscoveryManager()
{
  CHECK(m_searchAPI.get(), ("InitDiscoveryManager() must be called after InitSearchApi()"));
  CHECK(m_cityFinder.get(), ("InitDiscoveryManager() must be called after InitCityFinder()"));

  discovery::Manager::APIs const apis(*m_searchAPI, *m_promoApi, *m_localsApi);
  m_discoveryManager = make_unique<discovery::Manager>(m_featuresFetcher.GetDataSource(), apis);
}

void Framework::InitTransliteration()
{
  InitTransliterationInstanceWithDefaultDirs();

  if (!LoadTransliteration())
    Transliteration::Instance().SetMode(Transliteration::Mode::Disabled);
}

storage::CountryId Framework::GetCountryIndex(m2::PointD const & pt) const
{
  return m_infoGetter->GetRegionCountryId(pt);
}

string Framework::GetCountryName(m2::PointD const & pt) const
{
  storage::CountryInfo info;
  m_infoGetter->GetRegionInfo(pt, info);
  return info.m_name;
}

Framework::DoAfterUpdate Framework::ToDoAfterUpdate() const
{
  auto const connectionStatus = Platform::ConnectionStatus();
  if (connectionStatus == Platform::EConnectionType::CONNECTION_NONE)
    return DoAfterUpdate::Nothing;

  auto const & s = GetStorage();
  auto const & rootId = s.GetRootId();
  if (!IsEnoughSpaceForUpdate(rootId, s))
    return DoAfterUpdate::Nothing;

  NodeAttrs attrs;
  s.GetNodeAttrs(rootId, attrs);
  MwmSize const countrySizeInBytes = attrs.m_localMwmSize;

  if (countrySizeInBytes == 0 || attrs.m_status != NodeStatus::OnDiskOutOfDate)
    return DoAfterUpdate::Nothing;

  if (s.IsPossibleToAutoupdate() && connectionStatus == Platform::EConnectionType::CONNECTION_WIFI)
    return DoAfterUpdate::AutoupdateMaps;

  return DoAfterUpdate::AskForUpdateMaps;
}

SearchAPI & Framework::GetSearchAPI()
{
  ASSERT(m_searchAPI != nullptr, ("Search API is not initialized."));
  return *m_searchAPI;
}

SearchAPI const & Framework::GetSearchAPI() const
{
  ASSERT(m_searchAPI != nullptr, ("Search API is not initialized."));
  return *m_searchAPI;
}

search::DisplayedCategories const & Framework::GetDisplayedCategories()
{
  ASSERT(m_displayedCategories, ());
  ASSERT(m_cityFinder, ());

  string city;

  if (auto const position = GetCurrentPosition())
    city = m_cityFinder->GetCityName(*position, StringUtf8Multilang::kEnglishCode);

  // Apply sponsored modifiers.
  if (m_purchase && !m_purchase->IsSubscriptionActive(SubscriptionType::RemoveAds))
  {
    // Add Category modifiers here.
    //std::tuple<Modifier> modifiers(city);
    //base::for_each_in_tuple(modifiers, [&](size_t, SponsoredCategoryModifier & modifier)
    //{
    //  m_displayedCategories->Modify(modifier);
    //});
  }

  return *m_displayedCategories;
}

void Framework::SelectSearchResult(search::Result const & result, bool animation)
{
  using namespace search;
  place_page::BuildInfo info;
  info.m_source = place_page::BuildInfo::Source::Search;
  info.m_needAnimationOnSelection = false;
  int scale = -1;
  switch (result.GetResultType())
  {
  case Result::Type::Feature:
    info.m_mercator = result.GetFeatureCenter();
    info.m_featureId = result.GetFeatureID();
    info.m_isGeometrySelectionAllowed = true;
    break;

  case Result::Type::LatLon:
    info.m_mercator = result.GetFeatureCenter();
    info.m_match = place_page::BuildInfo::Match::Nothing;
    scale = scales::GetUpperComfortScale();
    break;

  case Result::Type::Postcode:
    info.m_mercator = result.GetFeatureCenter();
    info.m_match = place_page::BuildInfo::Match::Nothing;
    info.m_postcode = result.GetString();
    scale = scales::GetUpperComfortScale();
    break;

  case Result::Type::SuggestFromFeature:
  case Result::Type::PureSuggest:
    m_currentPlacePageInfo = {};
    ASSERT(false, ("Suggests should not be here."));
    return;
  }

  if (result.m_details.m_isHotel && result.HasPoint())
  {
    auto copy = result;
    search::Results results;
    results.AddResultNoChecks(move(copy));
    ShowViewportSearchResults(results, true, m_activeFilters);
  }

  m_currentPlacePageInfo = BuildPlacePageInfo(info);
  if (m_currentPlacePageInfo)
  {
    if (scale < 0)
      scale = GetFeatureViewportScale(m_currentPlacePageInfo->GetTypes());

    m2::PointD const center = m_currentPlacePageInfo->GetMercator();
    if (m_drapeEngine != nullptr)
      m_drapeEngine->SetModelViewCenter(center, scale, animation, true /* trackVisibleViewport */);

    ActivateMapSelection(m_currentPlacePageInfo);
  }
}

void Framework::ShowSearchResult(search::Result const & res, bool animation)
{
  GetSearchAPI().CancelAllSearches();
  StopLocationFollow();

  alohalytics::LogEvent("searchShowResult", {{"pos", strings::to_string(res.GetPositionInResults())},
                                             {"result", res.ToStringForStats()}});
  SelectSearchResult(res, animation);
}

size_t Framework::ShowSearchResults(search::Results const & results)
{
  using namespace search;

  size_t count = results.GetCount();
  if (count == 0)
    return 0;

  if (count == 1)
  {
    Result const & r = results[0];
    if (!r.IsSuggest())
      ShowSearchResult(r);
    else
      return 0;
  }

  FillSearchResultsMarks(true /* clear */, results);

  // Setup viewport according to results.
  m2::AnyRectD viewport = m_currentModelView.GlobalRect();
  m2::PointD const center = viewport.Center();

  double minDistance = numeric_limits<double>::max();
  int minInd = -1;
  for (size_t i = 0; i < count; ++i)
  {
    Result const & r = results[i];
    if (r.HasPoint())
    {
      double const dist = center.SquaredLength(r.GetFeatureCenter());
      if (dist < minDistance)
      {
        minDistance = dist;
        minInd = static_cast<int>(i);
      }
    }
  }

  if (minInd != -1)
  {
    m2::PointD const pt = results[minInd].GetFeatureCenter();

    if (m_currentModelView.isPerspective())
    {
      StopLocationFollow();
      SetViewportCenter(pt);
      return count;
    }

    if (!viewport.IsPointInside(pt))
    {
      viewport.SetSizesToIncludePoint(pt);
      StopLocationFollow();
    }
  }

  // Graphics engine can be recreated (on Android), so we always set up viewport here.
  ShowRect(viewport);

  return count;
}

void Framework::FillSearchResultsMarks(bool clear, search::Results const & results)
{
  FillSearchResultsMarks(results.begin(), results.end(), clear,
                         Framework::SearchMarkPostProcessing());
}

void Framework::FillSearchResultsMarks(search::Results::ConstIter begin,
                                       search::Results::ConstIter end, bool clear,
                                       SearchMarkPostProcessing fn)
{
  auto editSession = GetBookmarkManager().GetEditSession();
  if (clear)
    editSession.ClearGroup(UserMark::Type::SEARCH);
  editSession.SetIsVisible(UserMark::Type::SEARCH, true);

  optional<string> reason;
  for (auto it = begin; it != end; ++it)
  {
    auto const & r = *it;
    if (!r.HasPoint())
      continue;

    auto * mark = editSession.CreateUserMark<SearchMarkPoint>(r.GetFeatureCenter());
    auto const isFeature = r.GetResultType() == search::Result::Type::Feature;
    if (isFeature)
      mark->SetFoundFeature(r.GetFeatureID());

    mark->SetMatchedName(r.GetString());

    if (isFeature && r.IsRefusedByFilter())
    {
      mark->SetPreparing(true);

      if (!reason)
        reason = platform::GetLocalizedString("booking_map_component_filters");

      m_searchMarks.AppendUnavailable(mark->GetFeatureID(), *reason);
    }

    if (r.m_details.m_isSponsoredHotel)
    {
      mark->SetBookingType(isFeature && m_localAdsManager.HasVisualization(r.GetFeatureID()) /* hasLocalAds */);
      mark->SetRating(r.m_details.m_hotelRating);
      mark->SetPricing(r.m_details.m_hotelPricing);
    }
    else if (isFeature)
    {
      auto product = GetProductInfo(r);
      auto const type = r.GetFeatureType();
      mark->SetFromType(type, m_localAdsManager.HasVisualization(r.GetFeatureID()));
      if (product.m_ugcRating != search::ProductInfo::kInvalidRating)
        mark->SetRating(product.m_ugcRating);
    }

    // Case when place page is opened and new portion of results delivered.
    if (isFeature && m_currentPlacePageInfo && m_currentPlacePageInfo->IsFeature() &&
        m_currentPlacePageInfo->GetID() == mark->GetFeatureID())
    {
      m_searchMarks.MarkUnavailableIfNeeded(mark);
    }

    if (isFeature && m_searchMarks.IsUsed(mark->GetFeatureID()))
      mark->SetVisited(true);

    if (fn)
      fn(*mark);
  }
}

bool Framework::GetDistanceAndAzimut(m2::PointD const & point,
                                     double lat, double lon, double north,
                                     string & distance, double & azimut)
{
#ifdef FIXED_LOCATION
  m_fixedPos.GetLat(lat);
  m_fixedPos.GetLon(lon);
  m_fixedPos.GetNorth(north);
#endif

  double const d = ms::DistanceOnEarth(lat, lon, mercator::YToLat(point.y), mercator::XToLon(point.x));

  // Distance may be less than 1.0
  UNUSED_VALUE(measurement_utils::FormatDistance(d, distance));

  // We calculate azimuth even when distance is very short (d ~ 0),
  // because return value has 2 states (near me or far from me).

  azimut = ang::Azimuth(mercator::FromLatLon(lat, lon), point, north);

  double const pi2 = 2.0*math::pi;
  if (azimut < 0.0)
    azimut += pi2;
  else if (azimut > pi2)
    azimut -= pi2;

  // This constant and return value is using for arrow/flag choice.
  return (d < 25000.0);
}

void Framework::CreateDrapeEngine(ref_ptr<dp::GraphicsContextFactory> contextFactory, DrapeCreationParams && params)
{
  auto idReadFn = [this](df::MapDataProvider::TReadCallback<FeatureID const> const & fn,
                         m2::RectD const & r,
                         int scale) -> void { m_featuresFetcher.ForEachFeatureID(r, fn, scale); };

  auto featureReadFn = [this](df::MapDataProvider::TReadCallback<FeatureType> const & fn,
                              vector<FeatureID> const & ids) -> void
  {
    m_featuresFetcher.ReadFeatures(fn, ids);
  };

  auto myPositionModeChangedFn = [this](location::EMyPositionMode mode, bool routingActive)
  {
    GetPlatform().RunTask(Platform::Thread::Gui, [this, mode, routingActive]()
    {
      // Deactivate selection (and hide place page) if we return to routing in F&R mode.
      if (routingActive && mode == location::FollowAndRotate)
        DeactivateMapSelection(true /* notifyUI */);

      if (m_myPositionListener != nullptr)
        m_myPositionListener(mode, routingActive);
    });
  };

  auto filterFeatureFn = [this](FeatureType & ft) -> bool
  {
    if (m_purchase && !m_purchase->IsSubscriptionActive(SubscriptionType::RemoveAds))
      return false;
    return PartnerChecker::Instance().IsFakeObject(ft);
  };

  auto overlaysShowStatsFn = [this](list<df::OverlayShowEvent> && events)
  {
    if (events.empty())
      return;

    list<local_ads::Event> statEvents;
    for (auto const & event : events)
    {
      auto const & mwmInfo = event.m_feature.m_mwmId.GetInfo();
      if (!mwmInfo || !m_localAdsManager.HasAds(event.m_feature))
        continue;

      statEvents.emplace_back(local_ads::EventType::ShowPoint,
                              mwmInfo->GetVersion(), mwmInfo->GetCountryName(),
                              event.m_feature.m_index, event.m_zoomLevel, event.m_timestamp,
                              mercator::YToLat(event.m_myPosition.y),
                              mercator::XToLon(event.m_myPosition.x),
                              static_cast<uint16_t>(event.m_gpsAccuracy));
    }
    m_localAdsManager.GetStatistics().RegisterEvents(std::move(statEvents));
  };

  auto onGraphicsContextInitialized = [this]()
  {
    GetPlatform().RunTask(Platform::Thread::Gui, [this]()
    {
      if (m_onGraphicsContextInitialized)
        m_onGraphicsContextInitialized();
    });
  };

  auto isUGCFn = [this](FeatureID const & id)
  {
    auto const ugc = m_ugcApi->GetLoader().GetUGC(id);
    return !ugc.IsEmpty();
  };
  auto isCountryLoadedByNameFn = bind(&Framework::IsCountryLoadedByName, this, _1);
  auto updateCurrentCountryFn = bind(&Framework::OnUpdateCurrentCountry, this, _1, _2);

  bool allow3d;
  bool allow3dBuildings;
  Load3dMode(allow3d, allow3dBuildings);

  auto const isAutozoomEnabled = LoadAutoZoom();
  auto const trafficEnabled = m_trafficManager.IsEnabled();
  auto const isolinesEnabled = m_isolinesManager.IsEnabled();
  auto const guidesEnabled = m_guidesManager.IsEnabled();
  auto const simplifiedTrafficColors = m_trafficManager.HasSimplifiedColorScheme();
  auto const fontsScaleFactor = LoadLargeFontsSize() ? kLargeFontsScaleFactor : 1.0;

  df::DrapeEngine::Params p(
      params.m_apiVersion, contextFactory,
      dp::Viewport(0, 0, params.m_surfaceWidth, params.m_surfaceHeight),
      df::MapDataProvider(move(idReadFn), move(featureReadFn), move(filterFeatureFn),
                          move(isCountryLoadedByNameFn), move(updateCurrentCountryFn)),
      params.m_hints, params.m_visualScale, fontsScaleFactor, move(params.m_widgetsInitInfo),
      make_pair(params.m_initialMyPositionState, params.m_hasMyPositionState),
      move(myPositionModeChangedFn), allow3dBuildings,
      trafficEnabled, isolinesEnabled, guidesEnabled,
      params.m_isChoosePositionMode, params.m_isChoosePositionMode, GetSelectedFeatureTriangles(),
      m_routingManager.IsRoutingActive() && m_routingManager.IsRoutingFollowing(),
      isAutozoomEnabled, simplifiedTrafficColors, move(overlaysShowStatsFn), move(isUGCFn),
      move(onGraphicsContextInitialized));

  m_drapeEngine = make_unique_dp<df::DrapeEngine>(move(p));
  m_drapeEngine->SetModelViewListener([this](ScreenBase const & screen)
  {
    GetPlatform().RunTask(Platform::Thread::Gui, [this, screen](){ OnViewportChanged(screen); });
  });
  m_drapeEngine->SetTapEventInfoListener([this](df::TapInfo const & tapInfo)
  {
    GetPlatform().RunTask(Platform::Thread::Gui, [this, tapInfo]() {
      OnTapEvent(place_page::BuildInfo(tapInfo));
    });
  });
  m_drapeEngine->SetUserPositionListener([this](m2::PointD const & position, bool hasPosition)
  {
    GetPlatform().RunTask(Platform::Thread::Gui, [this, position, hasPosition](){
      OnUserPositionChanged(position, hasPosition);
    });
  });
  m_drapeEngine->SetUserPositionPendingTimeoutListener([this]()
  {
    GetPlatform().RunTask(Platform::Thread::Gui, [this](){
      if (m_myPositionPendingTimeoutListener)
        m_myPositionPendingTimeoutListener();
    });
  });

  OnSize(params.m_surfaceWidth, params.m_surfaceHeight);

  Allow3dMode(allow3d, allow3dBuildings);
  LoadViewport();

  SetVisibleViewport(m2::RectD(0, 0, params.m_surfaceWidth, params.m_surfaceHeight));

  if (m_connectToGpsTrack)
    GpsTracker::Instance().Connect(bind(&Framework::OnUpdateGpsTrackPointsCallback, this, _1, _2));

  GetBookmarkManager().SetDrapeEngine(make_ref(m_drapeEngine));
  m_drapeApi.SetDrapeEngine(make_ref(m_drapeEngine));
  m_routingManager.SetDrapeEngine(make_ref(m_drapeEngine), allow3d);
  m_trafficManager.SetDrapeEngine(make_ref(m_drapeEngine));
  m_transitManager.SetDrapeEngine(make_ref(m_drapeEngine));
  m_isolinesManager.SetDrapeEngine(make_ref(m_drapeEngine));
  m_guidesManager.SetDrapeEngine(make_ref(m_drapeEngine));
  m_localAdsManager.SetDrapeEngine(make_ref(m_drapeEngine));
  m_searchMarks.SetDrapeEngine(make_ref(m_drapeEngine));

  InvalidateUserMarks();

  auto const transitSchemeEnabled = LoadTransitSchemeEnabled();
  m_transitManager.EnableTransitSchemeMode(transitSchemeEnabled);

  // Show debug info if it's enabled in the config.
  bool showDebugInfo;
  if (!settings::Get(kShowDebugInfo, showDebugInfo))
    showDebugInfo = false;
  if (showDebugInfo)
    m_drapeEngine->ShowDebugInfo(showDebugInfo);

  benchmark::RunGraphicsBenchmark(this);
}

void Framework::OnRecoverSurface(int width, int height, bool recreateContextDependentResources)
{
  if (m_drapeEngine)
  {
    m_drapeEngine->RecoverSurface(width, height, recreateContextDependentResources);

    InvalidateUserMarks();

    UpdatePlacePageInfoForCurrentSelection();

    m_drapeApi.Invalidate();
  }

  m_trafficManager.OnRecoverSurface();
  m_transitManager.Invalidate();
  m_isolinesManager.Invalidate();
  m_localAdsManager.Invalidate();
}

void Framework::OnDestroySurface()
{
  m_trafficManager.OnDestroySurface();
}

void Framework::UpdateVisualScale(double vs)
{
  if (m_drapeEngine != nullptr)
    m_drapeEngine->UpdateVisualScale(vs, m_isRenderingEnabled);
}

void Framework::UpdateMyPositionRoutingOffset(bool useDefault, int offsetY)
{
  if (m_drapeEngine != nullptr)
    m_drapeEngine->UpdateMyPositionRoutingOffset(useDefault, offsetY);
}

ref_ptr<df::DrapeEngine> Framework::GetDrapeEngine()
{
  return make_ref(m_drapeEngine);
}

void Framework::DestroyDrapeEngine()
{
  if (m_drapeEngine != nullptr)
  {
    m_drapeApi.SetDrapeEngine(nullptr);
    m_routingManager.SetDrapeEngine(nullptr, false);
    m_trafficManager.SetDrapeEngine(nullptr);
    m_transitManager.SetDrapeEngine(nullptr);
    m_isolinesManager.SetDrapeEngine(nullptr);
    m_guidesManager.SetDrapeEngine(nullptr);
    m_localAdsManager.SetDrapeEngine(nullptr);
    m_searchMarks.SetDrapeEngine(nullptr);
    GetBookmarkManager().SetDrapeEngine(nullptr);

    m_trafficManager.Teardown();
    GpsTracker::Instance().Disconnect();
    m_drapeEngine.reset();
  }
}

void Framework::SetRenderingEnabled(ref_ptr<dp::GraphicsContextFactory> contextFactory)
{
  m_isRenderingEnabled = true;
  if (m_drapeEngine)
    m_drapeEngine->SetRenderingEnabled(contextFactory);
}

void Framework::SetRenderingDisabled(bool destroySurface)
{
  m_isRenderingEnabled = false;
  if (m_drapeEngine)
    m_drapeEngine->SetRenderingDisabled(destroySurface);
}

void Framework::SetGraphicsContextInitializationHandler(df::OnGraphicsContextInitialized && handler)
{
  m_onGraphicsContextInitialized = std::move(handler);
}

void Framework::EnableDebugRectRendering(bool enabled)
{
  if (m_drapeEngine)
    m_drapeEngine->EnableDebugRectRendering(enabled);
}

void Framework::ConnectToGpsTracker()
{
  m_connectToGpsTrack = true;
  if (m_drapeEngine)
  {
    m_drapeEngine->ClearGpsTrackPoints();
    GpsTracker::Instance().Connect(bind(&Framework::OnUpdateGpsTrackPointsCallback, this, _1, _2));
  }
}

void Framework::DisconnectFromGpsTracker()
{
  m_connectToGpsTrack = false;
  GpsTracker::Instance().Disconnect();
  if (m_drapeEngine)
    m_drapeEngine->ClearGpsTrackPoints();
}

void Framework::OnUpdateGpsTrackPointsCallback(vector<pair<size_t, location::GpsTrackInfo>> && toAdd,
                                               pair<size_t, size_t> const & toRemove)
{
  ASSERT(m_drapeEngine.get() != nullptr, ());

  vector<df::GpsTrackPoint> pointsAdd;
  pointsAdd.reserve(toAdd.size());
  for (auto const & ip : toAdd)
  {
    df::GpsTrackPoint pt;
    ASSERT_LESS(ip.first, static_cast<size_t>(numeric_limits<uint32_t>::max()), ());
    pt.m_id = static_cast<uint32_t>(ip.first);
    pt.m_speedMPS = ip.second.m_speed;
    pt.m_timestamp = ip.second.m_timestamp;
    pt.m_point = mercator::FromLatLon(ip.second.m_latitude, ip.second.m_longitude);
    pointsAdd.emplace_back(pt);
  }

  vector<uint32_t> indicesRemove;
  if (toRemove.first != GpsTrack::kInvalidId)
  {
    ASSERT_LESS_OR_EQUAL(toRemove.first, toRemove.second, ());
    indicesRemove.reserve(toRemove.second - toRemove.first + 1);
    for (size_t i = toRemove.first; i <= toRemove.second; ++i)
      indicesRemove.emplace_back(i);
  }

  m_drapeEngine->UpdateGpsTrackPoints(move(pointsAdd), move(indicesRemove));
}

void Framework::MarkMapStyle(MapStyle mapStyle)
{
  ASSERT_NOT_EQUAL(mapStyle, MapStyle::MapStyleMerged, ());

  // Store current map style before classificator reloading
  string mapStyleStr = MapStyleToString(mapStyle);
  if (mapStyleStr.empty())
  {
    mapStyle = kDefaultMapStyle;
    mapStyleStr = MapStyleToString(mapStyle);
  }
  settings::Set(kMapStyleKey, mapStyleStr);
  GetStyleReader().SetCurrentStyle(mapStyle);

  alohalytics::TStringMap details {{"mapStyle", mapStyleStr}};
  alohalytics::Stats::Instance().LogEvent("MapStyle_Changed", details);
}

void Framework::SetMapStyle(MapStyle mapStyle)
{
  MarkMapStyle(mapStyle);
  if (m_drapeEngine != nullptr)
    m_drapeEngine->UpdateMapStyle();
  InvalidateUserMarks();
  m_localAdsManager.Invalidate();
  UpdateMinBuildingsTapZoom();
}

MapStyle Framework::GetMapStyle() const
{
  return GetStyleReader().GetCurrentStyle();
}

void Framework::SetupMeasurementSystem()
{
  GetPlatform().SetupMeasurementSystem();

  auto units = measurement_utils::Units::Metric;
  settings::TryGet(settings::kMeasurementUnits, units);

  m_routingManager.SetTurnNotificationsUnits(units);
}

void Framework::SetWidgetLayout(gui::TWidgetsLayoutInfo && layout)
{
  ASSERT(m_drapeEngine != nullptr, ());
  m_drapeEngine->SetWidgetLayout(move(layout));
}

bool Framework::ShowMapForURL(string const & url)
{
  m2::PointD point;
  m2::RectD rect;
  string name;
  ApiMarkPoint const * apiMark = nullptr;

  enum ResultT { FAILED, NEED_CLICK, NO_NEED_CLICK };
  ResultT result = FAILED;

  if (strings::StartsWith(url, "ge0"))
  {
    ge0::Ge0Parser parser;
    ge0::Ge0Parser::Result parseResult;

    if (parser.Parse(url, parseResult))
    {
      point = mercator::FromLatLon(parseResult.m_lat, parseResult.m_lon);
      rect = df::GetRectForDrawScale(parseResult.m_zoomLevel, point);
      name = move(parseResult.m_name);
      result = NEED_CLICK;
    }
  }
  else if (m_parsedMapApi.IsValid())
  {
    if (!m_parsedMapApi.GetViewportRect(rect))
      rect = df::GetWorldRect();

    apiMark = m_parsedMapApi.GetSinglePoint();
    result = apiMark ? NEED_CLICK : NO_NEED_CLICK;
  }
  else  // Actually, we can parse any geo url scheme with correct coordinates.
  {
    url::GeoURLInfo info(url);
    if (info.IsValid())
    {
      point = mercator::FromLatLon(info.m_lat, info.m_lon);
      rect = df::GetRectForDrawScale(info.m_zoom, point);
      result = NEED_CLICK;
    }
  }

  if (result != FAILED)
  {
    // Always hide current map selection.
    DeactivateMapSelection(true /* notifyUI */);

    // Set viewport and stop follow mode.
    StopLocationFollow();
    ShowRect(rect);

    if (result != NO_NEED_CLICK)
    {
      place_page::BuildInfo info;
      info.m_needAnimationOnSelection = false;
      if (apiMark != nullptr)
      {
        info.m_mercator = apiMark->GetPivot();
        info.m_userMarkId = apiMark->GetId();
      }
      else
      {
        info.m_mercator = point;
      }

      m_currentPlacePageInfo = BuildPlacePageInfo(info);
      ActivateMapSelection(m_currentPlacePageInfo);
    }

    return true;
  }

  return false;
}

url_scheme::ParsedMapApi::ParsingResult Framework::ParseAndSetApiURL(string const & url)
{
  using namespace url_scheme;

  // Clear every current API-mark.
  {
    auto editSession = GetBookmarkManager().GetEditSession();
    editSession.ClearGroup(UserMark::Type::API);
    editSession.SetIsVisible(UserMark::Type::API, true);
  }

  auto const result = m_parsedMapApi.SetUrlAndParse(url);

  if (!m_parsedMapApi.GetAffiliateId().empty())
    m_bookingApi->SetAffiliateId(m_parsedMapApi.GetAffiliateId());

  return result;
}

Framework::ParsedRoutingData Framework::GetParsedRoutingData() const
{
  return Framework::ParsedRoutingData(m_parsedMapApi.GetRoutePoints(),
                                      routing::FromString(m_parsedMapApi.GetRoutingType()));
}

url_scheme::SearchRequest Framework::GetParsedSearchRequest() const
{
  return m_parsedMapApi.GetSearchRequest();
}

url_scheme::Subscription Framework::GetParsedSubscription() const
{
  return m_parsedMapApi.GetSubscription();
}

FeatureID Framework::GetFeatureAtPoint(m2::PointD const & mercator,
                                       FeatureMatcher && matcher /* = nullptr */) const
{
  FeatureID fullMatch, poi, line, area;
  auto haveBuilding = false;
  auto closestDistanceToCenter = numeric_limits<double>::max();
  auto currentDistance = numeric_limits<double>::max();
  indexer::ForEachFeatureAtPoint(m_featuresFetcher.GetDataSource(), [&](FeatureType & ft)
  {
    if (fullMatch.IsValid())
      return;

    if (matcher && matcher(ft))
    {
      fullMatch = ft.GetID();
      return;
    }

    switch (ft.GetGeomType())
    {
    case feature::GeomType::Point:
      poi = ft.GetID();
      break;
    case feature::GeomType::Line:
      // Skip/ignore isolines.
      if (ftypes::IsIsolineChecker::Instance()(ft))
        return;
      line = ft.GetID();
      break;
    case feature::GeomType::Area:
      // Buildings have higher priority over other types.
      if (haveBuilding)
        return;
      // Skip/ignore coastlines.
      if (feature::TypesHolder(ft).Has(classif().GetCoastType()))
        return;
      haveBuilding = ftypes::IsBuildingChecker::Instance()(ft);
      currentDistance = mercator::DistanceOnEarth(mercator, feature::GetCenter(ft));
      // Choose the first matching building or, if no buildings are matched,
      // the first among the closest matching non-buildings.
      if (!haveBuilding && currentDistance >= closestDistanceToCenter)
        return;
      area = ft.GetID();
      closestDistanceToCenter = currentDistance;
      break;
    case feature::GeomType::Undefined:
      ASSERT(false, ("case feature::Undefined"));
      break;
    }
  }, mercator);

  return fullMatch.IsValid() ? fullMatch : (poi.IsValid() ? poi : (area.IsValid() ? area : line));
}

osm::MapObject Framework::GetMapObjectByID(FeatureID const & fid) const
{
  osm::MapObject res;
  ASSERT(fid.IsValid(), ());
  FeaturesLoaderGuard guard(m_featuresFetcher.GetDataSource(), fid.m_mwmId);
  auto ft = guard.GetFeatureByIndex(fid.m_index);
  if (ft)
    res.SetFromFeatureType(*ft);
  return res;
}

BookmarkManager & Framework::GetBookmarkManager()
{
  ASSERT(m_bmManager != nullptr, ("Bookmark manager is not initialized."));
  return *m_bmManager.get();
}

BookmarkManager const & Framework::GetBookmarkManager() const
{
  ASSERT(m_bmManager != nullptr, ("Bookmark manager is not initialized."));
  return *m_bmManager.get();
}

void Framework::SetPlacePageListeners(PlacePageEvent::OnOpen const & onOpen,
                                      PlacePageEvent::OnClose const & onClose,
                                      PlacePageEvent::OnUpdate const & onUpdate)
{
  m_onPlacePageOpen = onOpen;
  m_onPlacePageClose = onClose;
  m_onPlacePageUpdate = onUpdate;
}

place_page::Info const & Framework::GetCurrentPlacePageInfo() const
{
  CHECK(HasPlacePageInfo(), ());
  return *m_currentPlacePageInfo;
}

place_page::Info & Framework::GetCurrentPlacePageInfo()
{
  CHECK(HasPlacePageInfo(), ());
  return *m_currentPlacePageInfo;
}

void Framework::ActivateMapSelection(std::optional<place_page::Info> const & info)
{
  if (!info)
    return;

  if (info->GetSelectedObject() == df::SelectionShape::OBJECT_GUIDE)
    StopLocationFollow();

  if (info->GetSelectedObject() == df::SelectionShape::OBJECT_TRACK)
    GetBookmarkManager().OnTrackSelected(info->GetTrackId());
  else
    GetBookmarkManager().OnTrackDeselected();

  // TODO(a): to replace dummies with correct values.
  if (m_currentPlacePageInfo->GetSponsoredType() == place_page::SponsoredType::Booking &&
      info->IsSearchMark())
  {
    m_searchMarks.OnActivate(m_currentPlacePageInfo->GetID());
  }

  CHECK_NOT_EQUAL(info->GetSelectedObject(), df::SelectionShape::OBJECT_EMPTY, ("Empty selections are impossible."));
  if (m_drapeEngine != nullptr)
  {
    m_drapeEngine->SelectObject(info->GetSelectedObject(), info->GetMercator(), info->GetID(),
                                info->GetBuildInfo().m_needAnimationOnSelection,
                                info->GetBuildInfo().m_isGeometrySelectionAllowed);
  }

  SetDisplacementMode(DisplacementModeManager::SLOT_MAP_SELECTION,
                      ftypes::IsHotelChecker::Instance()(info->GetTypes()) /* show */);

  if (m_onPlacePageOpen)
    m_onPlacePageOpen();
  else
    LOG(LWARNING, ("m_onPlacePageOpen has not been set up."));
}

void Framework::DeactivateMapSelection(bool notifyUI)
{
  bool const somethingWasAlreadySelected = (m_currentPlacePageInfo.has_value());

  if (notifyUI && m_onPlacePageClose)
    m_onPlacePageClose(!somethingWasAlreadySelected);

  if (somethingWasAlreadySelected)
  {
    DeactivateHotelSearchMark();
    GetBookmarkManager().OnTrackDeselected();

    m_currentPlacePageInfo = {};

    if (m_drapeEngine != nullptr)
      m_drapeEngine->DeselectObject();
  }

  SetDisplacementMode(DisplacementModeManager::SLOT_MAP_SELECTION, false /* show */);
}

void Framework::InvalidateUserMarks()
{
  GetBookmarkManager().GetEditSession();
}

void Framework::DeactivateHotelSearchMark()
{
  if (m_currentPlacePageInfo && m_currentPlacePageInfo->GetHotelType() &&
      m_currentPlacePageInfo->IsSearchMark())
  {
    if (GetSearchAPI().IsViewportSearchActive())
      m_searchMarks.OnDeactivate(m_currentPlacePageInfo->GetID());
    else
      GetBookmarkManager().GetEditSession().ClearGroup(UserMark::Type::SEARCH);
  }
}

void Framework::OnTapEvent(place_page::BuildInfo const & buildInfo)
{
  using place_page::SponsoredType;

  auto placePageInfo = BuildPlacePageInfo(buildInfo);

  if (placePageInfo)
  {
    auto const prevTrackId = m_currentPlacePageInfo ? m_currentPlacePageInfo->GetTrackId()
                                                    : kml::kInvalidTrackId;
    auto const prevIsGuide = m_currentPlacePageInfo && m_currentPlacePageInfo->IsGuide();

    DeactivateHotelSearchMark();

    m_currentPlacePageInfo = placePageInfo;

    // Log statistics events.
    {
      auto const ll = m_currentPlacePageInfo->GetLatLon();
      double metersToTap = -1;
      if (m_currentPlacePageInfo->IsMyPosition())
      {
        metersToTap = 0;
      }
      else if (auto const position = GetCurrentPosition())
      {
        auto const tapPoint = mercator::FromLatLon(ll);
        metersToTap = mercator::DistanceOnEarth(*position, tapPoint);
      }

      alohalytics::TStringMap kv = {{"longTap", buildInfo.m_isLongTap ? "1" : "0"},
                                    {"title", m_currentPlacePageInfo->GetTitle()},
                                    {"bookmark", m_currentPlacePageInfo->IsBookmark() ? "1" : "0"},
                                    {"meters", strings::to_string_dac(metersToTap, 0)}};
      if (m_currentPlacePageInfo->IsFeature())
        kv["types"] = DebugPrint(m_currentPlacePageInfo->GetTypes());

      if (m_currentPlacePageInfo->GetSponsoredType() == SponsoredType::Holiday)
      {
        kv["holiday"] = "1";
        auto const & mwmInfo = m_currentPlacePageInfo->GetID().m_mwmId.GetInfo();
        if (mwmInfo)
          kv["mwmVersion"] = strings::to_string(mwmInfo->GetVersion());
      }
      else if (m_currentPlacePageInfo->GetSponsoredType() == SponsoredType::Partner)
      {
        if (!m_currentPlacePageInfo->GetPartnerName().empty())
          kv["partner"] = m_currentPlacePageInfo->GetPartnerName();
      }

      if (m_currentPlacePageInfo->IsRoadType())
        kv["road_warning"] = DebugPrint(m_currentPlacePageInfo->GetRoadType());

      // Older version of statistics used "$GetUserMark" event.
      alohalytics::Stats::Instance().LogEvent("$SelectMapObject", kv,
                                              alohalytics::Location::FromLatLon(ll.m_lat, ll.m_lon));

      // Send realtime statistics about bookmark selection.
      if (m_currentPlacePageInfo->IsBookmark())
      {
        auto const categoryId = m_currentPlacePageInfo->GetBookmarkCategoryId();
        auto const serverId = m_bmManager->GetCategoryServerId(categoryId);
        if (!serverId.empty())
        {
          auto const accessRules = m_bmManager->GetCategoryData(categoryId).m_accessRules;
          if (BookmarkManager::IsGuide(accessRules))
          {
            alohalytics::TStringMap params = {{"server_id", serverId}};
            alohalytics::Stats::Instance().LogEvent("Map_GuideBookmark_open", params, alohalytics::kAllChannels);
          }
        }
      }

      if (m_currentPlacePageInfo->GetSponsoredType() == SponsoredType::Booking)
      {
        GetPlatform().GetMarketingService().SendPushWooshTag(marketing::kBookHotelOnBookingComDiscovered);
        GetPlatform().GetMarketingService().SendMarketingEvent(marketing::kPlacepageHotelBook,
                                                               {{"provider", "booking.com"}});
      }
    }

    if (m_currentPlacePageInfo->GetTrackId() != kml::kInvalidTrackId)
    {
      if (m_currentPlacePageInfo->GetTrackId() == prevTrackId)
      {
        if (m_drapeEngine)
        {
          m_drapeEngine->SelectObject(df::SelectionShape::ESelectedObject::OBJECT_TRACK,
                                      m_currentPlacePageInfo->GetMercator(), FeatureID(),
                                      false /* isAnim */, false /* isGeometrySelectionAllowed */);
        }
        return;
      }
      GetBookmarkManager().UpdateElevationMyPosition(m_currentPlacePageInfo->GetTrackId());
    }

    if (m_currentPlacePageInfo->IsGuide())
    {
      m_guidesManager.LogGuideSelectedStatistic();
      if (prevIsGuide)
      {
        m_guidesManager.OnGuideSelected();
        return;
      }
    }

    ActivateMapSelection(m_currentPlacePageInfo);
  }
  else
  {
    m_guidesManager.ResetActiveGuide();

    bool const somethingWasAlreadySelected = (m_currentPlacePageInfo.has_value());
    alohalytics::Stats::Instance().LogEvent(somethingWasAlreadySelected ? "$DelectMapObject" : "$EmptyTapOnMap");
    // UI is always notified even if empty map is tapped,
    // because empty tap event switches on/off full screen map view mode.
    DeactivateMapSelection(true /* notifyUI */);
  }
}

void Framework::InvalidateRendering()
{
  if (m_drapeEngine)
    m_drapeEngine->Invalidate();
}

void Framework::UpdateMinBuildingsTapZoom()
{
  constexpr int kMinTapZoom = 16;
  m_minBuildingsTapZoom = max(kMinTapZoom,
                              feature::GetDrawableScaleRange(classif().GetTypeByPath({"building"})).first);
}

FeatureID Framework::FindBuildingAtPoint(m2::PointD const & mercator) const
{
  FeatureID featureId;
  if (GetDrawScale() >= m_minBuildingsTapZoom)
  {
    constexpr int kScale = scales::GetUpperScale();
    constexpr double kSelectRectWidthInMeters = 1.1;
    m2::RectD const rect =
        mercator::RectByCenterXYAndSizeInMeters(mercator, kSelectRectWidthInMeters);
    m_featuresFetcher.ForEachFeature(rect, [&](FeatureType & ft)
    {
      if (!featureId.IsValid() &&
        ft.GetGeomType() == feature::GeomType::Area &&
          ftypes::IsBuildingChecker::Instance()(ft) &&
          ft.GetLimitRect(kScale).IsPointInside(mercator) &&
          feature::GetMinDistanceMeters(ft, mercator) == 0.0)
      {
        featureId = ft.GetID();
      }
    }, kScale);
  }
  return featureId;
}

void Framework::BuildTrackPlacePage(BookmarkManager::TrackSelectionInfo const & trackSelectionInfo,
                                    place_page::Info & info)
{
  info.SetSelectedObject(df::SelectionShape::OBJECT_TRACK);
  auto const & track = *GetBookmarkManager().GetTrack(trackSelectionInfo.m_trackId);
  FillTrackInfo(track, trackSelectionInfo.m_trackPoint, info);
  GetBookmarkManager().SetTrackSelectionInfo(trackSelectionInfo, true /* notifyListeners */);
}

void Framework::BuildGuidePlacePage(GuideMark const & guideMark, place_page::Info & info)
{
  m_guidesManager.SetActiveGuide(guideMark.GetGuideId());
  info.SetSelectedObject(df::SelectionShape::OBJECT_GUIDE);
  info.SetIsGuide(true);
}

std::optional<place_page::Info> Framework::BuildPlacePageInfo(
    place_page::BuildInfo const & buildInfo)
{
  place_page::Info outInfo;
  if (m_drapeEngine == nullptr)
    return {};

  outInfo.SetBuildInfo(buildInfo);
  outInfo.SetAdsEngine(m_adsEngine.get());

  if (buildInfo.IsUserMarkMatchingEnabled())
  {
    UserMark const * mark = FindUserMarkInTapPosition(buildInfo);
    if (mark != nullptr)
    {
      outInfo.SetSelectedObject(df::SelectionShape::OBJECT_USER_MARK);
      switch (mark->GetMarkType())
      {
      case UserMark::Type::API:
        FillApiMarkInfo(*static_cast<ApiMarkPoint const *>(mark), outInfo);
        break;
      case UserMark::Type::BOOKMARK:
        FillBookmarkInfo(*static_cast<Bookmark const *>(mark), outInfo);
        break;
      case UserMark::Type::SEARCH:
        FillSearchResultInfo(*static_cast<SearchMarkPoint const *>(mark), outInfo);
        break;
      case UserMark::Type::ROUTING:
        FillRouteMarkInfo(*static_cast<RouteMarkPoint const *>(mark), outInfo);
        break;
      case UserMark::Type::ROAD_WARNING:
        FillRoadTypeMarkInfo(*static_cast<RoadWarningMark const *>(mark), outInfo);
        break;
      case UserMark::Type::TRACK_INFO:
      {
        auto const & infoMark = *static_cast<TrackInfoMark const *>(mark);
        BuildTrackPlacePage(GetBookmarkManager().GetTrackSelectionInfo(infoMark.GetTrackId()),
                            outInfo);
        return outInfo;
      }
      case UserMark::Type::TRACK_SELECTION:
      {
        auto const & selMark = *static_cast<TrackSelectionMark const *>(mark);
        BuildTrackPlacePage(GetBookmarkManager().GetTrackSelectionInfo(selMark.GetTrackId()),
                            outInfo);
        return outInfo;
      }
      case UserMark::Type::GUIDE:
      {
        auto const & guideMark = *static_cast<GuideMark const *>(mark);
        BuildGuidePlacePage(guideMark, outInfo);
        return outInfo;
      }
      case UserMark::Type::GUIDE_CLUSTER:
      {
        auto const & guideClusterMark = *static_cast<GuidesClusterMark const *>(mark);
        m_guidesManager.OnClusterSelected(guideClusterMark, m_currentModelView);
        return {};
      }
      default:
        ASSERT(false, ("FindNearestUserMark returned invalid mark."));
      }
      SetPlacePageLocation(outInfo);

      utils::RegisterEyeEventIfPossible(eye::MapObject::Event::Type::Open, GetCurrentPosition(),
                                        outInfo);
      return outInfo;
    }
  }

  if (buildInfo.m_isMyPosition)
  {
    outInfo.SetSelectedObject(df::SelectionShape::OBJECT_MY_POSITION);
    FillMyPositionInfo(outInfo, buildInfo);
    SetPlacePageLocation(outInfo);
    return outInfo;
  }

  if (!buildInfo.m_postcode.empty())
  {
    outInfo.SetSelectedObject(df::SelectionShape::OBJECT_POI);
    FillPostcodeInfo(buildInfo.m_postcode, buildInfo.m_mercator, outInfo);
    GetBookmarkManager().SelectionMark().SetPtOrg(outInfo.GetMercator());
    SetPlacePageLocation(outInfo);

    utils::RegisterEyeEventIfPossible(eye::MapObject::Event::Type::Open, GetCurrentPosition(),
                                      outInfo);
    return outInfo;
  }

  FeatureID selectedFeature = buildInfo.m_featureId;
  auto const isFeatureMatchingEnabled = buildInfo.IsFeatureMatchingEnabled();

  if (buildInfo.IsTrackMatchingEnabled() && !buildInfo.m_isLongTap &&
      !(isFeatureMatchingEnabled && selectedFeature.IsValid()))
  {
    auto const trackSelectionInfo = FindTrackInTapPosition(buildInfo);
    if (trackSelectionInfo.m_trackId != kml::kInvalidTrackId)
    {
      BuildTrackPlacePage(trackSelectionInfo, outInfo);
      return outInfo;
    }
  }

  if (isFeatureMatchingEnabled && !selectedFeature.IsValid())
    selectedFeature = FindBuildingAtPoint(buildInfo.m_mercator);

  bool showMapSelection = false;
  if (selectedFeature.IsValid())
  {
    FillFeatureInfo(selectedFeature, outInfo);
    if (buildInfo.m_isLongTap)
      outInfo.SetMercator(buildInfo.m_mercator);
    showMapSelection = true;
  }
  else if (buildInfo.m_isLongTap || buildInfo.m_source != place_page::BuildInfo::Source::User)
  {
    if (isFeatureMatchingEnabled)
      FillPointInfo(outInfo, buildInfo.m_mercator, {});
    else
      FillNotMatchedPlaceInfo(outInfo, buildInfo.m_mercator, {});
    showMapSelection = true;
  }

  if (showMapSelection)
  {
    outInfo.SetSelectedObject(df::SelectionShape::OBJECT_POI);
    GetBookmarkManager().SelectionMark().SetPtOrg(outInfo.GetMercator());
    SetPlacePageLocation(outInfo);

    utils::RegisterEyeEventIfPossible(eye::MapObject::Event::Type::Open, GetCurrentPosition(),
                                      outInfo);
    return outInfo;
  }

  return {};
}

void Framework::UpdatePlacePageInfoForCurrentSelection(
    std::optional<place_page::BuildInfo> const & overrideInfo)
{
  if (!m_currentPlacePageInfo)
    return;

  m_currentPlacePageInfo = BuildPlacePageInfo(overrideInfo.has_value() ? *overrideInfo :
    m_currentPlacePageInfo->GetBuildInfo());
  if (m_currentPlacePageInfo && m_onPlacePageUpdate)
    m_onPlacePageUpdate();
}

BookmarkManager::TrackSelectionInfo Framework::FindTrackInTapPosition(
    place_page::BuildInfo const & buildInfo) const
{
  auto const & bm = GetBookmarkManager();
  if (buildInfo.m_trackId != kml::kInvalidTrackId)
  {
    if (bm.GetTrack(buildInfo.m_trackId) == nullptr)
      return {};
    auto const selection = bm.GetTrackSelectionInfo(buildInfo.m_trackId);
    CHECK_NOT_EQUAL(selection.m_trackId, kml::kInvalidTrackId, ());
    return selection;
  }
  auto const touchRect = df::TapInfo::GetDefaultTapRect(buildInfo.m_mercator,
                                                           m_currentModelView).GetGlobalRect();
  return bm.FindNearestTrack(touchRect);
}

UserMark const * Framework::FindUserMarkInTapPosition(place_page::BuildInfo const & buildInfo) const
{
  if (buildInfo.m_userMarkId != kml::kInvalidMarkId)
  {
    auto const & bm = GetBookmarkManager();
    auto mark = bm.IsBookmark(buildInfo.m_userMarkId) ? bm.GetBookmark(buildInfo.m_userMarkId)
                                                      : bm.GetUserMark(buildInfo.m_userMarkId);
    if (mark != nullptr)
      return mark;
  }

  UserMark const * mark = GetBookmarkManager().FindNearestUserMark(
    [this, &buildInfo](UserMark::Type type)
    {
      double constexpr kEps = 1e-7;
      if (buildInfo.m_source != place_page::BuildInfo::Source::User)
        return df::TapInfo::GetPreciseTapRect(buildInfo.m_mercator, kEps);

      if (type == UserMark::Type::BOOKMARK || type == UserMark::Type::TRACK_INFO)
        return df::TapInfo::GetBookmarkTapRect(buildInfo.m_mercator, m_currentModelView);

      if (type == UserMark::Type::ROUTING || type == UserMark::Type::ROAD_WARNING)
        return df::TapInfo::GetRoutingPointTapRect(buildInfo.m_mercator, m_currentModelView);

      if (type == UserMark::Type::GUIDE || type == UserMark::Type::GUIDE_CLUSTER)
        return df::TapInfo::GetGuideTapRect(buildInfo.m_mercator, m_currentModelView);

      return df::TapInfo::GetDefaultTapRect(buildInfo.m_mercator, m_currentModelView);
    },
    [](UserMark::Type type)
    {
      return type == UserMark::Type::TRACK_INFO || type == UserMark::Type::TRACK_SELECTION;
    });
  return mark;
}

void Framework::PredictLocation(double & lat, double & lon, double accuracy,
                                double bearing, double speed, double elapsedSeconds)
{
  double offsetInM = speed * elapsedSeconds;
  double angle = base::DegToRad(90.0 - bearing);

  m2::PointD mercatorPt = mercator::MetersToXY(lon, lat, accuracy).Center();
  mercatorPt = mercator::GetSmPoint(mercatorPt, offsetInM * cos(angle), offsetInM * sin(angle));
  lon = mercator::XToLon(mercatorPt.x);
  lat = mercator::YToLat(mercatorPt.y);
}

StringsBundle const & Framework::GetStringsBundle()
{
  return m_stringsBundle;
}

// static
string Framework::CodeGe0url(Bookmark const * bmk, bool addName)
{
  double lat = mercator::YToLat(bmk->GetPivot().y);
  double lon = mercator::XToLon(bmk->GetPivot().x);
  return ge0::GenerateShortShowMapUrl(lat, lon, bmk->GetScale(), addName ? bmk->GetPreferredName() : "");
}

// static
string Framework::CodeGe0url(double lat, double lon, double zoomLevel, string const & name)
{
  return ge0::GenerateShortShowMapUrl(lat, lon, zoomLevel, name);
}

string Framework::GenerateApiBackUrl(ApiMarkPoint const & point) const
{
  string res = m_parsedMapApi.GetGlobalBackUrl();
  if (!res.empty())
  {
    ms::LatLon const ll = point.GetLatLon();
    res += "pin?ll=" + strings::to_string(ll.m_lat) + "," + strings::to_string(ll.m_lon);
    if (!point.GetName().empty())
      res += "&n=" + url::UrlEncode(point.GetName());
    if (!point.GetApiID().empty())
      res += "&id=" + url::UrlEncode(point.GetApiID());
  }
  return res;
}

bool Framework::IsDataVersionUpdated()
{
  int64_t storedVersion;
  if (settings::Get("DataVersion", storedVersion))
  {
    return storedVersion < m_storage.GetCurrentDataVersion();
  }
  // no key in the settings, assume new version
  return true;
}

void Framework::UpdateSavedDataVersion()
{
  settings::Set("DataVersion", m_storage.GetCurrentDataVersion());
}

int64_t Framework::GetCurrentDataVersion() const { return m_storage.GetCurrentDataVersion(); }

dp::ApiVersion Framework::LoadPreferredGraphicsAPI()
{
  std::string apiStr;
  if (settings::Get(kPreferredGraphicsAPI, apiStr))
    return dp::ApiVersionFromString(apiStr);
  return dp::ApiVersionFromString({});
}

void Framework::SavePreferredGraphicsAPI(dp::ApiVersion apiVersion)
{
  settings::Set(kPreferredGraphicsAPI, DebugPrint(apiVersion));
}

void Framework::AllowTransliteration(bool allowTranslit)
{
  Transliteration::Instance().SetMode(allowTranslit ? Transliteration::Mode::Enabled
                                                    : Transliteration::Mode::Disabled);
  InvalidateRect(GetCurrentViewport());
}

bool Framework::LoadTransliteration()
{
  Transliteration::Mode mode;
  if (settings::Get(kTranslitMode, mode))
    return mode == Transliteration::Mode::Enabled;
  return true;
}

void Framework::SaveTransliteration(bool allowTranslit)
{
  settings::Set(kTranslitMode, allowTranslit ? Transliteration::Mode::Enabled
                                             : Transliteration::Mode::Disabled);
}

void Framework::Allow3dMode(bool allow3d, bool allow3dBuildings)
{
  if (m_drapeEngine == nullptr)
    return;

  if (!m_powerManager.IsFacilityEnabled(power_management::Facility::PerspectiveView))
    allow3d = false;

  if (!m_powerManager.IsFacilityEnabled(power_management::Facility::Buildings3d))
    allow3dBuildings = false;

  m_drapeEngine->Allow3dMode(allow3d, allow3dBuildings);
}

void Framework::Save3dMode(bool allow3d, bool allow3dBuildings)
{
  settings::Set(kAllow3dKey, allow3d);
  settings::Set(kAllow3dBuildingsKey, allow3dBuildings);
}

void Framework::Load3dMode(bool & allow3d, bool & allow3dBuildings)
{
  if (!settings::Get(kAllow3dKey, allow3d))
    allow3d = true;

  if (!settings::Get(kAllow3dBuildingsKey, allow3dBuildings))
    allow3dBuildings = true;
}

void Framework::SaveLargeFontsSize(bool isLargeSize)
{
  settings::Set(kLargeFontsSize, isLargeSize);
}

bool Framework::LoadLargeFontsSize()
{
  bool isLargeSize;
  if (!settings::Get(kLargeFontsSize, isLargeSize))
    isLargeSize = false;
  return isLargeSize;
}

void Framework::SetLargeFontsSize(bool isLargeSize)
{
  double const scaleFactor = isLargeSize ? kLargeFontsScaleFactor : 1.0;

  ASSERT(m_drapeEngine.get() != nullptr, ());
  m_drapeEngine->SetFontScaleFactor(scaleFactor);

  InvalidateRect(GetCurrentViewport());
}

bool Framework::LoadTrafficEnabled()
{
  bool enabled;
  if (!settings::Get(kTrafficEnabledKey, enabled))
    enabled = false;
  return enabled;
}

void Framework::SaveTrafficEnabled(bool trafficEnabled)
{
  settings::Set(kTrafficEnabledKey, trafficEnabled);
}

bool Framework::LoadTrafficSimplifiedColors()
{
  bool simplified;
  if (!settings::Get(kTrafficSimplifiedColorsKey, simplified))
    simplified = true;
  return simplified;
}

void Framework::SaveTrafficSimplifiedColors(bool simplified)
{
  settings::Set(kTrafficSimplifiedColorsKey, simplified);
}

bool Framework::LoadAutoZoom()
{
  bool allowAutoZoom;
  if (!settings::Get(kAllowAutoZoom, allowAutoZoom))
    allowAutoZoom = true;
  return allowAutoZoom;
}

void Framework::AllowAutoZoom(bool allowAutoZoom)
{
  routing::RouterType const type = m_routingManager.GetRouter();
  bool const isPedestrianRoute = type == RouterType::Pedestrian;
  bool const isTaxiRoute = type == RouterType::Taxi;

  if (m_drapeEngine != nullptr)
    m_drapeEngine->AllowAutoZoom(allowAutoZoom && !isPedestrianRoute && !isTaxiRoute);
}

void Framework::SaveAutoZoom(bool allowAutoZoom)
{
  settings::Set(kAllowAutoZoom, allowAutoZoom);
}

bool Framework::LoadTransitSchemeEnabled()
{
  bool enabled;
  if (!settings::Get(kTransitSchemeEnabledKey, enabled))
    enabled = false;
  return enabled;
}

void Framework::SaveTransitSchemeEnabled(bool enabled)
{
  settings::Set(kTransitSchemeEnabledKey, enabled);
}

bool Framework::LoadIsolinesEnabled()
{
  bool enabled;
  if (!settings::Get(kIsolinesEnabledKey, enabled))
    enabled = false;
  return enabled;
}

void Framework::SaveIsolinesEnabled(bool enabled)
{
  settings::Set(kIsolinesEnabledKey, enabled);
}

bool Framework::LoadGuidesEnabled()
{
  bool enabled;
  if (!settings::Get(kGuidesEnabledKey, enabled))
    enabled = false;
  return enabled;
}

void Framework::SaveGuidesEnabled(bool enabled) { settings::Set(kGuidesEnabledKey, enabled); }

void Framework::EnableChoosePositionMode(bool enable, bool enableBounds, bool applyPosition,
                                         m2::PointD const & position)
{
  if (m_drapeEngine != nullptr)
  {
    m_drapeEngine->EnableChoosePositionMode(enable,
      enableBounds ? GetSelectedFeatureTriangles() : vector<m2::TriangleD>(), applyPosition, position);
  }
}

discovery::Manager::Params Framework::GetDiscoveryParams(
    discovery::ClientParams && clientParams) const
{
  auto constexpr kRectSideM = 2000.0;
  discovery::Manager::Params p;
  p.m_viewportCenter = GetDiscoveryViewportCenter();
  p.m_viewport = mercator::RectByCenterXYAndSizeInMeters(p.m_viewportCenter, kRectSideM);
  p.m_curency = clientParams.m_currency;
  p.m_lang = clientParams.m_lang;
  p.m_itemsCount = clientParams.m_itemsCount;
  p.m_itemTypes = move(clientParams.m_itemTypes);
  return p;
}

std::string Framework::GetDiscoveryLocalExpertsUrl() const
{
  return m_discoveryManager->GetLocalExpertsUrl(GetDiscoveryViewportCenter());
}

m2::PointD Framework::GetDiscoveryViewportCenter() const
{
  auto const currentPosition = GetCurrentPosition();
  return currentPosition ? *currentPosition : GetViewportCenter();
}

vector<m2::TriangleD> Framework::GetSelectedFeatureTriangles() const
{
  vector<m2::TriangleD> triangles;
  if (!m_currentPlacePageInfo || !m_currentPlacePageInfo->GetID().IsValid())
    return triangles;

  FeaturesLoaderGuard const guard(m_featuresFetcher.GetDataSource(), m_currentPlacePageInfo->GetID().m_mwmId);
  auto ft = guard.GetFeatureByIndex(m_currentPlacePageInfo->GetID().m_index);
  if (!ft)
    return triangles;

  if (ftypes::IsBuildingChecker::Instance()(feature::TypesHolder(*ft)))
  {
    triangles.reserve(10);
    ft->ForEachTriangle([&](m2::PointD const & p1, m2::PointD const & p2, m2::PointD const & p3)
    {
      triangles.emplace_back(p1, p2, p3);
    }, scales::GetUpperScale());
  }

  return triangles;
}

void Framework::BlockTapEvents(bool block)
{
  if (m_drapeEngine != nullptr)
    m_drapeEngine->BlockTapEvents(block);
}

namespace feature
{
string GetPrintableTypes(FeatureType & ft) { return DebugPrint(feature::TypesHolder(ft)); }
uint32_t GetBestType(FeatureType & ft) { return feature::TypesHolder(ft).GetBestType(); }
}

bool Framework::ParseDrapeDebugCommand(string const & query)
{
  MapStyle desiredStyle = MapStyleCount;
  if (query == "?dark" || query == "mapstyle:dark")
    desiredStyle = MapStyleDark;
  else if (query == "?light" || query == "mapstyle:light")
    desiredStyle = MapStyleClear;
  else if (query == "?vdark" || query == "mapstyle:vehicle_dark")
    desiredStyle = MapStyleVehicleDark;
  else if (query == "?vlight" || query == "mapstyle:vehicle_light")
    desiredStyle = MapStyleVehicleClear;

  if (desiredStyle != MapStyleCount)
  {
#if defined(OMIM_OS_ANDROID)
    MarkMapStyle(desiredStyle);
#else
    SetMapStyle(desiredStyle);
#endif
    return true;
  }

  if (query == "?aa" || query == "effect:antialiasing")
  {
    m_drapeEngine->SetPosteffectEnabled(df::PostprocessRenderer::Antialiasing,
                                        true /* enabled */);
    return true;
  }
  if (query == "?no-aa" || query == "effect:no-antialiasing")
  {
    m_drapeEngine->SetPosteffectEnabled(df::PostprocessRenderer::Antialiasing,
                                        false /* enabled */);
    return true;
  }
  if (query == "?ugc")
  {
    m_drapeEngine->EnableUGCRendering(true /* enabled */);
    return true;
  }
  if (query == "?no-ugc")
  {
    m_drapeEngine->EnableUGCRendering(false /* enabled */);
    return true;
  }
  if (query == "?scheme")
  {
    m_transitManager.EnableTransitSchemeMode(true /* enable */);
    return true;
  }
  if (query == "?no-scheme")
  {
    m_transitManager.EnableTransitSchemeMode(false /* enable */);
    return true;
  }
  if (query == "?isolines")
  {
    m_isolinesManager.SetEnabled(true /* enable */);
    return true;
  }
  if (query == "?no-isolines")
  {
    m_isolinesManager.SetEnabled(false /* enable */);
    return true;
  }
  if (query == "?debug-info")
  {
    m_drapeEngine->ShowDebugInfo(true /* shown */);
    return true;
  }
  if (query == "?debug-info-always")
  {
    m_drapeEngine->ShowDebugInfo(true /* shown */);
    settings::Set(kShowDebugInfo, true);
    return true;
  }
  if (query == "?no-debug-info")
  {
    m_drapeEngine->ShowDebugInfo(false /* shown */);
    settings::Set(kShowDebugInfo, false);
    return true;
  }
  if (query == "?debug-rect")
  {
    m_drapeEngine->EnableDebugRectRendering(true /* shown */);
    return true;
  }
  if (query == "?no-debug-rect")
  {
    m_drapeEngine->EnableDebugRectRendering(false /* shown */);
    return true;
  }
#if defined(OMIM_METAL_AVAILABLE)
  if (query == "?metal")
  {
    SavePreferredGraphicsAPI(dp::ApiVersion::Metal);
    return true;
  }
#endif
#if defined(OMIM_OS_ANDROID)
  if (query == "?vulkan")
  {
    SavePreferredGraphicsAPI(dp::ApiVersion::Vulkan);
    return true;
  }
#endif
  if (query == "?gl")
  {
    SavePreferredGraphicsAPI(dp::ApiVersion::OpenGLES3);
    return true;
  }
  return false;
}

bool Framework::ParseEditorDebugCommand(search::SearchParams const & params)
{
  if (params.m_query == "?edits")
  {
    osm::Editor::Stats const stats = osm::Editor::Instance().GetStats();
    search::Results results;
    results.AddResultNoChecks(search::Result("Uploaded: " + strings::to_string(stats.m_uploadedCount), "?edits"));
    for (auto const & edit : stats.m_edits)
    {
      FeatureID const & fid = edit.first;

      FeaturesLoaderGuard guard(m_featuresFetcher.GetDataSource(), fid.m_mwmId);
      auto ft = guard.GetFeatureByIndex(fid.m_index);
      if (!ft)
      {
        LOG(LERROR, ("Feature can't be loaded:", fid));
        return true;
      }

      string name;
      ft->GetReadableName(name);
      feature::TypesHolder const types(*ft);
      search::Result::Details details;
      results.AddResultNoChecks(search::Result(fid, feature::GetCenter(*ft), name, edit.second,
                                               types.GetBestType(), details));
    }
    params.m_onResults(results);

    results.SetEndMarker(false /* isCancelled */);
    params.m_onResults(results);
    return true;
  }
  else if (params.m_query == "?eclear")
  {
    osm::Editor::Instance().ClearAllLocalEdits();
    return true;
  }

  static std::string const kFindFeatureDebugKey = "?fid ";
  if (params.m_query.find(kFindFeatureDebugKey) == 0)
  {
    // Format: ?fid<space>index<space>.
    auto fidStr = params.m_query.substr(kFindFeatureDebugKey.size());
    bool const canSearch = !fidStr.empty() && fidStr.back() == ' ';
    strings::Trim(fidStr);
    uint32_t index = 0;
    if (canSearch && strings::to_uint(fidStr, index))
    {
      bool isShown = false;
      auto const features = FindFeaturesByIndex(index);
      for (auto const & fid : features)
      {
        if (!fid.IsValid())
          continue;

        // Show the first feature on the map.
        if (!isShown)
        {
          ShowFeature(fid);
          isShown = true;
        }

        // Log found features.
        LOG(LINFO, ("Feature found:", fid));
      }
    }
    return true;
  }
  return false;
}

bool Framework::ParseRoutingDebugCommand(search::SearchParams const &)
{
  // This is an example.
  /*
    if (params.m_query == "?speedcams")
    {
      GetRoutingManager().RoutingSession().EnableMyFeature();
      return true;
    }
  */
  return false;
}

namespace
{
WARN_UNUSED_RESULT bool LocalizeStreet(DataSource const & dataSource, FeatureID const & fid,
                                       osm::LocalizedStreet & result)
{
  FeaturesLoaderGuard g(dataSource, fid.m_mwmId);
  auto ft = g.GetFeatureByIndex(fid.m_index);
  if (!ft)
    return false;

  ft->GetName(StringUtf8Multilang::kDefaultCode, result.m_defaultName);
  ft->GetReadableName(result.m_localizedName);
  if (result.m_localizedName == result.m_defaultName)
    result.m_localizedName.clear();
  return true;
}

vector<osm::LocalizedStreet> TakeSomeStreetsAndLocalize(
    vector<search::ReverseGeocoder::Street> const & streets, DataSource const & dataSource)

{
  vector<osm::LocalizedStreet> results;
  // Exact feature street always goes first in Editor UI street list.

  // Reasonable number of different nearby street names to display in UI.
  constexpr size_t kMaxNumberOfNearbyStreetsToDisplay = 8;
  for (auto const & street : streets)
  {
    auto const isDuplicate = find_if(begin(results), end(results),
                                     [&street](osm::LocalizedStreet const & s)
                                     {
                                       return s.m_defaultName == street.m_name ||
                                              s.m_localizedName == street.m_name;
                                     }) != results.end();
    if (isDuplicate)
      continue;

    osm::LocalizedStreet ls;
    if (!LocalizeStreet(dataSource, street.m_id, ls))
      continue;

    results.emplace_back(move(ls));
    if (results.size() >= kMaxNumberOfNearbyStreetsToDisplay)
      break;
  }
  return results;
}

void SetStreet(search::ReverseGeocoder const & coder, DataSource const & dataSource,
               FeatureType & ft, osm::EditableMapObject & emo)
{
  // Get exact feature's street address (if any) from mwm,
  // together with all nearby streets.
  vector<search::ReverseGeocoder::Street> streets;
  coder.GetNearbyStreets(ft, streets);

  string street = coder.GetFeatureStreetName(ft);

  auto localizedStreets = TakeSomeStreetsAndLocalize(streets, dataSource);

  if (!street.empty())
  {
    auto it = find_if(begin(streets), end(streets),
                      [&street](search::ReverseGeocoder::Street const & s)
                      {
                        return s.m_name == street;
                      });

    if (it != end(streets))
    {
      osm::LocalizedStreet ls;
      if (!LocalizeStreet(dataSource, it->m_id, ls))
        ls.m_defaultName = street;

      emo.SetStreet(ls);

      // A street that a feature belongs to should always be in the first place in the list.
      auto it =
          find_if(begin(localizedStreets), end(localizedStreets), [&ls](osm::LocalizedStreet const & rs)
                  {
                    return ls.m_defaultName == rs.m_defaultName;
                  });
      if (it != end(localizedStreets))
        iter_swap(it, begin(localizedStreets));
      else
        localizedStreets.insert(begin(localizedStreets), ls);
    }
    else
    {
      emo.SetStreet({street, ""});
    }
  }
  else
  {
    emo.SetStreet({});
  }

  emo.SetNearbyStreets(move(localizedStreets));
}

void SetHostingBuildingAddress(FeatureID const & hostingBuildingFid, DataSource const & dataSource,
                               search::ReverseGeocoder const & coder, osm::EditableMapObject & emo)
{
  if (!hostingBuildingFid.IsValid())
    return;

  FeaturesLoaderGuard g(dataSource, hostingBuildingFid.m_mwmId);
  auto hostingBuildingFeature = g.GetFeatureByIndex(hostingBuildingFid.m_index);
  if (!hostingBuildingFeature)
    return;

  search::ReverseGeocoder::Address address;
  if (coder.GetExactAddress(*hostingBuildingFeature, address))
  {
    if (emo.GetHouseNumber().empty())
      emo.SetHouseNumber(address.GetHouseNumber());
    if (emo.GetStreet().m_defaultName.empty())
      // TODO(mgsergio): Localize if localization is required by UI.
      emo.SetStreet({address.GetStreetName(), ""});
  }
}
}  // namespace

bool Framework::CanEditMap() const
{
  return !GetStorage().IsDownloadInProgress();
}

bool Framework::CreateMapObject(m2::PointD const & mercator, uint32_t const featureType,
                                osm::EditableMapObject & emo) const
{
  emo = {};
  auto const & dataSource = m_featuresFetcher.GetDataSource();
  MwmSet::MwmId const mwmId = dataSource.GetMwmIdByCountryFile(
        platform::CountryFile(m_infoGetter->GetRegionCountryId(mercator)));
  if (!mwmId.IsAlive())
    return false;

  GetPlatform().GetMarketingService().SendMarketingEvent(marketing::kEditorAddStart, {});

  search::ReverseGeocoder const coder(m_featuresFetcher.GetDataSource());
  vector<search::ReverseGeocoder::Street> streets;

  coder.GetNearbyStreets(mwmId, mercator, streets);
  emo.SetNearbyStreets(TakeSomeStreetsAndLocalize(streets, m_featuresFetcher.GetDataSource()));

  // TODO(mgsergio): Check emo is a poi. For now it is the only option.
  SetHostingBuildingAddress(FindBuildingAtPoint(mercator), dataSource, coder, emo);

  return osm::Editor::Instance().CreatePoint(featureType, mercator, mwmId, emo);
}

bool Framework::GetEditableMapObject(FeatureID const & fid, osm::EditableMapObject & emo) const
{
  if (!fid.IsValid())
    return false;

  FeaturesLoaderGuard guard(m_featuresFetcher.GetDataSource(), fid.m_mwmId);
  auto ft = guard.GetFeatureByIndex(fid.m_index);
  if (!ft)
    return false;

  GetPlatform().GetMarketingService().SendMarketingEvent(marketing::kEditorEditStart, {});

  emo = {};
  emo.SetFromFeatureType(*ft);
  auto const & editor = osm::Editor::Instance();
  emo.SetEditableProperties(editor.GetEditableProperties(*ft));

  auto const & dataSource = m_featuresFetcher.GetDataSource();
  search::ReverseGeocoder const coder(dataSource);
  SetStreet(coder, dataSource, *ft, emo);

  if (!ftypes::IsBuildingChecker::Instance()(*ft) &&
      (emo.GetHouseNumber().empty() || emo.GetStreet().m_defaultName.empty()))
  {
    SetHostingBuildingAddress(FindBuildingAtPoint(feature::GetCenter(*ft)), dataSource, coder, emo);
  }

  return true;
}

osm::Editor::SaveResult Framework::SaveEditedMapObject(osm::EditableMapObject emo)
{
  auto & editor = osm::Editor::Instance();

  ms::LatLon issueLatLon;

  auto shouldNotify = false;
  // Notify if a poi address and it's hosting building address differ.
  do
  {
    auto const isCreatedFeature = editor.IsCreatedFeature(emo.GetID());

    FeaturesLoaderGuard g(m_featuresFetcher.GetDataSource(), emo.GetID().m_mwmId);
    std::unique_ptr<FeatureType> originalFeature;
    if (!isCreatedFeature)
    {
      originalFeature = g.GetOriginalFeatureByIndex(emo.GetID().m_index);
      if (!originalFeature)
        return osm::Editor::SaveResult::NoUnderlyingMapError;
    }
    else
    {
      originalFeature = std::make_unique<FeatureType>(emo);
    }

    // Handle only pois.
    if (ftypes::IsBuildingChecker::Instance()(*originalFeature))
      break;

    auto const hostingBuildingFid = FindBuildingAtPoint(feature::GetCenter(*originalFeature));
    // The is no building to take address from. Fallback to simple saving.
    if (!hostingBuildingFid.IsValid())
      break;

    auto hostingBuildingFeature = g.GetFeatureByIndex(hostingBuildingFid.m_index);
    if (!hostingBuildingFeature)
      break;

    issueLatLon = mercator::ToLatLon(feature::GetCenter(*hostingBuildingFeature));

    search::ReverseGeocoder::Address hostingBuildingAddress;
    search::ReverseGeocoder const coder(m_featuresFetcher.GetDataSource());
    // The is no address to take from a hosting building. Fallback to simple saving.
    if (!coder.GetExactAddress(*hostingBuildingFeature, hostingBuildingAddress))
      break;

    string originalFeatureStreet;
    if (!isCreatedFeature)
    {
      originalFeatureStreet = coder.GetOriginalFeatureStreetName(originalFeature->GetID());
    }
    else
    {
      originalFeatureStreet = emo.GetStreet().m_defaultName;
    }

    auto isStreetOverridden = false;
    if (!hostingBuildingAddress.GetStreetName().empty() &&
        emo.GetStreet().m_defaultName != hostingBuildingAddress.GetStreetName())
    {
      isStreetOverridden = true;
      shouldNotify = true;
    }
    auto isHouseNumberOverridden = false;
    if (!hostingBuildingAddress.GetHouseNumber().empty() &&
        emo.GetHouseNumber() != hostingBuildingAddress.GetHouseNumber())
    {
      isHouseNumberOverridden = true;
      shouldNotify = true;
    }

    // TODO(mgsergio): Consider this:
    // User changed a feature that had no original address and the address he entered
    // is different from the one on the hosting building. He saved it and the changes
    // were uploaded to OSM. Then he set the address from the hosting building back
    // to the feature. As a result this address won't be merged in OSM because emo.SetStreet({})
    // and emo.SetHouseNumber("") will be called in the following code. So OSM ends up
    // with incorrect data.

    // There is (almost) always a street and/or house number set in emo. We must keep them from
    // saving to editor and pushing to OSM if they ware not overidden. To be saved to editor
    // emo is first converted to FeatureType and FeatureType is then saved to a file and editor.
    // To keep street and house number from penetrating to FeatureType we set them to be empty.

    // Do not save street if it was taken from hosting building.
    if ((originalFeatureStreet.empty() || isCreatedFeature) && !isStreetOverridden)
        emo.SetStreet({});
    // Do not save house number if it was taken from hosting building.
    if ((originalFeature->GetHouseNumber().empty() || isCreatedFeature) && !isHouseNumberOverridden)
      emo.SetHouseNumber("");

    if (!isStreetOverridden && !isHouseNumberOverridden)
    {
      // Address was taken from the hosting building of a feature. Nothing to note.
      shouldNotify = false;
      break;
    }

    if (shouldNotify)
    {
      auto editedFeature = editor.GetEditedFeature(emo.GetID());
      string editedFeatureStreet;
      // Such a notification have been already sent. I.e at least one of
      // street of house number should differ in emo and editor.
      shouldNotify =
          !isCreatedFeature &&
          ((editedFeature && !editedFeature->GetHouseNumber().empty() &&
            editedFeature->GetHouseNumber() != emo.GetHouseNumber()) ||
           (editor.GetEditedFeatureStreet(emo.GetID(), editedFeatureStreet) &&
            !editedFeatureStreet.empty() && editedFeatureStreet != emo.GetStreet().m_defaultName));
    }
  } while (0);

  if (shouldNotify)
  {
    // TODO @mgsergio fill with correct NoteProblemType
    editor.CreateNote(emo.GetLatLon(), emo.GetID(), emo.GetTypes(), emo.GetDefaultName(),
                      osm::Editor::NoteProblemType::General,
                      "The address on this POI is different from the building address."
                      " It is either a user's mistake, or an issue in the data. Please"
                      " check this and fix if needed. (This note was created automatically"
                      " without a user's input. Feel free to close it if it's wrong).");
  }

  emo.RemoveNeedlessNames();

  auto const result = osm::Editor::Instance().SaveEditedFeature(emo);

  // Automatically select newly created objects.
  if (!m_currentPlacePageInfo)
  {
    place_page::BuildInfo info;
    info.m_mercator = emo.GetMercator();
    info.m_featureId = emo.GetID();
    m_currentPlacePageInfo = BuildPlacePageInfo(info);
    ActivateMapSelection(m_currentPlacePageInfo);
  }

  return result;
}

void Framework::DeleteFeature(FeatureID const & fid)
{
  osm::Editor::Instance().DeleteFeature(fid);
  UpdatePlacePageInfoForCurrentSelection();
}

osm::NewFeatureCategories Framework::GetEditorCategories() const
{
  return osm::Editor::Instance().GetNewFeatureCategories();
}

bool Framework::RollBackChanges(FeatureID const & fid)
{
  auto & editor = osm::Editor::Instance();
  auto const status = editor.GetFeatureStatus(fid);
  auto const rolledBack = editor.RollBackChanges(fid);
  if (rolledBack)
  {
    if (status == FeatureStatus::Created)
      DeactivateMapSelection(true /* notifyUI */);
    else
      UpdatePlacePageInfoForCurrentSelection();
  }
  return rolledBack;
}

void Framework::CreateNote(osm::MapObject const & mapObject,
                           osm::Editor::NoteProblemType const type, string const & note)
{
  osm::Editor::Instance().CreateNote(mapObject.GetLatLon(), mapObject.GetID(), mapObject.GetTypes(),
                                     mapObject.GetDefaultName(), type, note);
  if (type == osm::Editor::NoteProblemType::PlaceDoesNotExist)
    DeactivateMapSelection(true /* notifyUI */);
}

storage::CountriesVec Framework::GetTopmostCountries(ms::LatLon const & latlon) const
{
  m2::PointD const point = mercator::FromLatLon(latlon);
  auto const countryId = m_infoGetter->GetRegionCountryId(point);
  storage::CountriesVec topmostCountryIds;
  GetStorage().GetTopmostNodesFor(countryId, topmostCountryIds);
  return topmostCountryIds;
}

namespace
{
vector<dp::Color> colorList = {
    dp::Color(255, 0, 0, 255),   dp::Color(0, 255, 0, 255),   dp::Color(0, 0, 255, 255),
    dp::Color(255, 255, 0, 255), dp::Color(0, 255, 255, 255), dp::Color(255, 0, 255, 255),
    dp::Color(100, 0, 0, 255),   dp::Color(0, 100, 0, 255),   dp::Color(0, 0, 100, 255),
    dp::Color(100, 100, 0, 255), dp::Color(0, 100, 100, 255), dp::Color(100, 0, 100, 255)};

dp::Color const cityBoundaryBBColor = dp::Color(255, 0, 0, 255);
dp::Color const cityBoundaryCBColor = dp::Color(0, 255, 0, 255);
dp::Color const cityBoundaryDBColor = dp::Color(0, 0, 255, 255);

template <class Box>
void DrawLine(Box const & box, dp::Color const & color, df::DrapeApi & drapeApi, string const & id)
{
  auto points = box.Points();
  CHECK(!points.empty(), ());
  points.push_back(points.front());

  points.erase(unique(points.begin(), points.end(), [](m2::PointD const & p1, m2::PointD const & p2) {
    m2::PointD const delta = p2 - p1;
    return delta.IsAlmostZero();
  }), points.end());

  if (points.size() <= 1)
    return;

  drapeApi.AddLine(id, df::DrapeApiLineData(points, color).Width(3.0f).ShowPoints(true).ShowId());
}

void VisualizeFeatureInRect(m2::RectD const & rect, FeatureType & ft, df::DrapeApi & drapeApi)
{
  static uint64_t counter = 0;
  bool allPointsOutside = true;
  vector<m2::PointD> points;
  ft.ForEachPoint([&points, &rect, &allPointsOutside](m2::PointD const & pt)
                  {
                    if (rect.IsPointInside(pt))
                      allPointsOutside = false;
                    points.push_back(pt);
                  }, scales::GetUpperScale());

  if (!allPointsOutside)
  {
    size_t const colorIndex = counter % colorList.size();
    // Note. The first param at DrapeApi::AddLine() should be unique. Other way last added line
    // replaces the previous added line with the same name.
    // As a consequence VisualizeFeatureInRect() should be applied to single mwm. Other way
    // feature ids will be dubbed.
    drapeApi.AddLine(
        strings::to_string(ft.GetID().m_index),
        df::DrapeApiLineData(points, colorList[colorIndex]).Width(3.0f).ShowPoints(true).ShowId());
    counter++;
  }
}
}  // namespace

std::vector<std::string> Framework::GetRegionsCountryIdByRect(m2::RectD const & rect, bool rough) const
{
  return m_infoGetter->GetRegionsCountryIdByRect(rect, rough);
}

void Framework::VisualizeRoadsInRect(m2::RectD const & rect)
{
  m_featuresFetcher.ForEachFeature(rect, [this, &rect](FeatureType & ft)
  {
    if (routing::IsRoad(feature::TypesHolder(ft)))
      VisualizeFeatureInRect(rect, ft, m_drapeApi);
  }, scales::GetUpperScale());
}

void Framework::VisualizeCityBoundariesInRect(m2::RectD const & rect)
{
  search::CitiesBoundariesTable table(GetDataSource());
  table.Load();

  vector<uint32_t> featureIds;
  GetCityBoundariesInRectForTesting(table, rect, featureIds);

  FeaturesLoaderGuard loader(GetDataSource(), GetDataSource().GetMwmIdByCountryFile(CountryFile("World")));
  for (auto const fid : featureIds)
  {
    search::CitiesBoundariesTable::Boundaries boundaries;
    table.Get(fid, boundaries);

    string id = "fid:" + strings::to_string(fid);
    auto ft = loader.GetFeatureByIndex(fid);
    if (ft)
    {
      string name;
      ft->GetName(StringUtf8Multilang::kDefaultCode, name);
      id += ", name:" + name;
    }

    size_t const boundariesSize = boundaries.GetBoundariesForTesting().size();
    for (size_t i = 0; i < boundariesSize; ++i)
    {
      string idWithIndex = id;
      auto const & cityBoundary = boundaries.GetBoundariesForTesting()[i];
      if (boundariesSize > 1)
        idWithIndex = id + " , i:" + strings::to_string(i);

      DrawLine(cityBoundary.m_bbox, cityBoundaryBBColor, m_drapeApi, idWithIndex + ", bb");
      DrawLine(cityBoundary.m_cbox, cityBoundaryCBColor, m_drapeApi, idWithIndex + ", cb");
      DrawLine(cityBoundary.m_dbox, cityBoundaryDBColor, m_drapeApi, idWithIndex + ", db");
    }
  }
}

void Framework::VisualizeCityRoadsInRect(m2::RectD const & rect)
{
  std::map<MwmSet::MwmId, unique_ptr<CityRoads>> cityRoads;
  GetDataSource().ForEachInRect(
      [this, &rect, &cityRoads](FeatureType & ft) {
        if (ft.GetGeomType() != feature::GeomType::Line)
          return;

        auto const & mwmId = ft.GetID().m_mwmId;
        auto const it = cityRoads.find(mwmId);
        if (it == cityRoads.cend())
        {
          MwmSet::MwmHandle handle = m_featuresFetcher.GetDataSource().GetMwmHandleById(mwmId);
          if (!handle.IsAlive())
            return;

          cityRoads[mwmId] = LoadCityRoads(GetDataSource(), handle);
        }

        if (!cityRoads[mwmId]->IsCityRoad(ft.GetID().m_index))
          return;  // ft is not a city road.

        VisualizeFeatureInRect(rect, ft, m_drapeApi);
      },
      rect, scales::GetUpperScale());
}

ads::Engine const & Framework::GetAdsEngine() const
{
  ASSERT(m_adsEngine, ());
  return *m_adsEngine;
}

void Framework::DisableAdProvider(ads::Banner::Type const type, ads::Banner::Place const place)
{
  ASSERT(m_adsEngine, ());
  m_adsEngine.get()->DisableAdProvider(type, place);
}

void Framework::RunUITask(function<void()> fn)
{
  GetPlatform().RunTask(Platform::Thread::Gui, move(fn));
}

void Framework::SetSearchDisplacementModeEnabled(bool enabled)
{
  SetDisplacementMode(DisplacementModeManager::SLOT_INTERACTIVE_SEARCH, enabled /* show */);
}

void Framework::ShowViewportSearchResults(search::Results::ConstIter begin,
                                          search::Results::ConstIter end, bool clear)
{
  FillSearchResultsMarks(begin, end, clear,
                         Framework::SearchMarkPostProcessing());
}

void Framework::ShowViewportSearchResults(search::Results::ConstIter begin,
                                          search::Results::ConstIter end, bool clear,
                                          booking::filter::Types types)
{
  search::Results results;
  results.AddResultsNoChecks(begin, end);

  ShowViewportSearchResults(results, clear, types);
}

void Framework::ShowViewportSearchResults(search::Results const & results, bool clear,
                                          booking::filter::Types types)
{
  using booking::filter::Type;
  using booking::filter::CachedResults;

  ASSERT(!types.empty(), ());

  auto const fillCallback = [this, clear, results](CachedResults filtersResults)
  {
    string const reason = platform::GetLocalizedString("booking_map_component_availability");
    auto const postProcessing = [this, filtersResults = move(filtersResults), &reason](SearchMarkPoint & mark)
    {
      auto const & id = mark.GetFeatureID();

      if (!id.IsValid())
        return;

      booking::PriceFormatter formatter;
      for (auto const & filterResult : filtersResults)
      {
        auto const & features = filterResult.m_passedFilter;
        ASSERT(is_sorted(features.cbegin(), features.cend()), ());

        auto const it = lower_bound(features.cbegin(), features.cend(), id);

        auto const found = it != features.cend() && *it == id;
        switch (filterResult.m_type)
        {
        case Type::Deals:
          mark.SetSale(found);
          break;
        case Type::Availability:
          mark.SetPreparing(!found);

          if (found && !filterResult.m_extras.empty())
          {
            auto const index = distance(features.cbegin(), it);
            auto price = formatter.Format(filterResult.m_extras[index].m_price,
                                          filterResult.m_extras[index].m_currency);
            mark.SetPrice(move(price));
          }

          if (!found)
          {
            auto const & filteredOut = filterResult.m_filteredOut;
            ASSERT(is_sorted(filteredOut.cbegin(), filteredOut.cend()), ());

            auto const isFilteredOut = binary_search(filteredOut.cbegin(), filteredOut.cend(), id);

            if (isFilteredOut)
              m_searchMarks.AppendUnavailable(mark.GetFeatureID(), reason);
          }

          break;
        }
      }
    };

    FillSearchResultsMarks(results.begin(), results.end(), clear,
                           postProcessing);
  };

  m_bookingFilterProcessor.GetFeaturesFromCache(types, results, fillCallback);
}

void Framework::ClearViewportSearchResults()
{
  m_searchMarks.ClearUsed();
  m_searchMarks.ClearUnavailable();
  GetBookmarkManager().GetEditSession().ClearGroup(UserMark::Type::SEARCH);
}

std::optional<m2::PointD> Framework::GetCurrentPosition() const
{
  auto const & myPosMark = GetBookmarkManager().MyPositionMark();
  if (!myPosMark.HasPosition())
    return {};
  return myPosMark.GetPivot();
}

bool Framework::ParseSearchQueryCommand(search::SearchParams const & params)
{
  if (ParseDrapeDebugCommand(params.m_query))
    return true;
  if (ParseSetGpsTrackMinAccuracyCommand(params.m_query))
    return true;
  if (ParseEditorDebugCommand(params))
    return true;
  if (ParseRoutingDebugCommand(params))
    return true;
  return false;
}

search::ProductInfo Framework::GetProductInfo(search::Result const & result) const
{
  ASSERT(m_ugcApi, ());
  if (result.GetResultType() != search::Result::Type::Feature)
    return {};

  search::ProductInfo productInfo;

  productInfo.m_isLocalAdsCustomer = m_localAdsManager.HasVisualization(result.GetFeatureID());

  auto const ugc = m_ugcApi->GetLoader().GetUGC(result.GetFeatureID());
  productInfo.m_ugcRating = ugc.m_totalRating;

  return productInfo;
}

double Framework::GetMinDistanceBetweenResults() const
{
  return m_searchMarks.GetMaxDimension(m_currentModelView);
}

vector<MwmSet::MwmId> Framework::GetMwmsByRect(m2::RectD const & rect, bool rough) const
{
  vector<MwmSet::MwmId> result;
  if (!m_infoGetter)
    return result;

  auto countryIds = m_infoGetter->GetRegionsCountryIdByRect(rect, rough);
  for (auto const & countryId : countryIds)
    result.push_back(GetMwmIdByName(countryId));

  return result;
}

MwmSet::MwmId Framework::GetMwmIdByName(string const & name) const
{
  return m_featuresFetcher.GetDataSource().GetMwmIdByCountryFile(platform::CountryFile(name));
}

vector<FeatureID> Framework::FindFeaturesByIndex(uint32_t featureIndex) const
{
  vector<FeatureID> result;
  auto mwms = GetMwmsByRect(m_currentModelView.ClipRect(), false /* rough */);
  set<MwmSet::MwmId> s(mwms.begin(), mwms.end());

  vector<shared_ptr<LocalCountryFile>> maps;
  m_storage.GetLocalMaps(maps);
  for (auto const & localFile : maps)
  {
    auto mwmId = GetMwmIdByName(localFile->GetCountryName());
    if (s.find(mwmId) != s.end())
      continue;

    if (mwmId.IsAlive())
      mwms.push_back(move(mwmId));
  }

  for (auto const & mwmId : mwms)
  {
    FeaturesLoaderGuard const guard(m_featuresFetcher.GetDataSource(), mwmId);
    if (featureIndex < guard.GetNumFeatures() && guard.GetFeatureByIndex(featureIndex))
      result.emplace_back(mwmId, featureIndex);
  }
  return result;
}

void Framework::ReadFeatures(function<void(FeatureType &)> const & reader,
                             vector<FeatureID> const & features)
{
  m_featuresFetcher.ReadFeatures(reader, features);
}

// RoutingManager::Delegate
void Framework::OnRouteFollow(routing::RouterType type)
{
  bool const isPedestrianRoute = type == RouterType::Pedestrian;
  bool const enableAutoZoom = isPedestrianRoute ? false : LoadAutoZoom();
  int const scale =
      isPedestrianRoute ? scales::GetPedestrianNavigationScale() : scales::GetNavigationScale();
  int scale3d =
      isPedestrianRoute ? scales::GetPedestrianNavigation3dScale() : scales::GetNavigation3dScale();
  if (enableAutoZoom)
    ++scale3d;

  bool const isBicycleRoute = type == RouterType::Bicycle;
  if ((isPedestrianRoute || isBicycleRoute) && LoadTrafficEnabled())
  {
    m_trafficManager.SetEnabled(false /* enabled */);
    SaveTrafficEnabled(false /* enabled */);
  }
  // TODO. We need to sync two enums VehicleType and RouterType to be able to pass
  // GetRoutingSettings(type).m_matchRoute to the FollowRoute() instead of |isPedestrianRoute|.
  // |isArrowGlued| parameter fully corresponds to |m_matchRoute| in RoutingSettings.
  m_drapeEngine->FollowRoute(scale, scale3d, enableAutoZoom, !isPedestrianRoute /* isArrowGlued */);
}

// RoutingManager::Delegate
void Framework::RegisterCountryFilesOnRoute(shared_ptr<routing::NumMwmIds> ptr) const
{
  m_storage.ForEachCountryFile(
      [&ptr](platform::CountryFile const & file) { ptr->RegisterFile(file); });
}

void Framework::InitCityFinder()
{
  ASSERT(!m_cityFinder, ());

  m_cityFinder = make_unique<search::CityFinder>(m_featuresFetcher.GetDataSource());
}

void Framework::InitTaxiEngine()
{
  ASSERT(!m_taxiEngine, ());
  ASSERT(m_infoGetter, ());
  ASSERT(m_cityFinder, ());

  m_taxiEngine = std::make_unique<taxi::Engine>();

  m_taxiEngine->SetDelegate(
      std::make_unique<TaxiDelegate>(GetStorage(), *m_infoGetter, *m_cityFinder));
}

void Framework::SetPlacePageLocation(place_page::Info & info)
{
  ASSERT(m_infoGetter, ());

  if (info.GetCountryId().empty())
    info.SetCountryId(m_infoGetter->GetRegionCountryId(info.GetMercator()));

  CountriesVec countries;
  if (info.GetTopmostCountryIds().empty())
  {
    GetStorage().GetTopmostNodesFor(info.GetCountryId(), countries);
    info.SetTopmostCountryIds(move(countries));
  }
}

void Framework::FillLocalExperts(FeatureType & ft, place_page::Info & info) const
{
  if (GetDrawScale() > scales::GetUpperWorldScale() ||
      !ftypes::IsCityChecker::Instance()(ft))
  {
    info.SetLocalsStatus(place_page::LocalsStatus::NotAvailable);
    return;
  }

  info.SetLocalsStatus(place_page::LocalsStatus::Available);
  info.SetLocalsPageUrl(locals::Api::GetLocalsPageUrl());
}

void Framework::FillDescription(FeatureType & ft, place_page::Info & info) const
{
  if (!ft.GetID().m_mwmId.IsAlive())
    return;
  auto const & regionData = ft.GetID().m_mwmId.GetInfo()->GetRegionData();
  auto const deviceLang = StringUtf8Multilang::GetLangIndex(languages::GetCurrentNorm());
  auto const langPriority = feature::GetDescriptionLangPriority(regionData, deviceLang);

  std::string description;
  if (m_descriptionsLoader->GetDescription(ft.GetID(), langPriority, description))
  {
    info.SetDescription(std::move(description));
    info.SetOpeningMode(m_routingManager.IsRoutingActive()
                        ? place_page::OpeningMode::Preview
                        : place_page::OpeningMode::PreviewPlus);
  }
}

void Framework::UploadUGC(User::CompleteUploadingHandler const & onCompleteUploading)
{
  if (GetPlatform().ConnectionStatus() == Platform::EConnectionType::CONNECTION_NONE ||
      !m_user.IsAuthenticated())
  {
    if (onCompleteUploading != nullptr)
      onCompleteUploading(false);

    return;
  }

  m_ugcApi->GetUGCToSend([this, onCompleteUploading](string && json, size_t numberOfUnsynchronized)
  {
    if (!json.empty())
    {
      m_user.UploadUserReviews(std::move(json), numberOfUnsynchronized,
                               [this, onCompleteUploading](bool isSuccessful)
      {
        if (onCompleteUploading != nullptr)
          onCompleteUploading(isSuccessful);

        if (isSuccessful)
          m_ugcApi->SendingCompleted();
      });
    }
    else
    {
      if (onCompleteUploading != nullptr)
        onCompleteUploading(true);
    }
  });
}

void Framework::GetUGC(FeatureID const & id, ugc::Api::UGCCallback const & callback)
{
  m_ugcApi->GetUGC(id, [this, callback](ugc::UGC const & ugc, ugc::UGCUpdate const & update)
  {
    ugc::UGC filteredUGC = ugc;
    filteredUGC.m_reviews = FilterUGCReviews(ugc.m_reviews);
    std::sort(filteredUGC.m_reviews.begin(), filteredUGC.m_reviews.end(),
              [](ugc::Review const & r1, ugc::Review const & r2)
    {
      return r1.m_time > r2.m_time;
    });
    callback(filteredUGC, update);
  });
}

ugc::Reviews Framework::FilterUGCReviews(ugc::Reviews const & reviews) const
{
  ugc::Reviews result;
  auto const details = m_user.GetDetails();
  ASSERT(std::is_sorted(details.m_reviewIds.begin(), details.m_reviewIds.end()), ());
  for (auto const & review : reviews)
  {
    if (!std::binary_search(details.m_reviewIds.begin(), details.m_reviewIds.end(), review.m_id))
      result.push_back(review);
  }
  return result;
}

void Framework::FilterResultsForHotelsQuery(booking::filter::Tasks const & filterTasks,
                                            search::Results const & results, bool inViewport)
{
  auto tasksInternal = booking::MakeInternalTasks(results, filterTasks, m_searchMarks, inViewport);
  m_bookingFilterProcessor.ApplyFilters(results, move(tasksInternal), filterTasks.GetMode());
}

void Framework::FilterHotels(booking::filter::Tasks const & filterTasks,
                             vector<FeatureID> && featureIds)
{
  auto tasksInternal = booking::MakeInternalTasks(featureIds, filterTasks, m_searchMarks);
  m_bookingFilterProcessor.ApplyFilters(move(featureIds), move(tasksInternal),
                                        filterTasks.GetMode());
}

void Framework::OnBookingFilterParamsUpdate(booking::filter::Tasks const & filterTasks)
{
  m_activeFilters.clear();
  for (auto const & task : filterTasks)
  {
    if (task.m_type == booking::filter::Type::Availability)
      m_bookingAvailabilityParams.Set(*task.m_filterParams.m_apiParams);

    m_activeFilters.push_back(task.m_type);
    m_bookingFilterProcessor.OnParamsUpdated(task.m_type, task.m_filterParams.m_apiParams);
  }
}

booking::AvailabilityParams Framework::GetLastBookingAvailabilityParams() const
{
  return m_bookingAvailabilityParams;
}

TipsApi const & Framework::GetTipsApi() const
{
  return m_tipsApi;
}

bool Framework::HaveTransit(m2::PointD const & pt) const
{
  auto const & dataSource = m_featuresFetcher.GetDataSource();
  MwmSet::MwmId const mwmId =
      dataSource.GetMwmIdByCountryFile(platform::CountryFile(m_infoGetter->GetRegionCountryId(pt)));

  MwmSet::MwmHandle handle = m_featuresFetcher.GetDataSource().GetMwmHandleById(mwmId);
  if (!handle.IsAlive())
    return false;

  return handle.GetValue()->m_cont.IsExist(TRANSIT_FILE_TAG);
}

double Framework::GetLastBackgroundTime() const
{
  return m_startBackgroundTime;
}

bool Framework::MakePlacePageForNotification(NotificationCandidate const & notification)
{
  if (notification.GetType() != NotificationCandidate::Type::UgcReview)
    return false;

  m2::RectD rect = mercator::RectByCenterXYAndOffset(notification.GetPos(), kMwmPointAccuracy);
  bool found = false;

  m_featuresFetcher.GetDataSource().ForEachInRect(
      [this, &notification, &found](FeatureType & ft) {
        auto const featureCenter = feature::GetCenter(ft);
        if (found || !featureCenter.EqualDxDy(notification.GetPos(), kMwmPointAccuracy))
          return;

        auto const & editor = osm::Editor::Instance();
        auto const foundMapObject = utils::MakeEyeMapObject(ft, editor);
        if (!foundMapObject.IsEmpty() && notification.IsSameMapObject(foundMapObject))
        {
          place_page::BuildInfo buildInfo;
          buildInfo.m_mercator = featureCenter;
          buildInfo.m_featureId = ft.GetID();
          m_currentPlacePageInfo = BuildPlacePageInfo(buildInfo);
          if (m_currentPlacePageInfo)
          {
            ActivateMapSelection(m_currentPlacePageInfo);
            found = true;
          }
        }
      },
      rect, scales::GetUpperScale());

  return found;
}

void Framework::OnPowerFacilityChanged(power_management::Facility const facility, bool enabled)
{
  if (facility == power_management::Facility::PerspectiveView ||
      facility == power_management::Facility::Buildings3d)
  {
    bool allow3d = true, allow3dBuildings = true;
    Load3dMode(allow3d, allow3dBuildings);

    if (facility == power_management::Facility::PerspectiveView)
      allow3d = allow3d && enabled;
    else
      allow3dBuildings = allow3dBuildings && enabled;

    Allow3dMode(allow3d, allow3dBuildings);
  }
  else if (facility == power_management::Facility::TrafficJams)
  {
    auto trafficState = enabled && LoadTrafficEnabled();
    if (trafficState == GetTrafficManager().IsEnabled())
      return;

    GetTrafficManager().SetEnabled(trafficState);
  }
}

void Framework::OnPowerSchemeChanged(power_management::Scheme const actualScheme)
{
  if (actualScheme == power_management::Scheme::EconomyMaximum && GetTrafficManager().IsEnabled())
    GetTrafficManager().SetEnabled(false);
}

notifications::NotificationManager & Framework::GetNotificationManager()
{
  return m_notificationManager;
}
