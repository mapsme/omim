#include "drape_frontend/path_text_shape.hpp"
#include "drape_frontend/path_text_handle.hpp"
#include "drape_frontend/render_state_extension.hpp"

#include "shaders/programs.hpp"

#include "drape/attribute_provider.hpp"
#include "drape/batcher.hpp"
#include "drape/overlay_handle.hpp"

#include "base/logging.hpp"
#include "base/math.hpp"
#include "base/matrix.hpp"
#include "base/stl_helpers.hpp"
#include "base/string_utils.hpp"
#include "base/timer.hpp"

#include "geometry/transformations.hpp"

using m2::Spline;

namespace df
{
PathTextShape::PathTextShape(m2::SharedSpline const & spline,
                             PathTextViewParams const & params,
                             TileKey const & tileKey, uint32_t baseTextIndex)
  : m_spline(spline)
  , m_params(params)
  , m_tileCoords(tileKey.GetTileCoords())
  , m_baseTextIndex(baseTextIndex)
{
  m_context.reset(new PathTextContext(m_spline));
}

bool PathTextShape::CalculateLayout(ref_ptr<dp::TextureManager> textures)
{
  std::string text = m_params.m_mainText;
  if (!m_params.m_auxText.empty())
    text += "   " + m_params.m_auxText;

  auto layout = make_unique_dp<PathTextLayout>(m_params.m_tileCenter,
                                               strings::MakeUniString(text),
                                               m_params.m_textFont.m_size,
                                               m_params.m_textFont.m_isSdf,
                                               textures);
  uint32_t const glyphCount = layout->GetGlyphCount();
  if (glyphCount == 0)
    return false;

  m_context->SetLayout(std::move(layout), m_params.m_baseGtoPScale);

  return !m_context->GetOffsets().empty();
}

uint64_t PathTextShape::GetOverlayPriority(uint32_t textIndex, size_t textLength) const
{
  // Overlay priority for path text shapes considers length of the text and index of text.
  // Greater text length has more priority, because smaller texts have more chances to be shown along the road.
  // [6 bytes - standard overlay priority][1 byte - length][1 byte - path text index].

  // Special displacement mode.
  if (m_params.m_specialDisplacement == SpecialDisplacement::SpecialMode)
    return dp::CalculateSpecialModePriority(m_params.m_specialPriority);

  static uint64_t constexpr kMask = ~static_cast<uint64_t>(0xFFFF);
  uint64_t priority = dp::CalculateOverlayPriority(m_params.m_minVisibleScale, m_params.m_rank,
                                                   m_params.m_depth);
  priority &= kMask;
  priority |= (static_cast<uint8_t>(textLength) << 8);
  priority |= static_cast<uint8_t>(textIndex);

  return priority;
}

void PathTextShape::DrawPathTextPlain(ref_ptr<dp::TextureManager> textures,
                                      ref_ptr<dp::Batcher> batcher) const
{
  auto const layout = m_context->GetLayout();
  ASSERT(layout != nullptr, ());
  ASSERT(!m_context->GetOffsets().empty(), ());

  dp::TextureManager::ColorRegion color;
  textures->GetColorRegion(m_params.m_textFont.m_color, color);

  auto state = CreateRenderState(layout->GetFixedHeight() > 0 ?
                                 gpu::Program::TextFixed : gpu::Program::Text,
                                 DepthLayer::OverlayLayer);
  state.SetProgram3d(layout->GetFixedHeight() > 0 ?
                     gpu::Program::TextFixedBillboard : gpu::Program::TextBillboard);
  state.SetDepthTestEnabled(m_params.m_depthTestEnabled);
  state.SetColorTexture(color.GetTexture());
  state.SetMaskTexture(layout->GetMaskTexture());

  gpu::TTextStaticVertexBuffer staticBuffer;
  gpu::TTextDynamicVertexBuffer dynBuffer;

  for (uint32_t textIndex = 0; textIndex < m_context->GetOffsets().size(); ++textIndex)
  {
    staticBuffer.clear();
    dynBuffer.clear();

    layout->CacheStaticGeometry(color, staticBuffer);
    dynBuffer.resize(staticBuffer.size());

    dp::AttributeProvider provider(2, static_cast<uint32_t>(staticBuffer.size()));
    provider.InitStream(0, gpu::TextStaticVertex::GetBindingInfo(),
                        make_ref(staticBuffer.data()));
    provider.InitStream(1, gpu::TextDynamicVertex::GetBindingInfo(),
                        make_ref(dynBuffer.data()));
    batcher->InsertListOfStrip(state, make_ref(&provider),
                               CreateOverlayHandle(textIndex, textures), 4);
  }
}

void PathTextShape::DrawPathTextOutlined(ref_ptr<dp::TextureManager> textures,
                                         ref_ptr<dp::Batcher> batcher) const
{
  auto const layout = m_context->GetLayout();
  ASSERT(layout != nullptr, ());
  ASSERT(!m_context->GetOffsets().empty(), ());

  dp::TextureManager::ColorRegion color;
  dp::TextureManager::ColorRegion outline;
  textures->GetColorRegion(m_params.m_textFont.m_color, color);
  textures->GetColorRegion(m_params.m_textFont.m_outlineColor, outline);

  auto state = CreateRenderState(gpu::Program::TextOutlined, DepthLayer::OverlayLayer);
  state.SetProgram3d(gpu::Program::TextOutlinedBillboard);
  state.SetDepthTestEnabled(m_params.m_depthTestEnabled);
  state.SetColorTexture(color.GetTexture());
  state.SetMaskTexture(layout->GetMaskTexture());

  gpu::TTextOutlinedStaticVertexBuffer staticBuffer;
  gpu::TTextDynamicVertexBuffer dynBuffer;
  for (uint32_t textIndex = 0; textIndex < m_context->GetOffsets().size(); ++textIndex)
  {
    staticBuffer.clear();
    dynBuffer.clear();

    layout->CacheStaticGeometry(color, outline, staticBuffer);
    dynBuffer.resize(staticBuffer.size());

    dp::AttributeProvider provider(2, static_cast<uint32_t>(staticBuffer.size()));
    provider.InitStream(0, gpu::TextOutlinedStaticVertex::GetBindingInfo(),
                        make_ref(staticBuffer.data()));
    provider.InitStream(1, gpu::TextDynamicVertex::GetBindingInfo(),
                        make_ref(dynBuffer.data()));
    batcher->InsertListOfStrip(state, make_ref(&provider),
                               CreateOverlayHandle(textIndex, textures), 4);
  }
}

drape_ptr<dp::OverlayHandle> PathTextShape::CreateOverlayHandle(uint32_t textIndex,
                                                                ref_ptr<dp::TextureManager> textures) const
{
  dp::OverlayID const overlayId = dp::OverlayID(m_params.m_featureID, m_tileCoords,
                                                m_baseTextIndex + textIndex);
  auto const layout = m_context->GetLayout();
  auto const priority = GetOverlayPriority(textIndex, layout->GetText().size());
  return make_unique_dp<PathTextHandle>(overlayId, m_context, m_params.m_depth,
                                        textIndex, priority, layout->GetFixedHeight(),
                                        textures, m_params.m_minVisibleScale, true /* isBillboard */);
}

void PathTextShape::Draw(ref_ptr<dp::Batcher> batcher, ref_ptr<dp::TextureManager> textures) const
{
  if (m_context->GetLayout() == nullptr || m_context->GetOffsets().empty())
    return;

  if (m_params.m_textFont.m_outlineColor == dp::Color::Transparent())
    DrawPathTextPlain(textures, batcher);
  else
    DrawPathTextOutlined(textures, batcher);
}
}  // namespace df
