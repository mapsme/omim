#include "drape_frontend/line_shape.hpp"

#include "drape_frontend/line_shape_helper.hpp"

#include "shaders/programs.hpp"

#include "drape/attribute_provider.hpp"
#include "drape/batcher.hpp"
#include "drape/glsl_types.hpp"
#include "drape/glsl_func.hpp"
#include "drape/support_manager.hpp"
#include "drape/texture_manager.hpp"
#include "drape/utils/vertex_decl.hpp"

#include "indexer/scales.hpp"

#include "base/logging.hpp"

#include <algorithm>
#include <vector>

namespace df
{
namespace
{
size_t const kTrackArrowsMaxCount = 128;
float const kTrackArrowsStepSize = 4.0f; // distance between centers of arrow rects in units equal to its diagonals
} // namespace

struct TrackArrowPackParams
{
  size_t m_arrowCount;
  m2::SharedSpline m_spline;
  m2::PointF m_arrowPixelSize;
  m2::PointD m_tileCenter;
  float m_baseDepth;
  m2::RectD m_tileRect;
};

class TrackArrowPackHandle : public dp::OverlayHandle
{
  using TBase = dp::OverlayHandle;

public:
  explicit TrackArrowPackHandle(TrackArrowPackParams const & params)
    : OverlayHandle(FeatureID(), dp::Anchor::Center, 0 /* priority */, 1 /* minVisibleScale */, false)
    , m_params(params)
    , m_currentPtoG(0.0f)
    , m_trackArrowsDynamicGeom(m_params.m_arrowCount * dp::Batcher::IndexPerQuad)
    , m_dirty(false)
  {}

  void GetAttributeMutation(ref_ptr<dp::AttributeBufferMutator> mutator) const override
  {
    if (!m_dirty) {
      return;
    }
    m_dirty = false;

    TOffsetNode const & node = GetOffsetNode(1);
    ASSERT(node.first.GetElementSize() == sizeof(gpu::TrackArrowDynamicVertex), ());
    ASSERT(node.second.m_count == m_trackArrowsDynamicGeom.size(), ());

    uint32_t const byteCount =
        static_cast<uint32_t>(m_trackArrowsDynamicGeom.size()) * sizeof(gpu::TrackArrowDynamicVertex);
    void * buffer = mutator->AllocateMutationBuffer(byteCount);
    std::uninitialized_copy(m_trackArrowsDynamicGeom.begin(), m_trackArrowsDynamicGeom.end(),
                            static_cast<gpu::TrackArrowDynamicVertex *>(buffer));

    dp::MutateNode mutateNode;
    mutateNode.m_region = node.second;
    mutateNode.m_data = make_ref(buffer);
    mutator->AddMutation(node.first, mutateNode);
  }

  bool Update(ScreenBase const & screen) override
  {
    ConstructDynamicVertices(screen.GetScale());
    return true;
  }

  m2::RectD GetPixelRect(ScreenBase const & screen, bool perspective) const override
  {
    UNUSED_VALUE(screen);
    UNUSED_VALUE(perspective);
    return {};
  }

  void GetPixelShape(ScreenBase const & screen, bool perspective, Rects & rects) const override
  {
    UNUSED_VALUE(screen);
    UNUSED_VALUE(perspective);
    UNUSED_VALUE(rects);
  }

  bool IndexesRequired() const override
  {
    return false;
  }

  size_t GetPointsCount() const
  {
    return m_trackArrowsDynamicGeom.size() / dp::Batcher::VertexPerQuad;
  }

  dp::BindingInfo const & GetTrackArrowsDynamicBindingInfo()
  {
    return gpu::TrackArrowDynamicVertex::GetBindingInfo();
  }

