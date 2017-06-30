#include "drape_frontend/text_layout.hpp"
#include "drape_frontend/map_shape.hpp"
#include "drape_frontend/visual_params.hpp"

#include "drape/bidi.hpp"
#include "drape/glsl_func.hpp"
#include "drape/overlay_handle.hpp"

#include <algorithm>
#include <iterator>
#include <numeric>

namespace df
{
namespace
{
float const kValidSplineTurn = 0.96f;

class TextGeometryGenerator
{
public:
  TextGeometryGenerator(dp::TextureManager::ColorRegion const & color,
                        gpu::TTextStaticVertexBuffer & buffer)
    : m_colorCoord(glsl::ToVec2(color.GetTexRect().Center()))
    , m_buffer(buffer)
  {}

  void operator() (dp::TextureManager::GlyphRegion const & glyph)
  {
    m2::RectF const & mask = glyph.GetTexRect();

    using TSV = gpu::TextStaticVertex;
    m_buffer.emplace_back(TSV(m_colorCoord, glsl::ToVec2(mask.LeftTop())));
    m_buffer.emplace_back(TSV(m_colorCoord, glsl::ToVec2(mask.LeftBottom())));
    m_buffer.emplace_back(TSV(m_colorCoord, glsl::ToVec2(mask.RightTop())));
    m_buffer.emplace_back(TSV(m_colorCoord, glsl::ToVec2(mask.RightBottom())));
  }

protected:
  glsl::vec2 m_colorCoord;
  gpu::TTextStaticVertexBuffer & m_buffer;
};

template<typename TParentGenerator>
class StraightTextGeometryGenerator : public TParentGenerator
{
public:
  template<typename... TParentGeneratorParams>
  StraightTextGeometryGenerator(glsl::vec4 const & pivot,
                                glsl::vec2 const & pixelOffset, float textRatio,
                                gpu::TTextDynamicVertexBuffer & dynBuffer,
                                TParentGeneratorParams & ... parentGeneratorParams)
    : TParentGenerator(parentGeneratorParams...)
    , m_pivot(pivot)
    , m_penPosition(pixelOffset)
    , m_buffer(dynBuffer)
    , m_textRatio(textRatio)
  {}

  void operator()(dp::TextureManager::GlyphRegion const & glyph)
  {
    if (!glyph.IsValid())
      return;
    m2::PointF pixelSize = glyph.GetPixelSize() * m_textRatio;

    float const xOffset = glyph.GetOffsetX() * m_textRatio;
    float const yOffset = glyph.GetOffsetY() * m_textRatio;

    float const upVector = -static_cast<int32_t>(pixelSize.y) - yOffset;
    float const bottomVector = -yOffset;

    if (m_isFirstGlyph)
    {
      m_isFirstGlyph = false;
      m_penPosition.x -= (xOffset + dp::kSdfBorder * m_textRatio);
    }

    using TDV = gpu::TextDynamicVertex;
    m_buffer.emplace_back(TDV(m_pivot, m_penPosition + glsl::vec2(xOffset, bottomVector)));
    m_buffer.emplace_back(TDV(m_pivot, m_penPosition + glsl::vec2(xOffset, upVector)));
    m_buffer.emplace_back(TDV(m_pivot, m_penPosition + glsl::vec2(pixelSize.x + xOffset, bottomVector)));
    m_buffer.emplace_back(TDV(m_pivot, m_penPosition + glsl::vec2(pixelSize.x + xOffset, upVector)));
    m_penPosition += glsl::vec2(glyph.GetAdvanceX() * m_textRatio, glyph.GetAdvanceY() * m_textRatio);

    TParentGenerator::operator()(glyph);
  }

private:
  glsl::vec4 const & m_pivot;
  glsl::vec2 m_penPosition;
  gpu::TTextDynamicVertexBuffer & m_buffer;
  float m_textRatio = 0.0f;
  bool m_isFirstGlyph = true;
};

class TextOutlinedGeometryGenerator
{
public:
  TextOutlinedGeometryGenerator(dp::TextureManager::ColorRegion const & color,
                                dp::TextureManager::ColorRegion const & outline,
                                gpu::TTextOutlinedStaticVertexBuffer & buffer)
    : m_colorCoord(glsl::ToVec2(color.GetTexRect().Center()))
    , m_outlineCoord(glsl::ToVec2(outline.GetTexRect().Center()))
    , m_buffer(buffer)
  {}

