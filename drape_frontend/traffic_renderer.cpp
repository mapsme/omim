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

int const kMinVisibleZoomLevel = 10;
int const kMinVisibleArrowZoomLevel = 16;

float const kTrafficArrowAspect = 64.0f / 16.0f;

float const kHalfWidthInPixel[] =
{
  // 1   2     3     4     5     6     7     8     9     10
  1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 2.0f,
  //11   12    13    14    15    16    17    18    19     20
  2.0f, 2.0f, 2.0f, 3.0f, 3.0f, 3.0f, 4.0f, 5.0f, 7.0f, 12.0f
};

float CalculateHalfWidth(ScreenBase const & screen)
{
  double const kLog2 = log(2.0);
  double const zoomLevel = my::clamp(fabs(log(screen.GetScale()) / kLog2), 1.0, scales::UPPER_STYLE_SCALE + 1.0);
  double zoom = trunc(zoomLevel);
  int const index = zoom - 1.0;
  float const lerpCoef = zoomLevel - zoom;

  float radius = 0.0f;
  if (index < scales::UPPER_STYLE_SCALE)
    radius = kHalfWidthInPixel[index] + lerpCoef * (kHalfWidthInPixel[index + 1] - kHalfWidthInPixel[index]);
  else
    radius = kHalfWidthInPixel[scales::UPPER_STYLE_SCALE];

  return radius * VisualParams::Instance().GetVisualScale();
}

} // namespace

TrafficRenderer::TrafficRenderer()
{
}

void TrafficRenderer::AddRenderData(ref_ptr<dp::GpuProgramManager> mng,
                                    vector<TrafficRenderData> && renderData)
{
  if (renderData.empty())
    return;

  size_t const startIndex = m_renderData.size();
  m_renderData.reserve(m_renderData.size() + renderData.size());
  move(renderData.begin(), renderData.end(), std::back_inserter(m_renderData));
  for (size_t i = startIndex; i < m_renderData.size(); i++)
  {
    ref_ptr<dp::GpuProgram> program = mng->GetProgram(m_renderData[i].m_state.GetProgramIndex());
    program->Bind();
    m_renderData[i].m_bucket->GetBuffer()->Build(program);
  }
}

void TrafficRenderer::UpdateTraffic(vector<TrafficSegmentData> const & trafficData)
{
  for (TrafficSegmentData const & segment : trafficData)
  {
    auto it = m_texCoords.find(segment.m_speedBucket);
    if (it == m_texCoords.end())
      continue;
    TrafficHandle * handle = FindHandle(segment.m_id);
    if (handle != nullptr)
      handle->SetTexCoord(it->second);
  }
}

TrafficHandle * TrafficRenderer::FindHandle(string const & segmentId) const
{
  for (TrafficRenderData const & renderData : m_renderData)
  {
    for (size_t i = 0; i < renderData.m_bucket->GetOverlayHandlesCount(); i++)
    {
      TrafficHandle * handle = static_cast<TrafficHandle *>(renderData.m_bucket->GetOverlayHandle(i).get());
      if (handle->GetSegmentId() == segmentId)
        return handle;
    }
  }
  return nullptr;
}

void TrafficRenderer::RenderTraffic(ScreenBase const & screen, int zoomLevel,
                                    ref_ptr<dp::GpuProgramManager> mng,
                                    dp::UniformValuesStorage const & commonUniforms)
{
  if (m_renderData.empty() || zoomLevel < kMinVisibleZoomLevel)
    return;

  float const pixelHalfWidth = CalculateHalfWidth(screen);
  float const invPixelLength = 1.0f / (2.0f * pixelHalfWidth * kTrafficArrowAspect);

  GLFunctions::glClearDepth();
  for (TrafficRenderData & renderData : m_renderData)
  {
    ref_ptr<dp::GpuProgram> program = mng->GetProgram(renderData.m_state.GetProgramIndex());
    program->Bind();
    dp::ApplyState(renderData.m_state, program);

    dp::UniformValuesStorage uniforms = commonUniforms;
    uniforms.SetFloatValue("u_opacity", 1.0f);
    uniforms.SetFloatValue("u_trafficParams", pixelHalfWidth, invPixelLength,
                           zoomLevel >= kMinVisibleArrowZoomLevel ? 1.0f : 0.0f);
    dp::ApplyUniforms(uniforms, program);

    renderData.m_bucket->Render();
  }
}

void TrafficRenderer::SetTexCoords(unordered_map<int, glsl::vec2> && texCoords)
{
  m_texCoords = move(texCoords);
}

void TrafficRenderer::Clear()
{
  m_renderData.clear();
}

} // namespace df

