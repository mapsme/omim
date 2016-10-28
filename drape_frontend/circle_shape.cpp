#include "drape_frontend/circle_shape.hpp"

#include "drape/utils/vertex_decl.hpp"
#include "drape/batcher.hpp"
#include "drape/attribute_provider.hpp"
#include "drape/glstate.hpp"
#include "drape/shader_def.hpp"
#include "drape/texture_manager.hpp"
#include "drape/overlay_handle.hpp"

namespace df
{

CircleShape::CircleShape(m2::PointF const & mercatorPt, CircleViewParams const & params, bool needOverlay)
  : m_pt(mercatorPt)
  , m_params(params)
  , m_needOverlay(needOverlay)
{}

void CircleShape::Draw(ref_ptr<dp::Batcher> batcher, ref_ptr<dp::TextureManager> textures) const
{
  int const TriangleCount = 20;
  double const etalonSector = (2.0 * math::pi) / static_cast<double>(TriangleCount);

  dp::TextureManager::ColorRegion region;
  textures->GetColorRegion(m_params.m_color, region);
  glsl::vec2 const colorPoint(glsl::ToVec2(region.GetTexRect().Center()));
  glsl::vec2 const pt = glsl::ToVec2(ConvertToLocal(m_pt, m_params.m_tileCenter, kShapeCoordScalar));

  buffer_vector<gpu::SolidTexturingVertex, 22> vertexes;
  vertexes.push_back(gpu::SolidTexturingVertex
  {
    glsl::vec4(pt, m_params.m_depth, 0.0f),
    glsl::vec2(0.0f, 0.0f),
    colorPoint
  });

  m2::PointD startNormal(0.0f, m_params.m_radius);

  for (size_t i = 0; i < TriangleCount + 1; ++i)
  {
    m2::PointD rotatedNormal = m2::Rotate(startNormal, (i) * etalonSector);
    vertexes.push_back(gpu::SolidTexturingVertex
    {
      glsl::vec4(pt, m_params.m_depth, 0.0f),
      glsl::ToVec2(rotatedNormal),
      colorPoint
    });
  }

  dp::GLState state(gpu::TEXTURING_PROGRAM, dp::GLState::OverlayLayer);
  state.SetColorTexture(region.GetTexture());

  dp::AttributeProvider provider(1, TriangleCount + 2);
  provider.InitStream(0, gpu::SolidTexturingVertex::GetBindingInfo(), make_ref(vertexes.data()));
  batcher->InsertTriangleFan(state, make_ref(&provider), CreateOverlay());
}

drape_ptr<dp::OverlayHandle> CircleShape::CreateOverlay() const
{
  if (!m_needOverlay)
    return nullptr;

  double const handleSize = 2 * m_params.m_radius;
  return make_unique_dp<dp::SquareHandle>(m_params.m_id, dp::Center, m_pt,
                                          m2::PointD(handleSize, handleSize),
                                          GetOverlayPriority(), false /* isBound */, "");
}

uint64_t CircleShape::GetOverlayPriority() const
{
  // Set up maximum priority for shapes which created by user in the editor.
  if (m_params.m_createdByEditor)
    return dp::kPriorityMaskAll;
  
  // Set up minimal priority for shapes which belong to areas.
  if (m_params.m_hasArea)
    return 0;

  return dp::CalculateOverlayPriority(m_params.m_minVisibleScale, m_params.m_rank, m_params.m_depth);
}

} // namespace df