  void operator() (dp::TextureManager::GlyphRegion const & glyph)
  {
    using TOSV = gpu::TextOutlinedStaticVertex;
    m2::RectF const & mask = glyph.GetTexRect();
    m_buffer.emplace_back(TOSV(m_colorCoord, m_outlineCoord, glsl::ToVec2(mask.LeftTop())));
    m_buffer.emplace_back(TOSV(m_colorCoord, m_outlineCoord, glsl::ToVec2(mask.LeftBottom())));
    m_buffer.emplace_back(TOSV(m_colorCoord, m_outlineCoord, glsl::ToVec2(mask.RightTop())));
    m_buffer.emplace_back(TOSV(m_colorCoord, m_outlineCoord, glsl::ToVec2(mask.RightBottom())));
  }

protected:
  glsl::vec2 m_colorCoord;
  glsl::vec2 m_outlineCoord;
  gpu::TTextOutlinedStaticVertexBuffer & m_buffer;
};

void SplitText(strings::UniString & visText, buffer_vector<size_t, 2> & delimIndexes)
{
  char const * delims = " \n\t";
  size_t count = visText.size();
  if (count > 15)
  {
    // split on two parts
    typedef strings::UniString::iterator TIter;
    auto const iMiddle = visText.begin() + count / 2;

    size_t const delimsSize = strlen(delims);

    // find next delimeter after middle [m, e)
    auto iNext = std::find_first_of(iMiddle,
                                    visText.end(),
                                    delims, delims + delimsSize);

    // find last delimeter before middle [b, m)
    auto iPrev = std::find_first_of(std::reverse_iterator<TIter>(iMiddle),
                                    std::reverse_iterator<TIter>(visText.begin()),
                                    delims, delims + delimsSize).base();
    // don't do split like this:
    //     xxxx
    // xxxxxxxxxxxx
    if (4 * std::distance(visText.begin(), iPrev) <= static_cast<long>(count))
      iPrev = visText.end();
    else
      --iPrev;

    // get closest delimiter to the middle
    if (iNext == visText.end() ||
        (iPrev != visText.end() && std::distance(iPrev, iMiddle) < std::distance(iMiddle, iNext)))
    {
      iNext = iPrev;
    }

    // split string on 2 parts
    if (iNext != visText.end())
    {
      ASSERT_NOT_EQUAL(iNext, visText.begin(), ());
      TIter delimSymbol = iNext;
      TIter secondPart = iNext + 1;

      delimIndexes.push_back(static_cast<size_t>(std::distance(visText.begin(), delimSymbol)));

      if (secondPart != visText.end())
      {
        strings::UniString result(visText.begin(), delimSymbol);
        result.append(secondPart, visText.end());
        visText = result;
        delimIndexes.push_back(visText.size());
      }
      return;
    }
  }

  delimIndexes.push_back(count);
}

class XLayouter
{
public:
  XLayouter(dp::Anchor anchor)
    : m_anchor(anchor)
  {}

  float operator()(float currentLength, float maxLength)
  {
    ASSERT_GREATER_OR_EQUAL(maxLength, currentLength, ());

    if (m_anchor & dp::Left)
      return 0.0;
    else if (m_anchor & dp::Right)
      return -currentLength;
    else
      return -(currentLength / 2.0f);
  }

private:
  dp::Anchor m_anchor;
};

class YLayouter
{
public:
  YLayouter(dp::Anchor anchor, float summaryHeight)
  {
    if (anchor & dp::Top)
      m_penOffset = 0.0f;
    else if (anchor & dp::Bottom)
      m_penOffset = -summaryHeight;
    else
      m_penOffset = -(summaryHeight / 2.0f);
  }

