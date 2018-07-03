#include "drape_frontend/transit_scheme_renderer.hpp"

#include "drape_frontend/postprocess_renderer.hpp"
#include "drape_frontend/shape_view_params.hpp"
#include "drape_frontend/visual_params.hpp"

#include "shaders/programs.hpp"

#include "drape/overlay_tree.hpp"
#include "drape/vertex_array_buffer.hpp"

#include <algorithm>

namespace df
{
namespace
{
float CalculateHalfWidth(ScreenBase const & screen)
{
  double zoom = 0.0;
  int index = 0;
  float lerpCoef = 0.0f;
  ExtractZoomFactors(screen, zoom, index, lerpCoef);

  return InterpolateByZoomLevels(index, lerpCoef, kTransitLinesWidthInPixel)
    * static_cast<float>(VisualParams::Instance().GetVisualScale());
}
}  // namespace

bool TransitSchemeRenderer::IsSchemeVisible(int zoomLevel) const
{
  return zoomLevel >= kTransitSchemeMinZoomLevel;
}

bool TransitSchemeRenderer::HasRenderData() const
{
  return !m_linesRenderData.empty() || !m_linesCapsRenderData.empty() || !m_markersRenderData.empty() ||
    !m_textRenderData.empty() || !m_colorSymbolRenderData.empty();
}

void TransitSchemeRenderer::ClearGLDependentResources(ref_ptr<dp::OverlayTree> tree)
{
  if (tree)
  {
    RemoveOverlays(tree, m_textRenderData);
    RemoveOverlays(tree, m_colorSymbolRenderData);
  }

  m_linesRenderData.clear();
  m_linesCapsRenderData.clear();
  m_markersRenderData.clear();
  m_textRenderData.clear();
  m_colorSymbolRenderData.clear();
}

void TransitSchemeRenderer::Clear(MwmSet::MwmId const & mwmId, ref_ptr<dp::OverlayTree> tree)
{
  ClearRenderData(mwmId, nullptr /* tree */, m_linesRenderData);
  ClearRenderData(mwmId, nullptr /* tree */, m_linesCapsRenderData);
  ClearRenderData(mwmId, nullptr /* tree */, m_markersRenderData);
  ClearRenderData(mwmId, tree, m_textRenderData);
  ClearRenderData(mwmId, tree, m_colorSymbolRenderData);
}

void TransitSchemeRenderer::ClearRenderData(MwmSet::MwmId const & mwmId, ref_ptr<dp::OverlayTree> tree,
                                            std::vector<TransitRenderData> & renderData)
{
  auto removePredicate = [&mwmId](TransitRenderData const & data) { return data.m_mwmId == mwmId; };
  ClearRenderData(removePredicate, tree, renderData);
}

void TransitSchemeRenderer::ClearRenderData(TRemovePredicate const & predicate, ref_ptr<dp::OverlayTree> tree,
                                            std::vector<TransitRenderData> & renderData)
{
  if (tree)
  {
    for (auto & data : renderData)
    {
      if (predicate(data))
        data.m_bucket->RemoveOverlayHandles(tree);
    }
  }

  renderData.erase(std::remove_if(renderData.begin(), renderData.end(), predicate),
                   renderData.end());
}

void TransitSchemeRenderer::AddRenderData(ref_ptr<gpu::ProgramManager> mng, ref_ptr<dp::OverlayTree> tree,
                                          TransitRenderData && renderData)
{
  switch (renderData.m_type)
  {
    case TransitRenderData::Type::LinesCaps:
      PrepareRenderData(mng, tree, m_linesCapsRenderData, std::move(renderData));
      break;
    case TransitRenderData::Type::Lines:
      PrepareRenderData(mng, tree, m_linesRenderData, std::move(renderData));
      break;
    case TransitRenderData::Type::Markers:
      PrepareRenderData(mng, tree, m_markersRenderData, std::move(renderData));
      break;
    case TransitRenderData::Type::Text:
      PrepareRenderData(mng, tree, m_textRenderData, std::move(renderData));
      break;
    case TransitRenderData::Type::Stubs:
      PrepareRenderData(mng, tree, m_colorSymbolRenderData, std::move(renderData));
      break;
  }
}

void TransitSchemeRenderer::PrepareRenderData(ref_ptr<gpu::ProgramManager> mng, ref_ptr<dp::OverlayTree> tree,
                                              std::vector<TransitRenderData> & currentRenderData,
                                              TransitRenderData && newRenderData)
{
  // Remove obsolete render data.
  auto const removePredicate = [this, &newRenderData](TransitRenderData const & rd)
  {
    return rd.m_mwmId == newRenderData.m_mwmId && rd.m_recacheId < m_lastRecacheId;
  };
  ClearRenderData(removePredicate, tree, currentRenderData);

  m_lastRecacheId = max(m_lastRecacheId, newRenderData.m_recacheId);

  // Add new render data.
  auto program = mng->GetProgram(newRenderData.m_state.GetProgram<gpu::Program>());
  program->Bind();
  newRenderData.m_bucket->GetBuffer()->Build(program);

  currentRenderData.emplace_back(std::move(newRenderData));
}

void TransitSchemeRenderer::RenderTransit(ScreenBase const & screen, ref_ptr<gpu::ProgramManager> mng,
                                          ref_ptr<PostprocessRenderer> postprocessRenderer,
                                          dp::UniformValuesStorage const & commonUniforms)
{
  auto const zoomLevel = GetDrawTileScale(screen);
  if (!IsSchemeVisible(zoomLevel) || !HasRenderData())
    return;

  float const pixelHalfWidth = CalculateHalfWidth(screen);

  RenderLinesCaps(screen, mng, commonUniforms, pixelHalfWidth);
  RenderLines(screen, mng, commonUniforms, pixelHalfWidth);
  RenderMarkers(screen, mng, commonUniforms, pixelHalfWidth);
  {
    StencilWriterGuard guard(postprocessRenderer);
    RenderText(screen, mng, commonUniforms);
  }
  // Render only for debug purpose.
  //RenderStubs(screen, mng, commonUniforms);
}

void TransitSchemeRenderer::CollectOverlays(ref_ptr<dp::OverlayTree> tree, ScreenBase const & modelView)
{
  CollectOverlays(tree, modelView, m_textRenderData);
  CollectOverlays(tree, modelView, m_colorSymbolRenderData);
}

void TransitSchemeRenderer::CollectOverlays(ref_ptr<dp::OverlayTree> tree, ScreenBase const & modelView,
                                            std::vector<TransitRenderData> & renderData)
{
  for (auto & data : renderData)
  {
    if (tree->IsNeedUpdate())
      data.m_bucket->CollectOverlayHandles(tree);
    else
      data.m_bucket->Update(modelView);
  }
}

void TransitSchemeRenderer::RemoveOverlays(ref_ptr<dp::OverlayTree> tree, std::vector<TransitRenderData> & renderData)
{
  for (auto & data : renderData)
    data.m_bucket->RemoveOverlayHandles(tree);
}

void TransitSchemeRenderer::RenderLinesCaps(ScreenBase const & screen, ref_ptr<gpu::ProgramManager> mng,
                                            dp::UniformValuesStorage const & commonUniforms, float pixelHalfWidth)
{
  GLFunctions::glEnable(gl_const::GLDepthTest);
  GLFunctions::glClear(gl_const::GLDepthBit);
  for (auto & renderData : m_linesCapsRenderData)
  {
    ref_ptr<dp::GpuProgram> program = mng->GetProgram(renderData.m_state.GetProgram<gpu::Program>());
    program->Bind();
    dp::ApplyState(renderData.m_state, program);

    dp::UniformValuesStorage uniforms = commonUniforms;
    math::Matrix<float, 4, 4> mv = screen.GetModelView(renderData.m_pivot, kShapeCoordScalar);
    uniforms.SetMatrix4x4Value("u_modelView", mv.m_data);

    uniforms.SetFloatValue("u_lineHalfWidth", pixelHalfWidth);
    uniforms.SetFloatValue("u_maxRadius", kTransitLineHalfWidth);
    dp::ApplyUniforms(uniforms, program);

    renderData.m_bucket->Render(false /* draw as line */);
  }
}

void TransitSchemeRenderer::RenderLines(ScreenBase const & screen, ref_ptr<gpu::ProgramManager> mng,
                                        dp::UniformValuesStorage const & commonUniforms, float pixelHalfWidth)
{
  GLFunctions::glEnable(gl_const::GLDepthTest);
  for (auto & renderData : m_linesRenderData)
  {
    ref_ptr<dp::GpuProgram> program = mng->GetProgram(renderData.m_state.GetProgram<gpu::Program>());
    program->Bind();
    dp::ApplyState(renderData.m_state, program);

    dp::UniformValuesStorage uniforms = commonUniforms;
    math::Matrix<float, 4, 4> mv = screen.GetModelView(renderData.m_pivot, kShapeCoordScalar);
    uniforms.SetMatrix4x4Value("u_modelView", mv.m_data);

    uniforms.SetFloatValue("u_lineHalfWidth", pixelHalfWidth);
    dp::ApplyUniforms(uniforms, program);

    renderData.m_bucket->Render(false /* draw as line */);
  }
}

void TransitSchemeRenderer::RenderMarkers(ScreenBase const & screen, ref_ptr<gpu::ProgramManager> mng,
                                          dp::UniformValuesStorage const & commonUniforms, float pixelHalfWidth)
{
  GLFunctions::glEnable(gl_const::GLDepthTest);
  GLFunctions::glClear(gl_const::GLDepthBit);
  for (auto & renderData : m_markersRenderData)
  {
    auto program = mng->GetProgram(renderData.m_state.GetProgram<gpu::Program>());
    program->Bind();
    dp::ApplyState(renderData.m_state, program);

    dp::UniformValuesStorage uniforms = commonUniforms;
    math::Matrix<float, 4, 4> mv = screen.GetModelView(renderData.m_pivot, kShapeCoordScalar);
    uniforms.SetMatrix4x4Value("u_modelView", mv.m_data);
    uniforms.SetFloatValue("u_params",
                           static_cast<float>(cos(screen.GetAngle())),
                           static_cast<float>(sin(screen.GetAngle())),
                           pixelHalfWidth);
    dp::ApplyUniforms(uniforms, program);

    renderData.m_bucket->Render(false /* draw as line */);
  }
}

void TransitSchemeRenderer::RenderText(ScreenBase const & screen, ref_ptr<gpu::ProgramManager> mng,
                                       dp::UniformValuesStorage const & commonUniforms)
{
  GLFunctions::glDisable(gl_const::GLDepthTest);
  auto const & params = df::VisualParams::Instance().GetGlyphVisualParams();
  for (auto & renderData : m_textRenderData)
  {
    ref_ptr<dp::GpuProgram> program = mng->GetProgram(renderData.m_state.GetProgram<gpu::Program>());
    program->Bind();
    dp::ApplyState(renderData.m_state, program);

    dp::UniformValuesStorage uniforms = commonUniforms;
    math::Matrix<float, 4, 4> mv = screen.GetModelView(renderData.m_pivot, kShapeCoordScalar);
    uniforms.SetMatrix4x4Value("u_modelView", mv.m_data);
    uniforms.SetFloatValue("u_opacity", 1.0);

    uniforms.SetFloatValue("u_contrastGamma", params.m_outlineContrast, params.m_outlineGamma);
    uniforms.SetFloatValue("u_isOutlinePass", 1.0f);
    dp::ApplyUniforms(uniforms, program);

    renderData.m_bucket->Render(false /* draw as line */);

    uniforms.SetFloatValue("u_contrastGamma", params.m_contrast, params.m_gamma);
    uniforms.SetFloatValue("u_isOutlinePass", 0.0f);
    dp::ApplyUniforms(uniforms, program);

    renderData.m_bucket->Render(false /* draw as line */);

    renderData.m_bucket->RenderDebug(screen);
  }
}

void TransitSchemeRenderer::RenderStubs(ScreenBase const & screen, ref_ptr<gpu::ProgramManager> mng,
                                        dp::UniformValuesStorage const & commonUniforms)
{
  for (auto & renderData : m_colorSymbolRenderData)
  {
    auto program = mng->GetProgram(renderData.m_state.GetProgram<gpu::Program>());
    program->Bind();
    dp::ApplyState(renderData.m_state, program);

    dp::UniformValuesStorage uniforms = commonUniforms;
    math::Matrix<float, 4, 4> mv = screen.GetModelView(renderData.m_pivot, kShapeCoordScalar);
    uniforms.SetMatrix4x4Value("u_modelView", mv.m_data);
    uniforms.SetFloatValue("u_opacity", 1.0);
    dp::ApplyUniforms(uniforms, program);

    GLFunctions::glEnable(gl_const::GLDepthTest);
    renderData.m_bucket->Render(false /* draw as line */);

    renderData.m_bucket->RenderDebug(screen);
  }
}
}  // namespace df
