#pragma once

#include "drape_frontend/map_shape.hpp"
#include "drape_frontend/shape_view_params.hpp"

#include "drape/glstate.hpp"
#include "drape/render_bucket.hpp"
#include "drape/utils/vertex_decl.hpp"

#include "geometry/polyline2d.hpp"

#include "std/vector.hpp"

namespace df
{

struct RouteJoinBounds
{
  double m_start = 0;
  double m_end = 0;
  double m_offset = 0;
};

class RouteShape
{
public:
  RouteShape(m2::PolylineD const & polyline,
             CommonViewParams const & params);

  m2::RectF GetArrowTextureRect(ref_ptr<dp::TextureManager> textures) const;
  vector<RouteJoinBounds> const & GetJoinsBounds() const { return m_joinsBounds; }
  double GetLength() const { return m_length; }
  dp::GLState const & GetEndOfRouteState() const { return m_endOfRouteState; }
  drape_ptr<dp::RenderBucket> && MoveEndOfRouteRenderBucket() { return move(m_endOfRouteRenderBucket); }

  void PrepareGeometry(ref_ptr<dp::TextureManager> textures);
  void Draw(ref_ptr<dp::Batcher> batcher, ref_ptr<dp::TextureManager> textures);

private:
  using RV = gpu::RouteVertex;
  using TGeometryBuffer = buffer_vector<gpu::RouteVertex, 128>;

  void CacheEndOfRouteSign(ref_ptr<dp::TextureManager> mng);

  TGeometryBuffer m_geometry;
  TGeometryBuffer m_joinsGeometry;
  vector<RouteJoinBounds> m_joinsBounds;
  double m_length;

  CommonViewParams m_params;
  m2::PolylineD m_polyline;

  dp::GLState m_endOfRouteState;
  drape_ptr<dp::RenderBucket> m_endOfRouteRenderBucket;
};

} // namespace df
