#include "drape_frontend/traffic_renderer.hpp"
#include "drape_frontend/color_constants.hpp"
#include "drape_frontend/visual_params.hpp"

#include "drape/glsl_func.hpp"
#include "drape/shader_def.hpp"
#include "drape/vertex_array_buffer.hpp"

#include "indexer/map_style_reader.hpp"
#include "indexer/scales.hpp"

#include "base/logging.hpp"

#include "std/algorithm.hpp"

namespace df
{

namespace
{

int const kMinVisibleZoomLevel = 5;

} // namespace

TrafficRenderer::TrafficRenderer()
{
}

/*void GpsTrackRenderer::AddRenderData(ref_ptr<dp::GpuProgramManager> mng,
                                     drape_ptr<GpsTrackRenderData> && renderData)
{
  drape_ptr<GpsTrackRenderData> data = move(renderData);
  ref_ptr<dp::GpuProgram> program = mng->GetProgram(gpu::TRACK_POINT_PROGRAM);
  program->Bind();
  data->m_bucket->GetBuffer()->Build(program);
  m_renderData.push_back(move(data));
  m_waitForRenderData = false;
}*/

void TrafficRenderer::RenderTraffic(ScreenBase const & screen, int zoomLevel,
                                    ref_ptr<dp::GpuProgramManager> mng,
                                    dp::UniformValuesStorage const & commonUniforms)
{
  if (zoomLevel < kMinVisibleZoomLevel)
    return;

  /*GLFunctions::glClearDepth();

  ASSERT_LESS_OR_EQUAL(m_renderData.size(), m_handlesCache.size(), ());

  // Render points.
  dp::UniformValuesStorage uniforms = commonUniforms;
  uniforms.SetFloatValue("u_opacity", 1.0f);
  ref_ptr<dp::GpuProgram> program = mng->GetProgram(gpu::TRACK_POINT_PROGRAM);
  program->Bind();

  ASSERT_GREATER(m_renderData.size(), 0, ());
  dp::ApplyState(m_renderData.front()->m_state, program);
  dp::ApplyUniforms(uniforms, program);

  for (size_t i = 0; i < m_renderData.size(); i++)
    if (m_handlesCache[i].second != 0)
      m_renderData[i]->m_bucket->Render();*/
}

void TrafficRenderer::Clear()
{
}

} // namespace df