  void ConstructDynamicVertices(float const currentPtoG)
  {
    if (m_currentPtoG == currentPtoG)
      return;
    m_currentPtoG = currentPtoG;

    m_trackArrowsDynamicGeom.clear();

    auto const toLocalCoord = [this] (m2::PointD const & p) -> glsl::vec2
    {
      return glsl::ToVec2(MapShape::ConvertToLocal(p, m_params.m_tileCenter, kShapeCoordScalar));
    };

    float const arrowStep = kTrackArrowsStepSize * currentPtoG * m_params.m_arrowPixelSize.Length();
    float const initialLength = m_params.m_spline->GetInitialLength();

    float arrowPos = arrowStep - std::fmod(initialLength, arrowStep);
    float depth = m_params.m_baseDepth + m_params.m_arrowCount;
    float trackLength = 0.0f;
    auto const & path = m_params.m_spline->GetPath();
    m2::PointD p1 = path.front();
    for (size_t i = 1; i < path.size(); ++i)
    {
      m2::PointD const & p2 = path[i];

      m2::PointD const dir = p2 - p1;
      float const sectionLength = dir.Length();
      glsl::vec2 const normalizedLocalDir = glsl::ToVec2(dir.Normalize());
      while (!(arrowPos < trackLength) && (arrowPos < trackLength + sectionLength))
      {
        float const fraction = (arrowPos - trackLength) / sectionLength;
        m2::PointD const p = p1 + dir * fraction;
        if (m_params.m_tileRect.IsPointInside(p))
          SubmitTrackDynamicArrow(toLocalCoord(p), depth, normalizedLocalDir);
        arrowPos += arrowStep;
      }
      depth -= 1.0f;

      trackLength += sectionLength;
      p1 = p2;
    }

    m_trackArrowsDynamicGeom.resize(m_params.m_arrowCount * dp::Batcher::IndexPerQuad);

    m_dirty = true;
  }

  size_t GetSize() const
  {
    return m_params.m_arrowCount;
  }

  ref_ptr<void> GetTrackArrowsDynamicData()
  {
    return make_ref(m_trackArrowsDynamicGeom.data());
  }

private:
  TrackArrowPackParams m_params;

  float m_currentPtoG;
  gpu::TTrackArrowDynamicVertexBuffer m_trackArrowsDynamicGeom;
  mutable bool m_dirty;

  void SubmitTrackDynamicArrow(glsl::vec2 const & pivot, float depth, glsl::vec2 direction)
  {
    if (m_params.m_arrowCount == m_trackArrowsDynamicGeom.size() / dp::Batcher::IndexPerQuad)
      return;

    glsl::vec3 const p = glsl::vec3(pivot, depth);

    auto const arrowRectPixelSize = glsl::ToVec2(m_params.m_arrowPixelSize * 0.5f);

    glsl::vec2 const dx = arrowRectPixelSize.y * glsl::vec2(direction.y, -direction.x);
    glsl::vec2 const dy = arrowRectPixelSize.x * direction;

    gpu::TrackArrowDynamicVertex const a(p,  dy - dx);
    gpu::TrackArrowDynamicVertex const b(p,  dx + dy);
    gpu::TrackArrowDynamicVertex const c(p, -dx - dy);
    gpu::TrackArrowDynamicVertex const d(p,  dx - dy);

    // winding order must be clockwise due to glFrontFace(GL_CW)
    m_trackArrowsDynamicGeom.push_back(a);
    m_trackArrowsDynamicGeom.push_back(b);
    m_trackArrowsDynamicGeom.push_back(c);

    m_trackArrowsDynamicGeom.push_back(c);
    m_trackArrowsDynamicGeom.push_back(b);
    m_trackArrowsDynamicGeom.push_back(d);
  }
};

struct LineShapeParams
{
  dp::TextureManager::ColorRegion m_color;
  dp::TextureManager::SymbolRegion m_arrow;
  float m_pxHalfWidth;
  float m_baseGtoP;
  float m_currentPtoG;
  float m_depth;
  bool m_depthTestEnabled;
  DepthLayer m_depthLayer;
  dp::LineCap m_cap;
  dp::LineJoin m_join;
  dp::TextureManager::StippleRegion m_stipple;
  m2::SharedSpline m_spline;
  m2::PointD m_tileCenter;
  m2::RectD m_tileRect;
};

class LineShapeInfo
{
public:
  LineShapeInfo(LineShapeParams const & params)
    : m_params(params)
    , m_colorCoord(glsl::ToVec2(params.m_color.GetTexRect().Center()))
  {}

