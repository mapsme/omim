#pragma once

#include "drape_frontend/backend_renderer.hpp"
#include "drape_frontend/color_constants.hpp"
#include "drape_frontend/frontend_renderer.hpp"
#include "drape_frontend/route_shape.hpp"
#include "drape_frontend/selection_shape.hpp"
#include "drape_frontend/traffic_generator.hpp"
#include "drape_frontend/threads_commutator.hpp"

#include "drape/pointers.hpp"
#include "drape/texture_manager.hpp"

#include "platform/location.hpp"

#include "geometry/polyline2d.hpp"
#include "geometry/screenbase.hpp"
#include "geometry/triangle2d.hpp"

#include "base/strings_bundle.hpp"

#include "std/map.hpp"
#include "std/mutex.hpp"

namespace dp { class OGLContextFactory; }

namespace df
{

class UserMarksProvider;
class MapDataProvider;
class Viewport;

class DrapeEngine
{
public:
  struct Params
  {
    Params(ref_ptr<dp::OGLContextFactory> factory,
           ref_ptr<StringsBundle> stringBundle,
           Viewport const & viewport,
           MapDataProvider const & model,
           double vs,
           gui::TWidgetsInitInfo && info,
           pair<location::EMyPositionMode, bool> const & initialMyPositionMode,
           bool allow3dBuildings,
           bool blockTapEvents,
           bool showChoosePositionMark,
           vector<m2::TriangleD> && boundAreaTriangles,
           bool firstLaunch,
           bool isRoutingActive,
           bool isAutozoomEnabled)
      : m_factory(factory)
      , m_stringsBundle(stringBundle)
      , m_viewport(viewport)
      , m_model(model)
      , m_vs(vs)
      , m_info(move(info))
      , m_initialMyPositionMode(initialMyPositionMode)
      , m_allow3dBuildings(allow3dBuildings)
      , m_blockTapEvents(blockTapEvents)
      , m_showChoosePositionMark(showChoosePositionMark)
      , m_boundAreaTriangles(move(boundAreaTriangles))
      , m_isFirstLaunch(firstLaunch)
      , m_isRoutingActive(isRoutingActive)
      , m_isAutozoomEnabled(isAutozoomEnabled)
    {}

    ref_ptr<dp::OGLContextFactory> m_factory;
    ref_ptr<StringsBundle> m_stringsBundle;
    Viewport m_viewport;
    MapDataProvider m_model;
    double m_vs;
    gui::TWidgetsInitInfo m_info;
    pair<location::EMyPositionMode, bool> m_initialMyPositionMode;
    bool m_allow3dBuildings;
    bool m_blockTapEvents;
    bool m_showChoosePositionMark;
    vector<m2::TriangleD> m_boundAreaTriangles;
    bool m_isFirstLaunch;
    bool m_isRoutingActive;
    bool m_isAutozoomEnabled;
  };

  DrapeEngine(Params && params);
  ~DrapeEngine();

  void Update(int w, int h);

  void Resize(int w, int h);
  void Invalidate();

  void SetVisibleViewport(m2::RectD const & rect) const;

  void AddTouchEvent(TouchEvent const & event);
  void Scale(double factor, m2::PointD const & pxPoint, bool isAnim);

  /// if zoom == -1, then current zoom will not change
  void SetModelViewCenter(m2::PointD const & centerPt, int zoom, bool isAnim);
  void SetModelViewRect(m2::RectD const & rect, bool applyRotation, int zoom, bool isAnim);
  void SetModelViewAnyRect(m2::AnyRectD const & rect, bool isAnim);

  using TModelViewListenerFn = FrontendRenderer::TModelViewChanged;
  void SetModelViewListener(TModelViewListenerFn && fn);

  void ClearUserMarksLayer(size_t layerId);
  void ChangeVisibilityUserMarksLayer(size_t layerId, bool isVisible);
  void UpdateUserMarksLayer(size_t layerId, UserMarksProvider * provider);

  void SetRenderingEnabled(ref_ptr<dp::OGLContextFactory> contextFactory = nullptr);
  void SetRenderingDisabled(bool const destroyContext);
  void InvalidateRect(m2::RectD const & rect);
  void UpdateMapStyle();