  float operator()(float currentHeight)
  {
    m_penOffset += currentHeight;
    return m_penOffset;
  }

private:
    float m_penOffset;
};

void CalculateOffsets(dp::Anchor anchor, float textRatio,
                      dp::TextureManager::TGlyphsBuffer const & glyphs,
                      buffer_vector<size_t, 2> const & delimIndexes,
                      buffer_vector<pair<size_t, glsl::vec2>, 2> & result,
                      m2::PointF & pixelSize)
{
  typedef pair<float, float> TLengthAndHeight;
  buffer_vector<TLengthAndHeight, 2> lengthAndHeight;
  float maxLength = 0;
  float summaryHeight = 0;

  size_t start = 0;
  for (size_t index = 0; index < delimIndexes.size(); ++index)
  {
    size_t end = delimIndexes[index];
    ASSERT_NOT_EQUAL(start, end, ());
    lengthAndHeight.push_back(TLengthAndHeight(0, 0));
    TLengthAndHeight & node = lengthAndHeight.back();
    for (size_t glyphIndex = start; glyphIndex < end && glyphIndex < glyphs.size(); ++glyphIndex)
    {
      dp::TextureManager::GlyphRegion const & glyph = glyphs[glyphIndex];
      if (!glyph.IsValid())
        continue;

      if (glyphIndex == start)
        node.first -= glyph.GetOffsetX() * textRatio;

      node.first += glyph.GetAdvanceX() * textRatio;

      float yAdvance = glyph.GetAdvanceY();
      if (glyph.GetOffsetY() < 0)
        yAdvance += glyph.GetOffsetY();

      node.second = max(node.second, (glyph.GetPixelHeight() + yAdvance) * textRatio);
    }
    maxLength = max(maxLength, node.first);
    summaryHeight += node.second;
    start = end;
  }

  ASSERT_EQUAL(delimIndexes.size(), lengthAndHeight.size(), ());

  XLayouter xL(anchor);
  YLayouter yL(anchor, summaryHeight);
  for (size_t index = 0; index < delimIndexes.size(); ++index)
  {
    TLengthAndHeight const & node = lengthAndHeight[index];
    result.push_back(make_pair(delimIndexes[index], glsl::vec2(xL(node.first, maxLength),
                                                               yL(node.second))));
  }

  pixelSize = m2::PointF(maxLength, summaryHeight);
}

float GetTextMinPeriod(float pixelTextLength)
{
  float const vs = static_cast<float>(df::VisualParams::Instance().GetVisualScale());
  float const etalonEmpty = std::max(300.0f * vs, pixelTextLength);
  return etalonEmpty + pixelTextLength;
}
}  // namespace

void TextLayout::Init(strings::UniString const & text, float fontSize, bool isSdf,
                      ref_ptr<dp::TextureManager> textures)
{
  m_text = text;
  float const fontScale = static_cast<float>(VisualParams::Instance().GetFontScale());
  float const baseSize = static_cast<float>(VisualParams::Instance().GetGlyphBaseSize());
  m_textSizeRatio = isSdf ? (fontSize * fontScale / baseSize) : 1.0f;
  m_fixedHeight = isSdf ? dp::GlyphManager::kDynamicGlyphSize
                        : static_cast<int>(fontSize * fontScale);
  textures->GetGlyphRegions(text, m_fixedHeight, m_metrics);
}

ref_ptr<dp::Texture> TextLayout::GetMaskTexture() const
{
  ASSERT(!m_metrics.empty(), ());
#ifdef DEBUG
  ref_ptr<dp::Texture> tex = m_metrics[0].GetTexture();
  for (GlyphRegion const & g : m_metrics)
  {
    ASSERT(g.GetTexture() == tex, ());
  }
#endif

  return m_metrics[0].GetTexture();
}

uint32_t TextLayout::GetGlyphCount() const
{
  return static_cast<uint32_t>(m_metrics.size());
}

float TextLayout::GetPixelLength() const
{
  return m_textSizeRatio * std::accumulate(m_metrics.begin(), m_metrics.end(), 0.0f,
                                           [](double const & v, GlyphRegion const & glyph) -> float
  {
    return static_cast<float>(v) + glyph.GetAdvanceX();
  });
}

float TextLayout::GetPixelHeight() const
{
  return m_fixedHeight > 0 ? m_fixedHeight
                           : m_textSizeRatio * VisualParams::Instance().GetGlyphBaseSize();
}

strings::UniString const & TextLayout::GetText() const
{
  return m_text;
}

StraightTextLayout::StraightTextLayout(strings::UniString const & text, float fontSize, bool isSdf,
                                       ref_ptr<dp::TextureManager> textures, dp::Anchor anchor)
{
  strings::UniString visibleText = bidi::log2vis(text);
  buffer_vector<size_t, 2> delimIndexes;
  if (visibleText == text)
    SplitText(visibleText, delimIndexes);
  else
    delimIndexes.push_back(visibleText.size());

  TBase::Init(visibleText, fontSize, isSdf, textures);
  CalculateOffsets(anchor, m_textSizeRatio, m_metrics, delimIndexes, m_offsets, m_pixelSize);
}

void StraightTextLayout::Cache(glm::vec4 const & pivot, glm::vec2 const & pixelOffset,
                               dp::TextureManager::ColorRegion const & colorRegion,
                               dp::TextureManager::ColorRegion const & outlineRegion,
                               gpu::TTextOutlinedStaticVertexBuffer & staticBuffer,
                               gpu::TTextDynamicVertexBuffer & dynamicBuffer) const
{
  size_t beginOffset = 0;
  for (pair<size_t, glsl::vec2> const & node : m_offsets)
  {
    StraightTextGeometryGenerator<TextOutlinedGeometryGenerator> generator(
          pivot, pixelOffset + node.second, m_textSizeRatio, dynamicBuffer,
          colorRegion, outlineRegion, staticBuffer);

    size_t const endOffset = node.first;
    for (size_t index = beginOffset; index < endOffset && index < m_metrics.size(); ++index)
      generator(m_metrics[index]);

    beginOffset = endOffset;
  }
}

void StraightTextLayout::Cache(glm::vec4 const & pivot, glm::vec2 const & pixelOffset,
                               dp::TextureManager::ColorRegion const & color,
                               gpu::TTextStaticVertexBuffer & staticBuffer,
                               gpu::TTextDynamicVertexBuffer & dynamicBuffer) const
{
  size_t beginOffset = 0;
  for (pair<size_t, glsl::vec2> const & node : m_offsets)
  {
    StraightTextGeometryGenerator<TextGeometryGenerator> generator(
          pivot, pixelOffset + node.second, m_textSizeRatio, dynamicBuffer,
          color, staticBuffer);

    size_t const endOffset = node.first;
    for (size_t index = beginOffset; index < endOffset && index < m_metrics.size(); ++index)
      generator(m_metrics[index]);

    beginOffset = endOffset;
  }
}

PathTextLayout::PathTextLayout(m2::PointD const & tileCenter, strings::UniString const & text,
                               float fontSize, bool isSdf, ref_ptr<dp::TextureManager> textures)
  : m_tileCenter(tileCenter)
{
  Init(bidi::log2vis(text), fontSize, isSdf, textures);
}

void PathTextLayout::CacheStaticGeometry(dp::TextureManager::ColorRegion const & colorRegion,
                                         dp::TextureManager::ColorRegion const & outlineRegion,
                                         gpu::TTextOutlinedStaticVertexBuffer & staticBuffer) const
{
  TextOutlinedGeometryGenerator gen(colorRegion, outlineRegion, staticBuffer);
  std::for_each(m_metrics.begin(), m_metrics.end(), gen);
}

void PathTextLayout::CacheStaticGeometry(dp::TextureManager::ColorRegion const & colorRegion,
                                         gpu::TTextStaticVertexBuffer & staticBuffer) const
{
  TextGeometryGenerator gen(colorRegion, staticBuffer);
  std::for_each(m_metrics.begin(), m_metrics.end(), gen);
}

bool PathTextLayout::CacheDynamicGeometry(m2::Spline::iterator const & iter, float depth,
                                          m2::PointD const & globalPivot,
                                          gpu::TTextDynamicVertexBuffer & buffer) const
{
  using TDV = gpu::TextDynamicVertex;

  float const halfLength = 0.5f * GetPixelLength();

  m2::Spline::iterator beginIter = iter;
  beginIter.Advance(-halfLength);
  m2::Spline::iterator endIter = iter;
  endIter.Advance(halfLength);
  if (beginIter.BeginAgain() || endIter.BeginAgain())
    return false;

  float const halfFontSize = 0.5f * GetPixelHeight();
  float advanceSign = 1.0f;
  m2::Spline::iterator penIter = beginIter;
  if (beginIter.m_pos.x > endIter.m_pos.x)
  {
    advanceSign = -advanceSign;
    penIter = endIter;
  }

  glsl::vec2 pxPivot = glsl::ToVec2(iter.m_pos);
  buffer.resize(4 * m_metrics.size());

  glsl::vec4 const pivot(glsl::ToVec2(MapShape::ConvertToLocal(globalPivot, m_tileCenter,
                                                               kShapeCoordScalar)), depth, 0.0f);
  for (size_t i = 0; i < m_metrics.size(); ++i)
  {
    GlyphRegion const & g = m_metrics[i];
    m2::PointF pxSize = g.GetPixelSize() * m_textSizeRatio;

    m2::PointD const pxBase = penIter.m_pos;
    m2::PointD const pxShiftBase = penIter.m_pos + penIter.m_dir;

    glsl::vec2 tangent = advanceSign * glsl::normalize(glsl::ToVec2(pxShiftBase - pxBase));
    glsl::vec2 normal = glsl::normalize(glsl::vec2(-tangent.y, tangent.x));
    glsl::vec2 formingVector = (glsl::ToVec2(pxBase) - pxPivot) + (halfFontSize * normal);

    float const xOffset = g.GetOffsetX() * m_textSizeRatio;
    float const yOffset = g.GetOffsetY() * m_textSizeRatio;

    float const upVector = - (pxSize.y + yOffset);
    float const bottomVector = - yOffset;

    size_t baseIndex = 4 * i;

    buffer[baseIndex + 0] = TDV(pivot, formingVector + normal * bottomVector + tangent * xOffset);
    buffer[baseIndex + 1] = TDV(pivot, formingVector + normal * upVector + tangent * xOffset);
    buffer[baseIndex + 2] = TDV(pivot, formingVector + normal * bottomVector + tangent * (pxSize.x + xOffset));
    buffer[baseIndex + 3] = TDV(pivot, formingVector + normal * upVector + tangent * (pxSize.x + xOffset));

    float const xAdvance = g.GetAdvanceX() * m_textSizeRatio;
    glsl::vec2 currentTangent = glsl::ToVec2(penIter.m_dir);
    penIter.Advance(advanceSign * xAdvance);
    float const dotProduct = glsl::dot(currentTangent, glsl::ToVec2(penIter.m_dir));
    if (dotProduct < kValidSplineTurn)
      return false;
  }
  return true;
}

float PathTextLayout::CalculateTextLength(float textPixelLength)
{
  // We leave a little space on each side of the text.
  float const kTextBorder = 4.0f;
  return kTextBorder + textPixelLength;
}

bool PathTextLayout::CalculatePerspectivePosition(float pixelSplineLength, float textPixelLength,
                                                  uint32_t textIndex, float & offset)
{
  UNUSED_VALUE(textIndex);
  float const textLength = CalculateTextLength(textPixelLength);
  if (textLength > pixelSplineLength * 2.0f)
    return false;
  offset = pixelSplineLength * 0.5f;
  return true;
}

void PathTextLayout::CalculatePositions(float splineLength, float splineScaleToPixel,
                                        float textPixelLength, std::vector<float> & offsets)
{
  float const textLength = CalculateTextLength(textPixelLength);

  // On the next scale m_scaleGtoP will be twice.
  if (textLength > splineLength * 2.0f * splineScaleToPixel)
    return;

  float const kPathLengthScalar = 0.75;
  float const pathLength = kPathLengthScalar * splineScaleToPixel * splineLength;
  float const minPeriodSize = GetTextMinPeriod(textLength);
  float const twoTextsAndEmpty = minPeriodSize + textLength;

  if (pathLength < twoTextsAndEmpty)
  {
    // If we can't place 2 texts and empty part along the path,
    // we place only one text at the center of the path.
    offsets.push_back(splineLength * 0.5f);
  }
  else
  {
    double const textCount = std::max(floor(static_cast<double>(pathLength / minPeriodSize)), 1.0);
    double const glbTextLen = splineLength / textCount;
    for (double offset = 0.5 * glbTextLen; offset < splineLength; offset += glbTextLen)
      offsets.push_back(static_cast<float>(offset));
  }
}

SharedTextLayout::SharedTextLayout(PathTextLayout * layout)
  : m_layout(layout)
{}

bool SharedTextLayout::IsNull() const
{
  return m_layout == nullptr;
}

void SharedTextLayout::Reset(PathTextLayout * layout)
{
  m_layout.reset(layout);
}

PathTextLayout * SharedTextLayout::GetRaw()
{
  return m_layout.get();
}

PathTextLayout * SharedTextLayout::operator->()
{
  return m_layout.get();
}

PathTextLayout const * SharedTextLayout::operator->() const
{
  return m_layout.get();
}
}  // namespace df