  virtual ~LineShapeInfo() = default;

  virtual dp::BindingInfo const & GetBindingInfo() = 0;
  virtual dp::RenderState GetState() = 0;

  virtual ref_ptr<void> GetLineData() = 0;
  virtual uint32_t GetLineSize() = 0;

  virtual ref_ptr<void> GetJoinData() = 0;
  virtual uint32_t GetJoinSize() = 0;

  virtual dp::BindingInfo const & GetCapBindingInfo() = 0;
  virtual dp::RenderState GetCapState() = 0;
  virtual ref_ptr<void> GetCapData() = 0;
  virtual uint32_t GetCapSize() = 0;

  void ConstructTrackArrows()
  {
    m_trackArrowsStaticGeom.clear();
    m_handle.reset();

    if (m_params.m_spline.IsNull() || !m_params.m_spline->IsValid())
      return;

    TrackArrowPackParams params;
    params.m_arrowCount = kTrackArrowsMaxCount;
    params.m_spline = m_params.m_spline;
    params.m_arrowPixelSize = m_params.m_arrow.GetPixelSize();
    params.m_tileCenter = m_params.m_tileCenter;
    params.m_baseDepth = m_params.m_depth;
    params.m_tileRect = m_params.m_tileRect;

    m_handle = make_unique_dp<TrackArrowPackHandle>(params);
    m_handle->ConstructDynamicVertices(m_params.m_currentPtoG);

    auto const texRect = m_params.m_arrow.GetTexRect();
    auto const toAtlasTexCoord = [&texRect] (float const u, float const v) -> glsl::vec2
    {
      return glsl::vec2((1.0f - u) * texRect.maxX() + u * texRect.minX(), (1.0f - v) * texRect.maxY() + v * texRect.minY());
    };

    gpu::TrackArrowStaticVertex const a(toAtlasTexCoord(0.0f, 1.0f));
    gpu::TrackArrowStaticVertex const b(toAtlasTexCoord(1.0f, 1.0f));
    gpu::TrackArrowStaticVertex const c(toAtlasTexCoord(0.0f, 0.0f));
    gpu::TrackArrowStaticVertex const d(toAtlasTexCoord(1.0f, 0.0f));

    for (size_t i = 0; i < m_handle->GetSize(); ++i)
    {
      m_trackArrowsStaticGeom.push_back(a);
      m_trackArrowsStaticGeom.push_back(b);
      m_trackArrowsStaticGeom.push_back(c);

      m_trackArrowsStaticGeom.push_back(c);
      m_trackArrowsStaticGeom.push_back(b);
      m_trackArrowsStaticGeom.push_back(d);
    }
  }

  dp::BindingInfo const & GetTrackArrowsStaticBindingInfo()
  {
    return gpu::TrackArrowStaticVertex::GetBindingInfo();
  }

  dp::RenderState GetTrackArrowsState()
  {
    auto state = CreateRenderState(gpu::Program::TrackArrow, m_params.m_depthLayer);
    state.SetColorTexture(m_params.m_arrow.GetTexture());
    state.SetTextureIndex(m_params.m_arrow.GetTextureIndex());
    state.SetDepthTestEnabled(m_params.m_depthTestEnabled);
    return state;
  }

  ref_ptr<void> GetTrackArrowsStaticData()
  {
    return make_ref(m_trackArrowsStaticGeom.data());
  }

  uint32_t GetTrackArrowsSize()
  {
    return static_cast<uint32_t>(m_trackArrowsStaticGeom.size());
  }

  drape_ptr<TrackArrowPackHandle> & GetHandle()
  {
    return m_handle;
  }

protected:
  LineShapeParams m_params;
  glsl::vec2 const m_colorCoord;

private:
  gpu::TTrackArrowStaticVertexBuffer m_trackArrowsStaticGeom;
  drape_ptr<TrackArrowPackHandle> m_handle;
};

namespace
{
class TextureCoordGenerator
{
public:
  explicit TextureCoordGenerator(dp::TextureManager::StippleRegion const & region)
    : m_region(region)
    , m_maskLength(static_cast<float>(m_region.GetMaskPixelLength()))
  {}