  void SetCompassInfo(location::CompassInfo const & info);
  void SetGpsInfo(location::GpsInfo const & info, bool isNavigable, location::RouteMatchingInfo const & routeInfo);
  void SwitchMyPositionNextMode();
  void LoseLocation();
  void StopLocationFollow();
  void SetMyPositionModeListener(location::TMyPositionModeChanged && fn);

  using TTapEventInfoFn = FrontendRenderer::TTapEventInfoFn;
  void SetTapEventInfoListener(TTapEventInfoFn && fn);
  using TUserPositionChangedFn = FrontendRenderer::TUserPositionChangedFn;
  void SetUserPositionListener(TUserPositionChangedFn && fn);

  FeatureID GetVisiblePOI(m2::PointD const & glbPoint);
  void SelectObject(SelectionShape::ESelectedObject obj, m2::PointD const & pt, FeatureID const & featureID, bool isAnim);
  void DeselectObject();
  bool GetMyPosition(m2::PointD & myPosition);
  SelectionShape::ESelectedObject GetSelectedObject();

  void AddRoute(m2::PolylineD const & routePolyline, vector<double> const & turns,
                df::ColorConstant color, df::RoutePattern pattern = df::RoutePattern());
  void RemoveRoute(bool deactivateFollowing);
  void FollowRoute(int preferredZoomLevel, int preferredZoomLevel3d, bool enableAutoZoom);
  void DeactivateRouteFollowing();
  void SetRoutePoint(m2::PointD const & position, bool isStart, bool isValid);

  void SetWidgetLayout(gui::TWidgetsLayoutInfo && info);

  void AllowAutoZoom(bool allowAutoZoom);

  void Allow3dMode(bool allowPerspectiveInNavigation, bool allow3dBuildings);
  void EnablePerspective();

  void UpdateGpsTrackPoints(vector<df::GpsTrackPoint> && toAdd, vector<uint32_t> && toRemove);
  void ClearGpsTrackPoints();

  void EnableChoosePositionMode(bool enable, vector<m2::TriangleD> && boundAreaTriangles,
                                bool hasPosition, m2::PointD const & position);
  void BlockTapEvents(bool block);

  void SetKineticScrollEnabled(bool enabled);

  void SetTimeInBackground(double time);

  void SetDisplacementMode(int mode);

  using TRequestSymbolsSizeCallback = function<void(vector<m2::PointU> const &)>;

  void RequestSymbolsSize(vector<string> const & symbols,
                          TRequestSymbolsSizeCallback const & callback);

  void AddTrafficSegments(vector<pair<uint64_t, m2::PolylineD>> const & segments);
  void UpdateTraffic(vector<TrafficSegmentData> const & segmentsData);

  void SetFontScaleFactor(double scaleFactor);

private:
  void AddUserEvent(drape_ptr<UserEvent> && e);
  void ModelViewChanged(ScreenBase const & screen);

  void MyPositionModeChanged(location::EMyPositionMode mode, bool routingActive);
  void TapEvent(TapInfo const & tapInfo);
  void UserPositionChanged(m2::PointD const & position);

  void ResizeImpl(int w, int h);
  void RecacheGui(bool needResetOldGui);
  void RecacheMapShapes();

private:
  drape_ptr<FrontendRenderer> m_frontend;
  drape_ptr<BackendRenderer> m_backend;
  drape_ptr<ThreadsCommutator> m_threadCommutator;
  drape_ptr<dp::TextureManager> m_textureManager;
  drape_ptr<RequestedTiles> m_requestedTiles;

  Viewport m_viewport;

  TModelViewListenerFn m_modelViewChanged;
  location::TMyPositionModeChanged m_myPositionModeChanged;
  TUserPositionChangedFn m_userPositionChanged;
  TTapEventInfoFn m_tapListener;

  gui::TWidgetsInitInfo m_widgetsInfo;
  gui::TWidgetsLayoutInfo m_widgetsLayout;

  bool m_choosePositionMode = false;
  bool m_kineticScrollEnabled = true;

  friend class DrapeApi;
};

} // namespace df
