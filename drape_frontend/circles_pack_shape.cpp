#include "drape_frontend/circles_pack_shape.hpp"
#include "drape_frontend/shader_def.hpp"

#include "drape/attribute_provider.hpp"
#include "drape/batcher.hpp"
#include "drape/glsl_func.hpp"
#include "drape/glsl_types.hpp"
#include "drape/texture_manager.hpp"

namespace df
{
namespace
{
uint32_t const kDynamicStreamID = 0x7F;

struct CirclesPackStaticVertex
{
  using TNormal = glsl::vec3;

  CirclesPackStaticVertex() = default;
  CirclesPackStaticVertex(TNormal const & normal) : m_normal(normal) {}

  TNormal m_normal;
};

dp::GLState GetCirclesPackState(ref_ptr<dp::TextureManager> texMng)
{
  auto state = CreateGLState(gpu::CIRCLE_POINT_PROGRAM, RenderState::OverlayLayer);
  state.SetColorTexture(texMng->GetSymbolsTexture());
  return state;
}

dp::BindingInfo const & GetCirclesPackStaticBindingInfo()
{
  static unique_ptr<dp::BindingInfo> s_info;
  if (s_info == nullptr)
  {
    dp::BindingFiller<CirclesPackStaticVertex> filler(1);
    filler.FillDecl<CirclesPackStaticVertex::TNormal>("a_normal");
    s_info.reset(new dp::BindingInfo(filler.m_info));
  }
  return *s_info;
}

dp::BindingInfo const & GetCirclesPackDynamicBindingInfo()
{
  static unique_ptr<dp::BindingInfo> s_info;
  if (s_info == nullptr)
  {
    dp::BindingFiller<CirclesPackDynamicVertex> filler(2, kDynamicStreamID);
    filler.FillDecl<CirclesPackDynamicVertex::TPosition>("a_position");
    filler.FillDecl<CirclesPackDynamicVertex::TColor>("a_color");
    s_info.reset(new dp::BindingInfo(filler.m_info));
  }
  return *s_info;
}
}  // namespace

CirclesPackHandle::CirclesPackHandle(size_t pointsCount)
  : OverlayHandle(FeatureID(), dp::Anchor::Center, 0, false)
  , m_needUpdate(false)
{
  m_buffer.resize(pointsCount * dp::Batcher::VertexPerQuad);
}

void CirclesPackHandle::GetAttributeMutation(ref_ptr<dp::AttributeBufferMutator> mutator) const
{
  if (!m_needUpdate)
    return;

  TOffsetNode const & node = GetOffsetNode(kDynamicStreamID);
  ASSERT(node.first.GetElementSize() == sizeof(CirclesPackDynamicVertex), ());
  ASSERT(node.second.m_count == m_buffer.size(), ());

  uint32_t const byteCount =
      static_cast<uint32_t>(m_buffer.size()) * sizeof(CirclesPackDynamicVertex);
  void * buffer = mutator->AllocateMutationBuffer(byteCount);
  memcpy(buffer, m_buffer.data(), byteCount);

  dp::MutateNode mutateNode;
  mutateNode.m_region = node.second;
  mutateNode.m_data = make_ref(buffer);
  mutator->AddMutation(node.first, mutateNode);

  m_needUpdate = false;
}

bool CirclesPackHandle::Update(ScreenBase const & screen)
{
  UNUSED_VALUE(screen);
  return true;
}

bool CirclesPackHandle::IndexesRequired() const { return false; }

m2::RectD CirclesPackHandle::GetPixelRect(ScreenBase const & screen, bool perspective) const
{
  UNUSED_VALUE(screen);
  UNUSED_VALUE(perspective);
  return m2::RectD();
}

void CirclesPackHandle::GetPixelShape(ScreenBase const & screen, bool perspective,
                                      Rects & rects) const
{
  UNUSED_VALUE(screen);
  UNUSED_VALUE(perspective);
}

void CirclesPackHandle::SetPoint(size_t index, m2::PointD const & position, float radius,
                                 dp::Color const & color)
{
  size_t bufferIndex = index * dp::Batcher::VertexPerQuad;
  ASSERT_GREATER_OR_EQUAL(bufferIndex, 0, ());
  ASSERT_LESS(bufferIndex, m_buffer.size(), ());

  for (size_t i = 0; i < dp::Batcher::VertexPerQuad; i++)
  {
    m_buffer[bufferIndex + i].m_position = glsl::vec3(position.x, position.y, radius);
    m_buffer[bufferIndex + i].m_color = glsl::ToVec4(color);
  }
  m_needUpdate = true;
}

void CirclesPackHandle::Clear()
{
  memset(m_buffer.data(), 0, m_buffer.size() * sizeof(CirclesPackDynamicVertex));
  m_needUpdate = true;
}

size_t CirclesPackHandle::GetPointsCount() const
{
  return m_buffer.size() / dp::Batcher::VertexPerQuad;
}

void CirclesPackShape::Draw(ref_ptr<dp::TextureManager> texMng, CirclesPackRenderData & data)
{
  ASSERT_NOT_EQUAL(data.m_pointsCount, 0, ());

  uint32_t const kVerticesInPoint = dp::Batcher::VertexPerQuad;
  uint32_t const kIndicesInPoint = dp::Batcher::IndexPerQuad;
  std::vector<CirclesPackStaticVertex> staticVertexData;
  staticVertexData.reserve(data.m_pointsCount * kVerticesInPoint);
  for (size_t i = 0; i < data.m_pointsCount; i++)
  {
    staticVertexData.emplace_back(CirclesPackStaticVertex::TNormal(-1.0f, 1.0f, 1.0f));
    staticVertexData.emplace_back(CirclesPackStaticVertex::TNormal(-1.0f, -1.0f, 1.0f));
    staticVertexData.emplace_back(CirclesPackStaticVertex::TNormal(1.0f, 1.0f, 1.0f));
    staticVertexData.emplace_back(CirclesPackStaticVertex::TNormal(1.0f, -1.0f, 1.0f));
  }

  std::vector<CirclesPackDynamicVertex> dynamicVertexData;
  dynamicVertexData.resize(data.m_pointsCount * kVerticesInPoint);

  dp::Batcher batcher(data.m_pointsCount * kIndicesInPoint, data.m_pointsCount * kVerticesInPoint);
  dp::SessionGuard guard(batcher, [&data](dp::GLState const & state, drape_ptr<dp::RenderBucket> && b)
  {
    data.m_bucket = std::move(b);
    data.m_state = state;
  });

  drape_ptr<dp::OverlayHandle> handle = make_unique_dp<CirclesPackHandle>(data.m_pointsCount);

  dp::AttributeProvider provider(2 /* stream count */,
                                 static_cast<uint32_t>(staticVertexData.size()));
  provider.InitStream(0 /* stream index */, GetCirclesPackStaticBindingInfo(),
                      make_ref(staticVertexData.data()));
  provider.InitStream(1 /* stream index */, GetCirclesPackDynamicBindingInfo(),
                      make_ref(dynamicVertexData.data()));
  batcher.InsertListOfStrip(GetCirclesPackState(texMng), make_ref(&provider), std::move(handle),
                            kVerticesInPoint);

  GLFunctions::glFlush();
}
}  // namespace df