  glsl::vec4 GetTexCoordsByDistance(float distance) const
  {
    return GetTexCoords(distance / m_maskLength);
  }

  glsl::vec4 GetTexCoords(float offset) const
  {
    m2::RectF const & texRect = m_region.GetTexRect();
    return glsl::vec4(offset, texRect.minX(), texRect.SizeX(), texRect.Center().y);
  }

  float GetMaskLength() const
  {
    return m_maskLength;
  }

  dp::TextureManager::StippleRegion const & GetRegion() const
  {
    return m_region;
  }

private:
  dp::TextureManager::StippleRegion const m_region;
  float const m_maskLength;
};

template <typename TVertex>
class BaseLineBuilder : public LineShapeInfo
{
public:
  BaseLineBuilder(LineShapeParams const & params, size_t geomsSize, size_t joinsSize)
    : LineShapeInfo(params)
  {
    m_geometry.reserve(geomsSize);
    m_joinGeom.reserve(joinsSize);
  }

  dp::BindingInfo const & GetBindingInfo() override
  {
    return TVertex::GetBindingInfo();
  }

  ref_ptr<void> GetLineData() override
  {
    return make_ref(m_geometry.data());
  }

  uint32_t GetLineSize() override
  {
    return static_cast<uint32_t>(m_geometry.size());
  }

  ref_ptr<void> GetJoinData() override
  {
    return make_ref(m_joinGeom.data());
  }

  uint32_t GetJoinSize() override
  {
    return static_cast<uint32_t>(m_joinGeom.size());
  }

  float GetHalfWidth()
  {
    return m_params.m_pxHalfWidth;
  }

  dp::BindingInfo const & GetCapBindingInfo() override
  {
    return GetBindingInfo();
  }

  dp::RenderState GetCapState() override
  {
    return GetState();
  }

  ref_ptr<void> GetCapData() override
  {
    return ref_ptr<void>();
  }

  uint32_t GetCapSize() override
  {
    return 0;
  }

  float GetSide(bool isLeft) const
  {
    return isLeft ? 1.0f : -1.0f;
  }

protected:
  using V = TVertex;
  using TGeometryBuffer = std::vector<V>;

  TGeometryBuffer m_geometry;
  TGeometryBuffer m_joinGeom;
};

class SolidLineBuilder : public BaseLineBuilder<gpu::LineVertex>
{
  using TBase = BaseLineBuilder<gpu::LineVertex>;
  using TNormal = gpu::LineVertex::TNormal;

  struct CapVertex
  {
    using TPosition = gpu::LineVertex::TPosition;
    using TNormal = gpu::LineVertex::TNormal;
    using TTexCoord = gpu::LineVertex::TTexCoord;

    CapVertex() {}
    CapVertex(TPosition const & pos, TNormal const & normal, TTexCoord const & color)
      : m_position(pos)
      , m_normal(normal)
      , m_color(color)
    {}

    TPosition m_position;
    TNormal m_normal;
    TTexCoord m_color;
  };

  using TCapBuffer = std::vector<CapVertex>;

public:
  SolidLineBuilder(LineShapeParams const & params, size_t pointsInSpline)
    : TBase(params, pointsInSpline * 2, (pointsInSpline - 2) * 8)
  {}

  dp::RenderState GetState() override
  {
    auto state = CreateRenderState(gpu::Program::Line, m_params.m_depthLayer);
    state.SetColorTexture(m_params.m_color.GetTexture());
    state.SetDepthTestEnabled(m_params.m_depthTestEnabled);
    return state;
  }

