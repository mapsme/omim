#include "drape_frontend/debug_rect_renderer.hpp"
#include "drape_frontend/render_state_extension.hpp"

#include <vector>

namespace df
{
namespace
{
void PixelPointToScreenSpace(ScreenBase const & screen, m2::PointF const & pt, std::vector<float> & buffer)
{
  auto const szX = static_cast<float>(screen.PixelRectIn3d().SizeX());
  auto const szY = static_cast<float>(screen.PixelRectIn3d().SizeY());

  buffer.push_back(2.0f * (pt.x / szX - 0.5f));
  buffer.push_back(2.0f * (-pt.y / szY + 0.5f));
}
}  // namespace

DebugRectRenderer::DebugRectRenderer(ref_ptr<dp::GraphicsContext> context, ref_ptr<dp::GpuProgram> program,
                                     ref_ptr<gpu::ProgramParamsSetter> paramsSetter)
  : Base(context, DrawPrimitive::LineStrip)
  , m_program(program)
  , m_paramsSetter(paramsSetter)
  , m_state(CreateRenderState(gpu::Program::DebugRect, DepthLayer::OverlayLayer))
{
  m_state.SetDepthTestEnabled(false);

  Base::SetBuffer(0 /* bufferInd */, {} /* vertices */, static_cast<uint32_t>(sizeof(float) * 2));
  Base::SetAttribute("a_position", 0 /* bufferInd */, 0 /* offset */, 2 /* componentsCount */);
}

bool DebugRectRenderer::IsEnabled() const
{
  return m_isEnabled;
}

void DebugRectRenderer::SetEnabled(bool enabled)
{
  m_isEnabled = enabled;
}

void DebugRectRenderer::SetArrow(ref_ptr<dp::GraphicsContext> context, m2::PointF const & arrowStart,
                                 m2::PointF const & arrowEnd, ScreenBase const & screen)
{
  std::vector<float> vertices;
  m2::PointF const dir = (arrowEnd - arrowStart).Normalize();
  m2::PointF const side = m2::PointF(-dir.y, dir.x);
  PixelPointToScreenSpace(screen, arrowStart, vertices);
  PixelPointToScreenSpace(screen, arrowEnd, vertices);
  PixelPointToScreenSpace(screen, arrowEnd - dir * 20 + side * 10, vertices);
  PixelPointToScreenSpace(screen, arrowEnd, vertices);
  PixelPointToScreenSpace(screen, arrowEnd - dir * 20 - side * 10, vertices);

  if (!Base::IsInitialized())
    Base::Build(context, m_program);
  Base::UpdateBuffer(context, 0 /* bufferInd */, std::move(vertices));
}

void DebugRectRenderer::SetRect(ref_ptr<dp::GraphicsContext> context, m2::RectF const & rect,
                                ScreenBase const & screen)
{
  std::vector<float> vertices;
  PixelPointToScreenSpace(screen, rect.LeftBottom(), vertices);
  PixelPointToScreenSpace(screen, rect.LeftTop(), vertices);
  PixelPointToScreenSpace(screen, rect.RightTop(), vertices);
  PixelPointToScreenSpace(screen, rect.RightBottom(), vertices);
  PixelPointToScreenSpace(screen, rect.LeftBottom(), vertices);

  if (!Base::IsInitialized())
    Base::Build(context, m_program);
  Base::UpdateBuffer(context, 0 /* bufferInd */, std::move(vertices));
}

void DebugRectRenderer::DrawRect(ref_ptr<dp::GraphicsContext> context, ScreenBase const & screen,
                                 m2::RectF const & rect, dp::Color const & color)
{
  if (!m_isEnabled)
    return;

  SetRect(context, rect, screen);

  gpu::DebugRectProgramParams params;
  params.m_color = glsl::ToVec4(color);

  Base::Render(context, m_program, m_state, m_paramsSetter, params);
};

void DebugRectRenderer::DrawArrow(ref_ptr<dp::GraphicsContext> context, ScreenBase const & screen,
                                  dp::OverlayTree::DisplacementData const & data)
{
  if (!m_isEnabled)
    return;

  if (data.m_arrowStart.EqualDxDy(data.m_arrowEnd, 1e-5f))
    return;

  SetArrow(context, data.m_arrowStart, data.m_arrowEnd, screen);

  gpu::DebugRectProgramParams params;
  params.m_color = glsl::ToVec4(data.m_arrowColor);

  Base::Render(context, m_program, m_state, m_paramsSetter, params);
};
}  // namespace df
