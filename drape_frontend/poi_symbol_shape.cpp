#include "drape_frontend/poi_symbol_shape.hpp"

#include "drape_frontend/color_constants.hpp"

#include "shaders/programs.hpp"

#include "drape/attribute_provider.hpp"
#include "drape/batcher.hpp"
#include "drape/texture_manager.hpp"
#include "drape/utils/vertex_decl.hpp"

#include <utility>
#include <vector>

namespace
{
using SV = gpu::SolidTexturingVertex;
using MV = gpu::MaskedTexturingVertex;

glsl::vec2 ShiftNormal(glsl::vec2 const & n, df::PoiSymbolViewParams const & params,
                       m2::PointF const & pixelSize)
{
  glsl::vec2 result = n + glsl::vec2(params.m_offset.x, params.m_offset.y);
  m2::PointF const halfPixelSize = pixelSize * 0.5f;

  if (params.m_anchor & dp::Top)
    result.y += halfPixelSize.y;
  else if (params.m_anchor & dp::Bottom)
    result.y -= halfPixelSize.y;

  if (params.m_anchor & dp::Left)
    result.x += halfPixelSize.x;
  else if (params.m_anchor & dp::Right)
    result.x -= halfPixelSize.x;

  return result;
}

template <typename TVertex>
void Batch(ref_ptr<dp::GraphicsContext> context, ref_ptr<dp::Batcher> batcher,
           drape_ptr<dp::OverlayHandle> && handle, glsl::vec4 const & position,
           df::PoiSymbolViewParams const & params,
           dp::TextureManager::SymbolRegion const & symbolRegion,
           dp::TextureManager::ColorRegion const & colorRegion)
{
  ASSERT(0, ("Can not be used without specialization"));
}

template <>
void Batch<SV>(ref_ptr<dp::GraphicsContext> context, ref_ptr<dp::Batcher> batcher,
               drape_ptr<dp::OverlayHandle> && handle, glsl::vec4 const & position,
               df::PoiSymbolViewParams const & params,
               dp::TextureManager::SymbolRegion const & symbolRegion,
               dp::TextureManager::ColorRegion const & colorRegion)
{
  m2::PointF const symbolRegionSize = symbolRegion.GetPixelSize();
  m2::PointF pixelSize = symbolRegionSize;
  if (pixelSize.x < params.m_pixelWidth)
    pixelSize.x = params.m_pixelWidth;
  m2::PointF const halfSize = pixelSize * 0.5f;
  m2::RectF const & texRect = symbolRegion.GetTexRect();

  std::vector<SV> vertexes;
  vertexes.reserve(params.m_pixelWidth ? 8 : 4);
  vertexes.emplace_back(position,
                        ShiftNormal(glsl::vec2(-halfSize.x, halfSize.y), params, pixelSize),
                        glsl::vec2(texRect.minX(), texRect.maxY()));
  vertexes.emplace_back(position,
                        ShiftNormal(glsl::vec2(-halfSize.x, -halfSize.y), params, pixelSize),
                        glsl::vec2(texRect.minX(), texRect.minY()));
  if (symbolRegionSize.x < params.m_pixelWidth)
  {
    float const stretchHalfWidth = (params.m_pixelWidth - symbolRegionSize.x) * 0.5f;
    float const midTexU = (texRect.minX() + texRect.maxX()) * 0.5f;
    vertexes.emplace_back(position,
                          ShiftNormal(glsl::vec2(-stretchHalfWidth, halfSize.y), params, pixelSize),
                          glsl::vec2(midTexU, texRect.maxY()));
    vertexes.emplace_back(position,
                          ShiftNormal(glsl::vec2(-stretchHalfWidth, -halfSize.y), params, pixelSize),
                          glsl::vec2(midTexU, texRect.minY()));
    vertexes.emplace_back(position,
                          ShiftNormal(glsl::vec2(stretchHalfWidth, halfSize.y), params, pixelSize),
                          glsl::vec2(midTexU, texRect.maxY()));
    vertexes.emplace_back(position,
                          ShiftNormal(glsl::vec2(stretchHalfWidth, -halfSize.y), params, pixelSize),
                          glsl::vec2(midTexU, texRect.minY()));
  }
  vertexes.emplace_back(position,
                        ShiftNormal(glsl::vec2(halfSize.x, halfSize.y), params, pixelSize),
                        glsl::vec2(texRect.maxX(), texRect.maxY()));
  vertexes.emplace_back(position,
                        ShiftNormal(glsl::vec2(halfSize.x, -halfSize.y), params, pixelSize),
                        glsl::vec2(texRect.maxX(), texRect.minY()));

  auto state = df::CreateRenderState(gpu::Program::Texturing, params.m_depthLayer);
  state.SetProgram3d(gpu::Program::TexturingBillboard);
  state.SetDepthTestEnabled(params.m_depthTestEnabled);
  state.SetColorTexture(symbolRegion.GetTexture());
  state.SetTextureFilter(dp::TextureFilter::Nearest);
  state.SetTextureIndex(symbolRegion.GetTextureIndex());

  dp::AttributeProvider provider(1 /* streamCount */, vertexes.size());
  provider.InitStream(0 /* streamIndex */, SV::GetBindingInfo(), make_ref(vertexes.data()));
  batcher->InsertTriangleStrip(context, state, make_ref(&provider), move(handle));
}

template <>
void Batch<MV>(ref_ptr<dp::GraphicsContext> context, ref_ptr<dp::Batcher> batcher,
               drape_ptr<dp::OverlayHandle> && handle, glsl::vec4 const & position,
               df::PoiSymbolViewParams const & params,
               dp::TextureManager::SymbolRegion const & symbolRegion,
               dp::TextureManager::ColorRegion const & colorRegion)
{
  m2::PointF const symbolRegionSize = symbolRegion.GetPixelSize();
  m2::PointF pixelSize = symbolRegionSize;
  if (pixelSize.x < params.m_pixelWidth)
    pixelSize.x = params.m_pixelWidth;
  m2::PointF const halfSize = pixelSize * 0.5f;
  m2::RectF const & texRect = symbolRegion.GetTexRect();
  glsl::vec2 const maskColorCoords = glsl::ToVec2(colorRegion.GetTexRect().Center());

  std::vector<MV> vertexes;
  vertexes.reserve(params.m_pixelWidth ? 8 : 4);
  vertexes.emplace_back(position,
                        ShiftNormal(glsl::vec2(-halfSize.x, halfSize.y), params, pixelSize),
                        glsl::vec2(texRect.minX(), texRect.maxY()), maskColorCoords);
  vertexes.emplace_back(position,
                        ShiftNormal(glsl::vec2(-halfSize.x, -halfSize.y), params, pixelSize),
                        glsl::vec2(texRect.minX(), texRect.minY()), maskColorCoords);
  if (symbolRegionSize.x < params.m_pixelWidth)
  {
    float const stretchHalfWidth = (params.m_pixelWidth - symbolRegionSize.x) * 0.5f;
    float const midTexU = (texRect.minX() + texRect.maxX()) * 0.5f;
    vertexes.emplace_back(position,
                          ShiftNormal(glsl::vec2(-stretchHalfWidth, halfSize.y), params, pixelSize),
                          glsl::vec2(midTexU, texRect.maxY()), maskColorCoords);
    vertexes.emplace_back(position,
                          ShiftNormal(glsl::vec2(-stretchHalfWidth, -halfSize.y), params, pixelSize),
                          glsl::vec2(midTexU, texRect.minY()), maskColorCoords);
    vertexes.emplace_back(position,
                          ShiftNormal(glsl::vec2(stretchHalfWidth, halfSize.y), params, pixelSize),
                          glsl::vec2(midTexU, texRect.maxY()), maskColorCoords);
    vertexes.emplace_back(position,
                          ShiftNormal(glsl::vec2(stretchHalfWidth, -halfSize.y), params, pixelSize),
                          glsl::vec2(midTexU, texRect.minY()), maskColorCoords);
  }
  vertexes.emplace_back(position,
                        ShiftNormal(glsl::vec2(halfSize.x, halfSize.y), params, pixelSize),
                        glsl::vec2(texRect.maxX(), texRect.maxY()), maskColorCoords);
  vertexes.emplace_back(position,
                        ShiftNormal(glsl::vec2(halfSize.x, -halfSize.y), params, pixelSize),
                        glsl::vec2(texRect.maxX(), texRect.minY()), maskColorCoords);

  auto state = df::CreateRenderState(gpu::Program::MaskedTexturing, params.m_depthLayer);
  state.SetProgram3d(gpu::Program::MaskedTexturingBillboard);
  state.SetDepthTestEnabled(params.m_depthTestEnabled);
  state.SetColorTexture(symbolRegion.GetTexture());
  state.SetMaskTexture(colorRegion.GetTexture()); // Here mask is a color.
  state.SetTextureFilter(dp::TextureFilter::Nearest);
  state.SetTextureIndex(symbolRegion.GetTextureIndex());

  dp::AttributeProvider provider(1 /* streamCount */, vertexes.size());
  provider.InitStream(0 /* streamIndex */, MV::GetBindingInfo(), make_ref(vertexes.data()));
  batcher->InsertTriangleStrip(context, state, make_ref(&provider), move(handle));
}
}  // namespace