  dp::BindingInfo const & GetCapBindingInfo() override
  {
    if (m_params.m_cap == dp::ButtCap)
      return TBase::GetCapBindingInfo();

    static std::unique_ptr<dp::BindingInfo> s_capInfo;
    if (s_capInfo == nullptr)
    {
      dp::BindingFiller<CapVertex> filler(3);
      filler.FillDecl<CapVertex::TPosition>("a_position");
      filler.FillDecl<CapVertex::TNormal>("a_normal");
      filler.FillDecl<CapVertex::TTexCoord>("a_colorTexCoords");

      s_capInfo.reset(new dp::BindingInfo(filler.m_info));
    }

    return *s_capInfo;
  }

  dp::RenderState GetCapState() override
  {
    if (m_params.m_cap == dp::ButtCap)
      return TBase::GetCapState();

    auto state = CreateRenderState(gpu::Program::CapJoin, m_params.m_depthLayer);
    state.SetDepthTestEnabled(m_params.m_depthTestEnabled);
    state.SetColorTexture(m_params.m_color.GetTexture());
    state.SetDepthFunction(dp::TestFunction::Less);
    return state;
  }

  ref_ptr<void> GetCapData() override
  {
    return make_ref<void>(m_capGeometry.data());
  }

  uint32_t GetCapSize() override
  {
    return static_cast<uint32_t>(m_capGeometry.size());
  }

  void SubmitVertex(glsl::vec3 const & pivot, glsl::vec2 const & normal, bool isLeft)
  {
    float const halfWidth = GetHalfWidth();
    m_geometry.emplace_back(V(pivot, TNormal(halfWidth * normal, halfWidth * GetSide(isLeft)), m_colorCoord));
  }

  void SubmitJoin(glsl::vec2 const & pos)
  {
    if (m_params.m_join == dp::RoundJoin)
      CreateRoundCap(pos);
  }

  void SubmitCap(glsl::vec2 const & pos)
  {
    if (m_params.m_cap != dp::ButtCap)
      CreateRoundCap(pos);
  }

private:
  void CreateRoundCap(glsl::vec2 const & pos)
  {
    // Here we use an equilateral triangle to render circle (incircle of a triangle).
    static float const kSqrt3 = sqrt(3.0f);
    float const radius = GetHalfWidth();

    m_capGeometry.reserve(3);
    m_capGeometry.push_back(CapVertex(CapVertex::TPosition(pos, m_params.m_depth),
                                      CapVertex::TNormal(-radius * kSqrt3, -radius, radius),
                                      CapVertex::TTexCoord(m_colorCoord)));
    m_capGeometry.push_back(CapVertex(CapVertex::TPosition(pos, m_params.m_depth),
                                      CapVertex::TNormal(radius * kSqrt3, -radius, radius),
                                      CapVertex::TTexCoord(m_colorCoord)));
    m_capGeometry.push_back(CapVertex(CapVertex::TPosition(pos, m_params.m_depth),
                                      CapVertex::TNormal(0, 2.0f * radius, radius),
                                      CapVertex::TTexCoord(m_colorCoord)));
  }

private:
  TCapBuffer m_capGeometry;
};

class SimpleSolidLineBuilder : public BaseLineBuilder<gpu::AreaVertex>
{
  using TBase = BaseLineBuilder<gpu::AreaVertex>;

public:
  SimpleSolidLineBuilder(LineShapeParams const & params, size_t pointsInSpline, int lineWidth)
    : TBase(params, pointsInSpline, 0)
    , m_lineWidth(lineWidth)
  {}

  dp::RenderState GetState() override
  {
    auto state = CreateRenderState(gpu::Program::AreaOutline, m_params.m_depthLayer);
    state.SetDepthTestEnabled(m_params.m_depthTestEnabled);
    state.SetColorTexture(m_params.m_color.GetTexture());
    state.SetDrawAsLine(true);
    state.SetLineWidth(m_lineWidth);
    return state;
  }

  void SubmitVertex(glsl::vec3 const & pivot)
  {
    m_geometry.emplace_back(V(pivot, m_colorCoord));
  }

private:
  int m_lineWidth;
};

class DashedLineBuilder : public BaseLineBuilder<gpu::DashedLineVertex>
{
  using TBase = BaseLineBuilder<gpu::DashedLineVertex>;
  using TNormal = gpu::LineVertex::TNormal;

public:
  DashedLineBuilder(LineShapeParams const & params, size_t pointsInSpline)
    : TBase(params, pointsInSpline * 8, (pointsInSpline - 2) * 8)
    , m_texCoordGen(params.m_stipple)
  {}

