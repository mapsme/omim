#pragma once

#include "drape_frontend/backend_renderer.hpp"
#include "drape_frontend/color_constants.hpp"
#include "drape_frontend/drape_hints.hpp"
#include "drape_frontend/frontend_renderer.hpp"
#include "drape_frontend/route_shape.hpp"
#include "drape_frontend/overlays_tracker.hpp"
#include "drape_frontend/postprocess_renderer.hpp"
#include "drape_frontend/scenario_manager.hpp"
#include "drape_frontend/selection_shape.hpp"
#include "drape_frontend/threads_commutator.hpp"

#include "drape/drape_global.hpp"
#include "drape/pointers.hpp"
#include "drape/texture_manager.hpp"
#include "drape/viewport.hpp"

#include "traffic/traffic_info.hpp"

#include "platform/location.hpp"

#include "geometry/polyline2d.hpp"
#include "geometry/screenbase.hpp"
#include "geometry/triangle2d.hpp"

#include "base/strings_bundle.hpp"

#include <functional>
#include <mutex>
#include <vector>

namespace dp
{
class OGLContextFactory;
}  // namespace dp

namespace df
{
class UserMarksProvider;
class MapDataProvider;

class DrapeEngine
{
public:
  struct Params
  {
    Params(dp::ApiVersion apiVersion,
           ref_ptr<dp::OGLContextFactory> factory,
           ref_ptr<StringsBundle> stringBundle,
           dp::Viewport const & viewport,
           MapDataProvider const & model,
           Hints const & hints,
           double vs,
           double fontsScaleFactor,
           gui::TWidgetsInitInfo && info,
           pair<location::EMyPositionMode, bool> const & initialMyPositionMode,
           location::TMyPositionModeChanged && myPositionModeChanged,
           bool allow3dBuildings,
           bool trafficEnabled,
           bool blockTapEvents,
           bool showChoosePositionMark,
           std::vector<m2::TriangleD> && boundAreaTriangles,
           bool isRoutingActive,
           bool isAutozoomEnabled,
           bool simplifiedTrafficColors,
           OverlaysShowStatsCallback && overlaysShowStatsCallback)
      : m_apiVersion(apiVersion)
      , m_factory(factory)
      , m_stringsBundle(stringBundle)
      , m_viewport(viewport)
      , m_model(model)
      , m_hints(hints)
      , m_vs(vs)
      , m_fontsScaleFactor(fontsScaleFactor)
      , m_info(std::move(info))
      , m_initialMyPositionMode(initialMyPositionMode)
      , m_myPositionModeChanged(std::move(myPositionModeChanged))
      , m_allow3dBuildings(allow3dBuildings)
      , m_trafficEnabled(trafficEnabled)
      , m_blockTapEvents(blockTapEvents)
      , m_showChoosePositionMark(showChoosePositionMark)
      , m_boundAreaTriangles(std::move(boundAreaTriangles))
      , m_isRoutingActive(isRoutingActive)
      , m_isAutozoomEnabled(isAutozoomEnabled)
      , m_simplifiedTrafficColors(simplifiedTrafficColors)
      , m_overlaysShowStatsCallback(std::move(overlaysShowStatsCallback))
    {}

    dp::ApiVersion m_apiVersion;
    ref_ptr<dp::OGLContextFactory> m_factory;
    ref_ptr<StringsBundle> m_stringsBundle;
    dp::Viewport m_viewport;
    MapDataProvider m_model;
    Hints m_hints;
    double m_vs;
    double m_fontsScaleFactor;
    gui::TWidgetsInitInfo m_info;
    pair<location::EMyPositionMode, bool> m_initialMyPositionMode;
    location::TMyPositionModeChanged m_myPositionModeChanged;
    bool m_allow3dBuildings;
    bool m_trafficEnabled;
    bool m_blockTapEvents;
    bool m_showChoosePositionMark;
    std::vector<m2::TriangleD> m_boundAreaTriangles;
    bool m_isRoutingActive;
    bool m_isAutozoomEnabled;
    bool m_simplifiedTrafficColors;
    OverlaysShowStatsCallback m_overlaysShowStatsCallback;
  };

  DrapeEngine(Params && params);
  ~DrapeEngine();

  void Update(int w, int h);

  void Resize(int w, int h);
  void Invalidate();

  void SetVisibleViewport(m2::RectD const & rect) const;

  void AddTouchEvent(TouchEvent const & event);
  void Scale(double factor, m2::PointD const & pxPoint, bool isAnim);

  // If zoom == -1 then current zoom will not be changed.
  void SetModelViewCenter(m2::PointD const & centerPt, int zoom, bool isAnim,
                          bool trackVisibleViewport);
  void SetModelViewRect(m2::RectD const & rect, bool applyRotation, int zoom, bool isAnim);
  void SetModelViewAnyRect(m2::AnyRectD const & rect, bool isAnim);

  using TModelViewListenerFn = FrontendRenderer::TModelViewChanged;
  void SetModelViewListener(TModelViewListenerFn && fn);

  void ClearUserMarksGroup(size_t layerId);
  void ChangeVisibilityUserMarksGroup(MarkGroupID groupId, bool isVisible);
  void UpdateUserMarksGroup(MarkGroupID groupId, UserMarksProvider * provider);
  void InvalidateUserMarks();

