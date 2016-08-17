#include "drape_frontend/traffic_generator.hpp"

#include "drape_frontend/line_shape_helper.hpp"

#include "drape/attribute_provider.hpp"
#include "drape/batcher.hpp"
#include "drape/glsl_func.hpp"
#include "drape/shader_def.hpp"
#include "drape/texture_manager.hpp"

#include "base/logging.hpp"

#include "std/algorithm.hpp"

namespace df
{

namespace
{

uint32_t const kDynamicStreamID = 0x7F;
uint32_t const kSymbolPixelLength = 20;

dp::BindingInfo const & GetTrafficStaticBindingInfo()
{
  static unique_ptr<dp::BindingInfo> s_info;
  if (s_info == nullptr)
  {
    dp::BindingFiller<TrafficStaticVertex> filler(2);
    filler.FillDecl<TrafficStaticVertex::TPosition>("a_position");
    filler.FillDecl<TrafficStaticVertex::TNormal>("a_normal");
    s_info.reset(new dp::BindingInfo(filler.m_info));
  }
  return *s_info;
}

dp::BindingInfo const & GetTrafficDynamicBindingInfo()
{
  static unique_ptr<dp::BindingInfo> s_info;
  if (s_info == nullptr)
  {
    dp::BindingFiller<TrafficDynamicVertex> filler(1, kDynamicStreamID);
    filler.FillDecl<TrafficDynamicVertex::TTexCoord>("a_colorTexCoord");
    s_info.reset(new dp::BindingInfo(filler.m_info));
  }
  return *s_info;
}

void SubmitStaticVertex(glsl::vec3 const & pivot, glsl::vec2 const & normal, bool isLeft, float offsetFromStart,
                        vector<TrafficStaticVertex> & staticGeom)
{
  float const side = isLeft ? 1.0 : -1.0;
  float const offset = offsetFromStart / kSymbolPixelLength;
  staticGeom.emplace_back(TrafficStaticVertex(pivot, TrafficStaticVertex::TNormal(normal, side, offset)));
}

void SubmitDynamicVertex(glsl::vec2 const & texCoord, vector<TrafficDynamicVertex> & dynamicGeom)
{
  dynamicGeom.emplace_back(TrafficDynamicVertex(texCoord));
}

} // namespace

TrafficHandle::TrafficHandle(string const & segmentId, glsl::vec2 const & texCoord, size_t verticesCount)
  : OverlayHandle(FeatureID(), dp::Anchor::Center, 0, false)
  , m_segmentId(segmentId)
  , m_needUpdate(false)
{
  m_buffer.resize(verticesCount);
  for (size_t i = 0; i < m_buffer.size(); i++)
    m_buffer[i] = texCoord;
}

void TrafficHandle::GetAttributeMutation(ref_ptr<dp::AttributeBufferMutator> mutator) const
{
  if (!m_needUpdate)
    return;

  TOffsetNode const & node = GetOffsetNode(kDynamicStreamID);
  ASSERT(node.first.GetElementSize() == sizeof(TrafficDynamicVertex), ());
  ASSERT(node.second.m_count == m_buffer.size(), ());

  uint32_t const byteCount = m_buffer.size() * sizeof(TrafficDynamicVertex);
  void * buffer = mutator->AllocateMutationBuffer(byteCount);
  memcpy(buffer, m_buffer.data(), byteCount);

  dp::MutateNode mutateNode;
  mutateNode.m_region = node.second;
  mutateNode.m_data = make_ref(buffer);
  mutator->AddMutation(node.first, mutateNode);

  m_needUpdate = false;
}

bool TrafficHandle::Update(ScreenBase const & screen)
{
  UNUSED_VALUE(screen);
  return true;
}

bool TrafficHandle::IndexesRequired() const
{
  return false;
}

m2::RectD TrafficHandle::GetPixelRect(ScreenBase const & screen, bool perspective) const
{
  UNUSED_VALUE(screen);
  UNUSED_VALUE(perspective);
  return m2::RectD();
}

void TrafficHandle::GetPixelShape(ScreenBase const & screen, bool perspective, Rects & rects) const
{
  UNUSED_VALUE(screen);
  UNUSED_VALUE(perspective);
}

void TrafficHandle::SetTexCoord(glsl::vec2 const & texCoord)
{
  for (size_t i = 0; i < m_buffer.size(); i++)
    m_buffer[i] = texCoord;
  m_needUpdate = true;
}

string const & TrafficHandle::GetSegmentId() const
{
  return m_segmentId;
}

void TrafficGenerator::AddSegment(string const & segmentId, m2::PolylineD const & polyline)
{
  m_segments.insert(make_pair(segmentId, polyline));
}

void TrafficGenerator::ClearCache()
{
  m_colorsCache.clear();
  m_segmentsCache.clear();
}

vector<TrafficSegmentData> TrafficGenerator::GetSegmentsToUpdate(vector<TrafficSegmentData> const & trafficData) const
{
  vector<TrafficSegmentData> result;
  for (TrafficSegmentData const & segment : trafficData)
  {
    if (m_segmentsCache.find(segment.m_id) != m_segmentsCache.end())
      result.push_back(segment);
  }
  return result;
}

void TrafficGenerator::GetTrafficGeom(ref_ptr<dp::TextureManager> textures,
                                      vector<TrafficSegmentData> const & trafficData,
                                      vector<TrafficRenderData> & data)
{
  FillColorsCache(textures);

  dp::TextureManager::SymbolRegion symbolRegion;
  textures->GetSymbolRegion("route-arrow", symbolRegion); //TODO: correct arrow
  m2::RectF const texRect = symbolRegion.GetTexRect();

  dp::GLState state(gpu::TRAFFIC_PROGRAM, dp::GLState::GeometryLayer);
  state.SetColorTexture(m_colorsCache[TrafficSpeedBucket::Normal].GetTexture());
  state.SetMaskTexture(symbolRegion.GetTexture());

  uint32_t const kBatchSize = 15000;
  dp::Batcher batcher(kBatchSize, kBatchSize);
  dp::SessionGuard guard(batcher, [&data, &texRect](dp::GLState const & state, drape_ptr<dp::RenderBucket> && b)
  {
    TrafficRenderData bucket(state);
    bucket.m_bucket = move(b);
    bucket.m_texRect = texRect;
    data.emplace_back(move(bucket));
  });

  for (TrafficSegmentData const & segment : trafficData)
  {
    // Check if a segment hasn't been added.
    auto it = m_segments.find(segment.m_id);
    if (it == m_segments.end())
      continue;

    // Check if a segment has already generated.
    if (m_segmentsCache.find(segment.m_id) != m_segmentsCache.end())
      continue;

    m_segmentsCache.insert(segment.m_id);

    dp::TextureManager::ColorRegion const & colorRegion = m_colorsCache[segment.m_speedBucket];

    m2::PolylineD const & polyline = it->second;
    vector<TrafficStaticVertex> staticGeometry;
    vector<TrafficDynamicVertex> dynamicGeometry;
    GenerateSegment(colorRegion, polyline, staticGeometry, dynamicGeometry);

    glsl::vec2 const uv = glsl::ToVec2(colorRegion.GetTexRect().Center());
    drape_ptr<dp::OverlayHandle> handle = make_unique_dp<TrafficHandle>(segment.m_id, uv, staticGeometry.size());

    dp::AttributeProvider provider(2 /* stream count */, staticGeometry.size());
    provider.InitStream(0 /* stream index */, GetTrafficStaticBindingInfo(), make_ref(staticGeometry.data()));
    provider.InitStream(1 /* stream index */, GetTrafficDynamicBindingInfo(), make_ref(dynamicGeometry.data()));
    batcher.InsertListOfStrip(state, make_ref(&provider), move(handle), 4);
  }

  GLFunctions::glFlush();
}

void TrafficGenerator::GenerateSegment(dp::TextureManager::ColorRegion const & colorRegion,
                                       m2::PolylineD const & polyline,
                                       vector<TrafficStaticVertex> & staticGeometry,
                                       vector<TrafficDynamicVertex> & dynamicGeometry)
{
  vector<m2::PointD> const & path = polyline.GetPoints();
  ASSERT_GREATER(path.size(), 1, ());

  size_t const kAverageSize = path.size() * 4;
  staticGeometry.reserve(kAverageSize);
  dynamicGeometry.reserve(kAverageSize);

  // Build geometry.
  for (size_t i = 1; i < path.size(); ++i)
  {
    if (path[i].EqualDxDy(path[i - 1], 1.0E-5))
      continue;

    glsl::vec2 const p1 = glsl::vec2(path[i - 1].x, path[i - 1].y);
    glsl::vec2 const p2 = glsl::vec2(path[i].x, path[i].y);
    glsl::vec2 tangent, leftNormal, rightNormal;
    CalculateTangentAndNormals(p1, p2, tangent, leftNormal, rightNormal);

    float const kDepth = 0.0f;
    int const kSteps = 1;
    float const maskSize = glsl::length(p2 - p1);
    float currentSize = 0.0f;
    glsl::vec3 currentStartPivot = glsl::vec3(p1, kDepth);
    for (int step = 0; step < kSteps; step++)
    {
      currentSize += maskSize;
      glsl::vec3 const newPivot = glsl::vec3(p1 + tangent * currentSize, kDepth);

      SubmitStaticVertex(currentStartPivot, rightNormal, false /* isLeft */, 0.0f, staticGeometry);
      SubmitStaticVertex(currentStartPivot, leftNormal, true /* isLeft */, 0.0f, staticGeometry);
      SubmitStaticVertex(newPivot, rightNormal, false /* isLeft */, maskSize, staticGeometry);
      SubmitStaticVertex(newPivot, leftNormal, true /* isLeft */, maskSize, staticGeometry);

      for (int j = 0; j < 4; j++)
        SubmitDynamicVertex(glsl::ToVec2(colorRegion.GetTexRect().Center()), dynamicGeometry);

      currentStartPivot = newPivot;
    }
  }
}

void TrafficGenerator::FillColorsCache(ref_ptr<dp::TextureManager> textures)
{
  //TODO: get colors from colors constants.
  if (m_colorsCache.empty())
  {
    dp::TextureManager::ColorRegion colorRegion;
    textures->GetColorRegion(dp::Color::Red(), colorRegion);
    m_colorsCache[TrafficSpeedBucket::VerySlow] = colorRegion;

    textures->GetColorRegion(dp::Color(255, 255, 0, 255), colorRegion);
    m_colorsCache[TrafficSpeedBucket::Slow] = colorRegion;

    textures->GetColorRegion(dp::Color::Green(), colorRegion);
    m_colorsCache[TrafficSpeedBucket::Normal] = colorRegion;

    m_colorsCacheRefreshed = true;
  }
}

unordered_map<int, glsl::vec2> TrafficGenerator::ProcessCacheRefreshing()
{
  unordered_map<int, glsl::vec2> result;
  for (auto it = m_colorsCache.begin(); it != m_colorsCache.end(); ++it)
    result[it->first] = glsl::ToVec2(it->second.GetTexRect().Center());
  m_colorsCacheRefreshed = false;
  return result;
}

} // namespace df