  int GetDashesCount(float const globalLength) const
  {
    float const pixelLen = globalLength * m_params.m_baseGtoP;
    return static_cast<int>((pixelLen + m_texCoordGen.GetMaskLength() - 1) / m_texCoordGen.GetMaskLength());
  }

  dp::RenderState GetState() override
  {
    auto state = CreateRenderState(gpu::Program::DashedLine, m_params.m_depthLayer);
    state.SetDepthTestEnabled(m_params.m_depthTestEnabled);
    state.SetColorTexture(m_params.m_color.GetTexture());
    state.SetMaskTexture(m_texCoordGen.GetRegion().GetTexture());
    return state;
  }

  void SubmitVertex(glsl::vec3 const & pivot, glsl::vec2 const & normal, bool isLeft, float offsetFromStart)
  {
    float const halfWidth = GetHalfWidth();
    m_geometry.emplace_back(V(pivot, TNormal(halfWidth * normal, halfWidth * GetSide(isLeft)),
                              m_colorCoord, m_texCoordGen.GetTexCoordsByDistance(offsetFromStart)));
  }

private:
  TextureCoordGenerator m_texCoordGen;
};
}  // namespace

LineShape::LineShape(m2::SharedSpline const & spline, LineViewParams const & params, bool showArrows)
  : m_showArrows(showArrows)
  , m_params(params)
  , m_spline(spline)
  , m_isSimple(false)
{
  ASSERT_GREATER(m_spline->GetPath().size(), 1, ());
}

LineShape::~LineShape() = default;

template <typename TBuilder>
void LineShape::Construct(TBuilder & builder) const
{
  ASSERT(false, ("No implementation"));
}

// Specialization optimized for dashed lines.
template <>
void LineShape::Construct<DashedLineBuilder>(DashedLineBuilder & builder) const
{
  std::vector<m2::PointD> const & path = m_spline->GetPath();
  ASSERT_GREATER(path.size(), 1, ());

  // build geometry
  for (size_t i = 1; i < path.size(); ++i)
  {
    if (path[i].EqualDxDy(path[i - 1], 1.0E-5))
      continue;

    glsl::vec2 const p1 = glsl::ToVec2(ConvertToLocal(path[i - 1], m_params.m_tileCenter, kShapeCoordScalar));
    glsl::vec2 const p2 = glsl::ToVec2(ConvertToLocal(path[i], m_params.m_tileCenter, kShapeCoordScalar));
    glsl::vec2 tangent, leftNormal, rightNormal;
    CalculateTangentAndNormals(p1, p2, tangent, leftNormal, rightNormal);

    // calculate number of steps to cover line segment
    float const initialGlobalLength = static_cast<float>((path[i] - path[i - 1]).Length());
    int const steps = std::max(1, builder.GetDashesCount(initialGlobalLength));
    float const maskSize = glsl::length(p2 - p1) / steps;
    float const offsetSize = initialGlobalLength / steps;

    // generate vertices
    float currentSize = 0;
    glsl::vec3 currentStartPivot = glsl::vec3(p1, m_params.m_depth);
    for (int step = 0; step < steps; step++)
    {
      currentSize += maskSize;
      glsl::vec3 const newPivot = glsl::vec3(p1 + tangent * currentSize, m_params.m_depth);

      builder.SubmitVertex(currentStartPivot, rightNormal, false /* isLeft */, 0.0);
      builder.SubmitVertex(currentStartPivot, leftNormal, true /* isLeft */, 0.0);
      builder.SubmitVertex(newPivot, rightNormal, false /* isLeft */, offsetSize);
      builder.SubmitVertex(newPivot, leftNormal, true /* isLeft */, offsetSize);

      currentStartPivot = newPivot;
    }
  }
}

// Specialization optimized for solid lines.
template <>
void LineShape::Construct<SolidLineBuilder>(SolidLineBuilder & builder) const
{
  std::vector<m2::PointD> const & path = m_spline->GetPath();
  ASSERT_GREATER(path.size(), 1, ());

  // skip joins generation
  float const kJoinsGenerationThreshold = 2.5f;
  bool generateJoins = true;
  if (builder.GetHalfWidth() <= kJoinsGenerationThreshold)
    generateJoins = false;

  // build geometry
  glsl::vec2 firstPoint = glsl::ToVec2(ConvertToLocal(path.front(), m_params.m_tileCenter, kShapeCoordScalar));
  glsl::vec2 lastPoint;
  bool hasConstructedSegments = false;
  for (size_t i = 1; i < path.size(); ++i)
  {
    if (path[i].EqualDxDy(path[i - 1], 1.0E-5))
      continue;

    glsl::vec2 const p1 = glsl::ToVec2(ConvertToLocal(path[i - 1], m_params.m_tileCenter, kShapeCoordScalar));
    glsl::vec2 const p2 = glsl::ToVec2(ConvertToLocal(path[i], m_params.m_tileCenter, kShapeCoordScalar));
    glsl::vec2 tangent, leftNormal, rightNormal;
    CalculateTangentAndNormals(p1, p2, tangent, leftNormal, rightNormal);

    glsl::vec3 const startPoint = glsl::vec3(p1, m_params.m_depth);
    glsl::vec3 const endPoint = glsl::vec3(p2, m_params.m_depth);

    builder.SubmitVertex(startPoint, rightNormal, false /* isLeft */);
    builder.SubmitVertex(startPoint, leftNormal, true /* isLeft */);
    builder.SubmitVertex(endPoint, rightNormal, false /* isLeft */);
    builder.SubmitVertex(endPoint, leftNormal, true /* isLeft */);

    // generate joins
    if (generateJoins && i < path.size() - 1)
      builder.SubmitJoin(p2);

    lastPoint = p2;
    hasConstructedSegments = true;
  }

  if (hasConstructedSegments)
  {
    builder.SubmitCap(firstPoint);
    builder.SubmitCap(lastPoint);
  }
}

// Specialization optimized for simple solid lines.
template <>
void LineShape::Construct<SimpleSolidLineBuilder>(SimpleSolidLineBuilder & builder) const
{
  std::vector<m2::PointD> const & path = m_spline->GetPath();
  ASSERT_GREATER(path.size(), 1, ());

  // Build geometry.
  for (size_t i = 0; i < path.size(); ++i)
  {
    glsl::vec2 const p = glsl::ToVec2(ConvertToLocal(path[i], m_params.m_tileCenter, kShapeCoordScalar));
    builder.SubmitVertex(glsl::vec3(p, m_params.m_depth));
  }
}

bool LineShape::CanBeSimplified(int & lineWidth) const
{
  // Disable simplification for world map.
  if (m_params.m_zoomLevel > 0 && m_params.m_zoomLevel <= scales::GetUpperCountryScale())
    return false;

  static float width = std::min(2.5f, dp::SupportManager::Instance().GetMaxLineWidth());
  if (m_params.m_width <= width)
  {
    lineWidth = std::max(1, static_cast<int>(m_params.m_width));
    return true;
  }

  lineWidth = 1;
  return false;
}

void LineShape::Prepare(ref_ptr<dp::TextureManager> textures) const
{
  dp::TextureManager::ColorRegion colorRegion;
  textures->GetColorRegion(m_params.m_color, colorRegion);

  dp::TextureManager::SymbolRegion symbolRegion;
  ASSERT(textures->HasSymbolRegion("track-arrow"), ());
  textures->GetSymbolRegion("track-arrow", symbolRegion);

  LineShapeParams p;
  p.m_cap = m_params.m_cap;
  p.m_color = colorRegion;
  p.m_arrow = symbolRegion;
  p.m_depthTestEnabled = m_params.m_depthTestEnabled;
  p.m_depth = m_params.m_depth;
  p.m_depthLayer = m_params.m_depthLayer;
  p.m_join = m_params.m_join;
  p.m_pxHalfWidth = m_params.m_width / 2.0f;
  p.m_baseGtoP = m_params.m_baseGtoPScale;
  p.m_currentPtoG = m_params.m_currentPtoGScale;
  p.m_spline = m_spline;
  p.m_tileCenter = m_params.m_tileCenter;
  p.m_tileRect = m_params.m_tileRect;

  if (m_params.m_pattern.empty())
  {
    int lineWidth = 1;
    m_isSimple = CanBeSimplified(lineWidth);
    if (m_isSimple)
    {
      auto builder =
          std::make_unique<SimpleSolidLineBuilder>(p, m_spline->GetPath().size(), lineWidth);
      Construct<SimpleSolidLineBuilder>(*builder);
      m_lineShapeInfo = move(builder);
    }
    else
    {
      auto builder = std::make_unique<SolidLineBuilder>(p, m_spline->GetPath().size());
      Construct<SolidLineBuilder>(*builder);
      m_lineShapeInfo = move(builder);
    }
  }
  else
  {
    dp::TextureManager::StippleRegion maskRegion;
    textures->GetStippleRegion(m_params.m_pattern, maskRegion);

    p.m_stipple = maskRegion;

    auto builder = std::make_unique<DashedLineBuilder>(p, m_spline->GetPath().size());
    Construct<DashedLineBuilder>(*builder);
    m_lineShapeInfo = move(builder);
  }
}

void LineShape::Draw(ref_ptr<dp::GraphicsContext> context, ref_ptr<dp::Batcher> batcher,
                     ref_ptr<dp::TextureManager> textures) const
{
  if (!m_lineShapeInfo)
    Prepare(textures);
  if (m_showArrows)
    m_lineShapeInfo->ConstructTrackArrows();

  ASSERT(m_lineShapeInfo != nullptr, ());
  dp::RenderState state = m_lineShapeInfo->GetState();
  dp::AttributeProvider provider(1, m_lineShapeInfo->GetLineSize());
  provider.InitStream(0, m_lineShapeInfo->GetBindingInfo(), m_lineShapeInfo->GetLineData());
  if (!m_isSimple)
  {
    batcher->InsertListOfStrip(context, state, make_ref(&provider), dp::Batcher::VertexPerQuad);

    uint32_t const joinSize = m_lineShapeInfo->GetJoinSize();
    if (joinSize > 0)
    {
      dp::AttributeProvider joinsProvider(1, joinSize);
      joinsProvider.InitStream(0, m_lineShapeInfo->GetBindingInfo(), m_lineShapeInfo->GetJoinData());
      batcher->InsertTriangleList(context, state, make_ref(&joinsProvider));
    }

    uint32_t const capSize = m_lineShapeInfo->GetCapSize();
    if (capSize > 0)
    {
      dp::AttributeProvider capProvider(1, capSize);
      capProvider.InitStream(0, m_lineShapeInfo->GetCapBindingInfo(), m_lineShapeInfo->GetCapData());
      batcher->InsertTriangleList(context, m_lineShapeInfo->GetCapState(), make_ref(&capProvider));
    }
  }
  else
  {
    batcher->InsertLineStrip(context, state, make_ref(&provider));
  }
  uint32_t const trackSize = m_lineShapeInfo->GetTrackArrowsSize();
  if (trackSize > 0)
  {
    auto & handle = m_lineShapeInfo->GetHandle();

    dp::AttributeProvider trackArrowProvider(2 /* stream count */, trackSize);
    trackArrowProvider.InitStream(0 /* stream index */, m_lineShapeInfo->GetTrackArrowsStaticBindingInfo(), m_lineShapeInfo->GetTrackArrowsStaticData());
    trackArrowProvider.InitStream(1 /* stream index */, handle->GetTrackArrowsDynamicBindingInfo(), handle->GetTrackArrowsDynamicData());
    batcher->InsertTriangleList(context, m_lineShapeInfo->GetTrackArrowsState(), make_ref(&trackArrowProvider), std::move(handle));
  }
}

}  // namespace df