  void SetRenderingEnabled(ref_ptr<dp::OGLContextFactory> contextFactory = nullptr);
  void SetRenderingDisabled(bool const destroyContext);
  void InvalidateRect(m2::RectD const & rect);
  void UpdateMapStyle();

  void SetCompassInfo(location::CompassInfo const & info);
  void SetGpsInfo(location::GpsInfo const & info, bool isNavigable,
                  location::RouteMatchingInfo const & routeInfo);
  void SwitchMyPositionNextMode();
  void LoseLocation();
  void StopLocationFollow();

  using TTapEventInfoFn = FrontendRenderer::TTapEventInfoFn;
  void SetTapEventInfoListener(TTapEventInfoFn && fn);
  using TUserPositionChangedFn = FrontendRenderer::TUserPositionChangedFn;
  void SetUserPositionListener(TUserPositionChangedFn && fn);

  void SelectObject(SelectionShape::ESelectedObject obj, m2::PointD const & pt,
                    FeatureID const & featureID, bool isAnim);
  void DeselectObject();
  
  dp::DrapeID AddSubroute(SubrouteConstPtr subroute);
  void RemoveSubroute(dp::DrapeID subrouteId, bool deactivateFollowing);
  void FollowRoute(int preferredZoomLevel, int preferredZoomLevel3d, bool enableAutoZoom);
  void DeactivateRouteFollowing();
  void SetSubrouteVisibility(dp::DrapeID subrouteId, bool isVisible);
  dp::DrapeID AddRoutePreviewSegment(m2::PointD const & startPt, m2::PointD const & finishPt);
  void RemoveRoutePreviewSegment(dp::DrapeID segmentId);
  void RemoveAllRoutePreviewSegments();

  void SetWidgetLayout(gui::TWidgetsLayoutInfo && info);

  void AllowAutoZoom(bool allowAutoZoom);

  void Allow3dMode(bool allowPerspectiveInNavigation, bool allow3dBuildings);
  void EnablePerspective();

  void UpdateGpsTrackPoints(std::vector<df::GpsTrackPoint> && toAdd,
                            std::vector<uint32_t> && toRemove);
  void ClearGpsTrackPoints();

  void EnableChoosePositionMode(bool enable, std::vector<m2::TriangleD> && boundAreaTriangles,
                                bool hasPosition, m2::PointD const & position);
  void BlockTapEvents(bool block);

  void SetKineticScrollEnabled(bool enabled);

  void SetTimeInBackground(double time);

  void SetDisplacementMode(int mode);

  using TRequestSymbolsSizeCallback = std::function<void(std::vector<m2::PointF> const &)>;

  void RequestSymbolsSize(std::vector<string> const & symbols,
                          TRequestSymbolsSizeCallback const & callback);

  void EnableTraffic(bool trafficEnabled);
  void UpdateTraffic(traffic::TrafficInfo const & info);
  void ClearTrafficCache(MwmSet::MwmId const & mwmId);
  void SetSimplifiedTrafficColors(bool simplified);

  void SetFontScaleFactor(double scaleFactor);

  void RunScenario(ScenarioManager::ScenarioData && scenarioData,
                   ScenarioManager::ScenarioCallback const & onStartFn,
                   ScenarioManager::ScenarioCallback const & onFinishFn);

  // Custom features are features which we do not render usual way.
  // All these features will be skipped in process of geometry generation.
  void SetCustomFeatures(std::set<FeatureID> && ids);
  void RemoveCustomFeatures(MwmSet::MwmId const & mwmId);
  void RemoveAllCustomFeatures();

  void SetPosteffectEnabled(PostprocessRenderer::Effect effect, bool enabled);

  void RunFirstLaunchAnimation();

private:
  void AddUserEvent(drape_ptr<UserEvent> && e);
  void PostUserEvent(drape_ptr<UserEvent> && e);
  void ModelViewChanged(ScreenBase const & screen);

  void MyPositionModeChanged(location::EMyPositionMode mode, bool routingActive);
  void TapEvent(TapInfo const & tapInfo);
  void UserPositionChanged(m2::PointD const & position, bool hasPosition);

  void ResizeImpl(int w, int h);
  void RecacheGui(bool needResetOldGui);
  void RecacheMapShapes();

  dp::DrapeID GenerateDrapeID();

  drape_ptr<FrontendRenderer> m_frontend;
  drape_ptr<BackendRenderer> m_backend;
  drape_ptr<ThreadsCommutator> m_threadCommutator;
  drape_ptr<dp::TextureManager> m_textureManager;
  drape_ptr<RequestedTiles> m_requestedTiles;
  location::TMyPositionModeChanged m_myPositionModeChanged;

  dp::Viewport m_viewport;

  TModelViewListenerFn m_modelViewChanged;
  TUserPositionChangedFn m_userPositionChanged;
  TTapEventInfoFn m_tapListener;

  gui::TWidgetsInitInfo m_widgetsInfo;
  gui::TWidgetsLayoutInfo m_widgetsLayout;

  bool m_choosePositionMode = false;
  bool m_kineticScrollEnabled = true;

  std::mutex m_drapeIdGeneratorMutex;
  dp::DrapeID m_drapeIdGenerator = 0;

  friend class DrapeApi;
};
}  // namespace df