namespace df
{
PoiSymbolShape::PoiSymbolShape(m2::PointD const & mercatorPt, PoiSymbolViewParams const & params,
                               TileKey const & tileKey, uint32_t textIndex)
  : m_pt(mercatorPt)
  , m_params(params)
  , m_tileCoords(tileKey.GetTileCoords())
  , m_textIndex(textIndex)
{}

void PoiSymbolShape::Draw(ref_ptr<dp::GraphicsContext> context, ref_ptr<dp::Batcher> batcher,
                          ref_ptr<dp::TextureManager> textures) const
{
  dp::TextureManager::SymbolRegion region;
  textures->GetSymbolRegion(m_params.m_symbolName, region);

  glsl::vec2 const pt = glsl::ToVec2(ConvertToLocal(m_pt, m_params.m_tileCenter, kShapeCoordScalar));
  glsl::vec4 const position = glsl::vec4(pt, m_params.m_depth, -m_params.m_posZ);
  m2::PointF const pixelSize = region.GetPixelSize();

  if (m_params.m_maskColor.empty())
  {
    Batch<SV>(context, batcher, CreateOverlayHandle(pixelSize), position, m_params, region,
              dp::TextureManager::ColorRegion());
  }
  else
  {
    dp::TextureManager::ColorRegion maskColorRegion;
    textures->GetColorRegion(df::GetColorConstant(m_params.m_maskColor), maskColorRegion);
    Batch<MV>(context, batcher, CreateOverlayHandle(pixelSize), position, m_params, region,
              maskColorRegion);
  }
}

drape_ptr<dp::OverlayHandle> PoiSymbolShape::CreateOverlayHandle(m2::PointF const & pixelSize) const
{
  dp::OverlayID overlayId = dp::OverlayID(m_params.m_id, m_tileCoords, m_textIndex);
  drape_ptr<dp::OverlayHandle> handle = make_unique_dp<dp::SquareHandle>(overlayId, m_params.m_anchor,
                                                                         m_pt, m2::PointD(pixelSize),
                                                                         m2::PointD(m_params.m_offset),
                                                                         GetOverlayPriority(),
                                                                         true /* isBound */,
                                                                         m_params.m_symbolName,
                                                                         m_params.m_minVisibleScale,
                                                                         true /* isBillboard */);
  handle->SetPivotZ(m_params.m_posZ);
  handle->SetExtendingSize(m_params.m_extendingSize);
  if (m_params.m_specialDisplacement == SpecialDisplacement::UserMark ||
      m_params.m_specialDisplacement == SpecialDisplacement::SpecialModeUserMark)
  {
    handle->SetSpecialLayerOverlay(true);
  }
  handle->SetOverlayRank(m_params.m_startOverlayRank);
  return handle;
}

uint64_t PoiSymbolShape::GetOverlayPriority() const
{
  // Set up maximum priority for some POI.
  if (m_params.m_prioritized)
    return dp::kPriorityMaskAll;

  // Special displacement mode.
  if (m_params.m_specialDisplacement == SpecialDisplacement::SpecialMode)
    return dp::CalculateSpecialModePriority(m_params.m_specialPriority);

  if (m_params.m_specialDisplacement == SpecialDisplacement::SpecialModeUserMark)
    return dp::CalculateSpecialModeUserMarkPriority(m_params.m_specialPriority);

  if (m_params.m_specialDisplacement == SpecialDisplacement::UserMark)
    return dp::CalculateUserMarkPriority(m_params.m_minVisibleScale, m_params.m_specialPriority);

  return dp::CalculateOverlayPriority(m_params.m_minVisibleScale, m_params.m_rank, m_params.m_depth);
}
}  // namespace df
