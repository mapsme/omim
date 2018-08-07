#pragma once

#include "drape_frontend/gui/layer_render.hpp"

#include "drape_frontend/base_renderer.hpp"
#include "drape_frontend/batchers_pool.hpp"
#include "drape_frontend/drape_api_builder.hpp"
#include "drape_frontend/map_data_provider.hpp"
#include "drape_frontend/overlay_batcher.hpp"
#include "drape_frontend/requested_tiles.hpp"
#include "drape_frontend/traffic_generator.hpp"
#include "drape_frontend/transit_scheme_builder.hpp"
#include "drape_frontend/user_mark_generator.hpp"

#include "drape/pointers.hpp"
#include "drape/viewport.hpp"

#include <functional>

namespace dp
{
class GraphicsContextFactory;
class TextureManager;
}  // namespace dp

namespace df
{
class Message;
class ReadManager;
class RouteBuilder;
class MetalineManager;

using TIsUGCFn = std::function<bool(FeatureID const &)>;

class BackendRenderer : public BaseRenderer
{
public:
  using TUpdateCurrentCountryFn = function<void (m2::PointD const &, int)>;

  struct Params : BaseRenderer::Params
  {
    Params(dp::ApiVersion apiVersion, ref_ptr<ThreadsCommutator> commutator,
           ref_ptr<dp::GraphicsContextFactory> factory, ref_ptr<dp::TextureManager> texMng,
           MapDataProvider const & model, TUpdateCurrentCountryFn const & updateCurrentCountryFn,
           ref_ptr<RequestedTiles> requestedTiles, bool allow3dBuildings, bool trafficEnabled,
           bool simplifiedTrafficColors, TIsUGCFn && isUGCFn)
      : BaseRenderer::Params(apiVersion, commutator, factory, texMng)
      , m_model(model)
      , m_updateCurrentCountryFn(updateCurrentCountryFn)
      , m_requestedTiles(requestedTiles)
      , m_allow3dBuildings(allow3dBuildings)
      , m_trafficEnabled(trafficEnabled)
      , m_simplifiedTrafficColors(simplifiedTrafficColors)
      , m_isUGCFn(std::move(isUGCFn))
    {}

    MapDataProvider const & m_model;
    TUpdateCurrentCountryFn m_updateCurrentCountryFn;
    ref_ptr<RequestedTiles> m_requestedTiles;
    bool m_allow3dBuildings;
    bool m_trafficEnabled;
    bool m_simplifiedTrafficColors;
    TIsUGCFn m_isUGCFn;
  };

  explicit BackendRenderer(Params && params);
  ~BackendRenderer() override;

  void Teardown();

protected:
  unique_ptr<threads::IRoutine> CreateRoutine() override;

  void OnContextCreate() override;
  void OnContextDestroy() override;

private:
  void RecacheGui(gui::TWidgetsInitInfo const & initInfo, bool needResetOldGui);
  void RecacheChoosePositionMark();
  void RecacheMapShapes();

#ifdef RENDER_DEBUG_INFO_LABELS
  void RecacheDebugLabels();
#endif

  void AcceptMessage(ref_ptr<Message> message) override;

  class Routine : public threads::IRoutine
  {
  public:
    explicit Routine(BackendRenderer & renderer);

    void Do() override;

  private:
    BackendRenderer & m_renderer;
  };

  void ReleaseResources();

  void InitGLDependentResource();
  void FlushGeometry(TileKey const & key, dp::RenderState const & state, drape_ptr<dp::RenderBucket> && buffer);

  void FlushTransitRenderData(TransitRenderData && renderData);
  void FlushTrafficRenderData(TrafficRenderData && renderData);
  void FlushUserMarksRenderData(TUserMarksRenderData && renderData);

  void CleanupOverlays(TileKey const & tileKey);

  MapDataProvider m_model;
  drape_ptr<BatchersPool<TileKey, TileKeyStrictComparator>> m_batchersPool;
  drape_ptr<ReadManager> m_readManager;
  drape_ptr<RouteBuilder> m_routeBuilder;
  drape_ptr<TransitSchemeBuilder> m_transitBuilder;
  drape_ptr<TrafficGenerator> m_trafficGenerator;
  drape_ptr<UserMarkGenerator> m_userMarkGenerator;
  drape_ptr<DrapeApiBuilder> m_drapeApiBuilder;
  gui::LayerCacher m_guiCacher;

  ref_ptr<RequestedTiles> m_requestedTiles;

  TOverlaysRenderData m_overlays;

  TUpdateCurrentCountryFn m_updateCurrentCountryFn;

  drape_ptr<MetalineManager> m_metalineManager;

  gui::TWidgetsInitInfo m_lastWidgetsInfo;

#ifdef DEBUG
  bool m_isTeardowned;
#endif
};
}  // namespace df
