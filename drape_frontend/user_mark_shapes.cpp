#include "drape_frontend/user_mark_shapes.hpp"

#include "drape_frontend/line_shape.hpp"
#include "drape_frontend/map_shape.hpp"
#include "drape_frontend/shader_def.hpp"
#include "drape_frontend/shape_view_params.hpp"
#include "drape_frontend/tile_utils.hpp"
#include "drape_frontend/visual_params.hpp"

#include "drape/utils/vertex_decl.hpp"
#include "drape/attribute_provider.hpp"
#include "drape/batcher.hpp"

#include "geometry/clipping.hpp"

namespace df
{

namespace
{

template <typename TCreateVector>
void AlignFormingNormals(TCreateVector const & fn, dp::Anchor anchor,
                         dp::Anchor first, dp::Anchor second,
                         glsl::vec2 & firstNormal, glsl::vec2 & secondNormal)
{
  firstNormal = fn();
  secondNormal = -firstNormal;
  if ((anchor & second) != 0)
  {
    firstNormal *= 2;
    secondNormal = glsl::vec2(0.0, 0.0);
  }
  else if ((anchor & first) != 0)
  {
    firstNormal = glsl::vec2(0.0, 0.0);
    secondNormal *= 2;
  }
}

void AlignHorizontal(float halfWidth, dp::Anchor anchor,
                     glsl::vec2 & left, glsl::vec2 & right)
{
  AlignFormingNormals([&halfWidth]{ return glsl::vec2(-halfWidth, 0.0f); }, anchor, dp::Left, dp::Right, left, right);
}

void AlignVertical(float halfHeight, dp::Anchor anchor,
                   glsl::vec2 & up, glsl::vec2 & down)
{
  AlignFormingNormals([&halfHeight]{ return glsl::vec2(0.0f, -halfHeight); }, anchor, dp::Top, dp::Bottom, up, down);
}

struct UserPointVertex : gpu::BaseVertex
{
  UserPointVertex() = default;
  UserPointVertex(TPosition const & pos, TNormal const & normal, TTexCoord const & texCoord, bool isAnim)
    : m_position(pos)
    , m_normal(normal)
    , m_texCoord(texCoord)
    , m_isAnim(isAnim ? 1.0f : -1.0f)
  {}

  static dp::BindingInfo GetBinding()
  {
    dp::BindingInfo info(4);
    uint8_t offset = 0;
    offset += dp::FillDecl<TPosition, UserPointVertex>(0, "a_position", info, offset);
    offset += dp::FillDecl<TNormal, UserPointVertex>(1, "a_normal", info, offset);
    offset += dp::FillDecl<TTexCoord, UserPointVertex>(2, "a_colorTexCoords", info, offset);
    offset += dp::FillDecl<bool, UserPointVertex>(3, "a_animate", info, offset);

    return info;
  }

  TPosition m_position;
  TNormal m_normal;
  TTexCoord m_texCoord;
  float m_isAnim;
};

} // namespace

void CacheUserMarks(TileKey const & tileKey, ref_ptr<dp::TextureManager> textures,
                    MarkIndexesCollection const & indexes, UserMarksRenderCollection & renderParams,
                    dp::Batcher & batcher)
{
  using UPV = UserPointVertex;
  uint32_t const vertexCount = static_cast<uint32_t>(indexes.size()) * dp::Batcher::VertexPerQuad;
  buffer_vector<UPV, 128> buffer;
  buffer.reserve(vertexCount);

  dp::TextureManager::SymbolRegion region;
  for (auto const markIndex : indexes)
  {
    UserMarkRenderParams & renderInfo = renderParams[markIndex];
    if (!renderInfo.m_isVisible)
      continue;
    textures->GetSymbolRegion(renderInfo.m_symbolName, region);
    m2::RectF const & texRect = region.GetTexRect();
    m2::PointF const pxSize = region.GetPixelSize();
    dp::Anchor const anchor = renderInfo.m_anchor;
    m2::PointD const pt = MapShape::ConvertToLocal(renderInfo.m_pivot, tileKey.GetGlobalRect().Center(),
                                                   kShapeCoordScalar);
    glsl::vec3 const pos = glsl::vec3(glsl::ToVec2(pt), renderInfo.m_depth);
    bool const runAnim = renderInfo.m_runCreationAnim;
    renderInfo.m_runCreationAnim = false;

    glsl::vec2 left, right, up, down;
    AlignHorizontal(pxSize.x * 0.5f, anchor, left, right);
    AlignVertical(pxSize.y * 0.5f, anchor, up, down);

    m2::PointD const pixelOffset = renderInfo.m_pixelOffset;
    glsl::vec2 const offset(pixelOffset.x, pixelOffset.y);

    buffer.emplace_back(pos, left + down + offset, glsl::ToVec2(texRect.LeftTop()), runAnim);
    buffer.emplace_back(pos, left + up + offset, glsl::ToVec2(texRect.LeftBottom()), runAnim);
    buffer.emplace_back(pos, right + down + offset, glsl::ToVec2(texRect.RightTop()), runAnim);
    buffer.emplace_back(pos, right + up + offset, glsl::ToVec2(texRect.RightBottom()), runAnim);
  }

  dp::GLState state(gpu::BOOKMARK_PROGRAM, dp::GLState::UserMarkLayer);
  state.SetProgram3dIndex(gpu::BOOKMARK_BILLBOARD_PROGRAM);
  state.SetColorTexture(region.GetTexture());
  state.SetTextureFilter(gl_const::GLNearest);

  dp::AttributeProvider attribProvider(1, static_cast<uint32_t>(buffer.size()));
  attribProvider.InitStream(0, UPV::GetBinding(), make_ref(buffer.data()));

  batcher.InsertListOfStrip(state, make_ref(&attribProvider), dp::Batcher::VertexPerQuad);
}

void CacheUserLines(TileKey const & tileKey, ref_ptr<dp::TextureManager> textures,
                    LineIndexesCollection const & indexes, UserLinesRenderCollection & renderParams,
                    dp::Batcher & batcher)
{
  float const vs = static_cast<float>(df::VisualParams::Instance().GetVisualScale());
  for (auto lineIndex : indexes)
  {
    UserLineRenderParams & renderInfo = renderParams[lineIndex];

    auto const splines = m2::ClipSplineByRect(tileKey.GetGlobalRect(), renderInfo.m_spline);
    for (auto const & spline : splines)
    {
      for (auto const & layer : renderInfo.m_layers)
      {
        LineViewParams params;
        params.m_tileCenter = tileKey.GetGlobalRect().Center();
        params.m_baseGtoPScale = 1.0f;
        params.m_cap = dp::RoundCap;
        params.m_join = dp::RoundJoin;
        params.m_color = layer.m_color;
        params.m_depth = layer.m_depth;
        params.m_width = layer.m_width * vs;
        params.m_minVisibleScale = 1;
        params.m_rank = 0;

        LineShape(spline, params).Draw(make_ref(&batcher), textures);
      }
    }
  }
}
} // namespace df
