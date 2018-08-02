#include "transit_scheme_builder.hpp"

#include "drape_frontend/color_constants.hpp"
#include "drape_frontend/colored_symbol_shape.hpp"
#include "drape_frontend/line_shape_helper.hpp"
#include "drape_frontend/map_shape.hpp"
#include "drape_frontend/render_state_extension.hpp"
#include "drape_frontend/shape_view_params.hpp"
#include "drape_frontend/text_layout.hpp"
#include "drape_frontend/text_shape.hpp"
#include "drape_frontend/visual_params.hpp"

#include "shaders/programs.hpp"

#include "drape/batcher.hpp"
#include "drape/glsl_func.hpp"
#include "drape/glsl_types.hpp"
#include "drape/render_bucket.hpp"
#include "drape/utils/vertex_decl.hpp"

#include "base/string_utils.hpp"

using namespace std;

namespace df
{
int const kTransitSchemeMinZoomLevel = 10;
float const kTransitLineHalfWidth = 0.8f;
std::vector<float> const kTransitLinesWidthInPixel =
{
  // 1   2     3     4     5     6     7     8     9    10
  1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.25f,
  //11  12    13    14    15    16    17    18    19     20
  1.65f, 2.0f, 2.5f, 3.0f, 3.5f, 4.3f, 5.0f, 5.5f, 5.8f, 5.8f
};

namespace
{
float constexpr kBaseLineDepth = 0.0f;
float constexpr kDepthPerLine = 1.0f;
float constexpr kBaseMarkerDepth = 300.0f;
int constexpr kFinalStationMinZoomLevel = 10;
int constexpr kTransferMinZoomLevel = 11;
int constexpr kStopMinZoomLevel = 12;
uint16_t constexpr kFinalStationPriorityInc = 2;

float constexpr kOuterMarkerDepth = kBaseMarkerDepth + 0.5f;
float constexpr kInnerMarkerDepth = kBaseMarkerDepth + 1.0f;
uint32_t constexpr kTransitStubOverlayIndex = 1000;
uint32_t constexpr kTransitOverlayIndex = 1001;

std::string const kTransitMarkText = "TransitMarkPrimaryText";
std::string const kTransitMarkTextOutline = "TransitMarkPrimaryTextOutline";
std::string const kTransitTransferOuterColor = "TransitTransferOuterMarker";
std::string const kTransitTransferInnerColor = "TransitTransferInnerMarker";
std::string const kTransitStopInnerColor = "TransitStopInnerMarker";

float const kTransitMarkTextSize = 11.0f;

struct TransitStaticVertex
{
  using TPosition = glsl::vec3;
  using TNormal = glsl::vec4;
  using TColor = glsl::vec4;

  TransitStaticVertex() = default;
  TransitStaticVertex(TPosition const & position, TNormal const & normal,
                          TColor const & color)
    : m_position(position), m_normal(normal), m_color(color) {}

  TPosition m_position;
  TNormal m_normal;
  TColor m_color;
};

struct SchemeSegment
{
  glsl::vec2 m_p1;
  glsl::vec2 m_p2;
  glsl::vec2 m_tangent;
  glsl::vec2 m_leftNormal;
  glsl::vec2 m_rightNormal;
};
using TGeometryBuffer = std::vector<TransitStaticVertex>;

dp::BindingInfo const & GetTransitStaticBindingInfo()
{
  static unique_ptr<dp::BindingInfo> s_info;
  if (s_info == nullptr)
  {
    dp::BindingFiller<TransitStaticVertex> filler(3);
    filler.FillDecl<TransitStaticVertex::TPosition>("a_position");
    filler.FillDecl<TransitStaticVertex::TNormal>("a_normal");
    filler.FillDecl<TransitStaticVertex::TColor>("a_color");
    s_info.reset(new dp::BindingInfo(filler.m_info));
  }
  return *s_info;
}

void GenerateLineCaps(std::vector<SchemeSegment> const & segments, glsl::vec4 const & color,
                      float lineOffset, float halfWidth, float depth, dp::Batcher & batcher)
{
  using TV = TransitStaticVertex;

  TGeometryBuffer geometry;
  geometry.reserve(segments.size() * 3);

  for (auto const & segment : segments)
  {
    // Here we use an equilateral triangle to render a circle (incircle of a triangle).
    static float const kSqrt3 = sqrt(3.0f);
    auto const offset = lineOffset * segment.m_rightNormal;
    auto const pivot = glsl::vec3(segment.m_p2, depth);

    auto const n1 = glsl::vec2(-2.0 * halfWidth, -halfWidth);
    auto const n2 = glsl::vec2(2.0 * halfWidth, -halfWidth);
    auto const n3 = glsl::vec2(0.0f, (2.0f * kSqrt3 - 1.0f) * halfWidth);

    geometry.emplace_back(pivot, TV::TNormal(-offset, n1), color);
    geometry.emplace_back(pivot, TV::TNormal(-offset, n2), color);
    geometry.emplace_back(pivot, TV::TNormal(-offset, n3), color);
  }

  dp::AttributeProvider provider(1 /* stream count */, static_cast<uint32_t>(geometry.size()));
  provider.InitStream(0 /* stream index */, GetTransitStaticBindingInfo(), make_ref(geometry.data()));
  auto state = CreateRenderState(gpu::Program::TransitCircle, DepthLayer::TransitSchemeLayer);
  batcher.InsertTriangleList(state, make_ref(&provider));
}

struct TitleInfo
{
  TitleInfo() = default;
  TitleInfo(std::string const & text)
    : m_text(text)
  {}

  std::string m_text;
  size_t m_rowsCount = 0;
  m2::PointF m_pixelSize;
  m2::PointF m_offset;
  dp::Anchor m_anchor = dp::Left;
};

std::vector<TitleInfo> PlaceTitles(StopNodeParams const & stopParams, float textSize,
                                   ref_ptr<dp::TextureManager> textures)
{
  std::vector<TitleInfo> titles;
  for (auto const & stopInfo : stopParams.m_stopsInfo)
  {
    if (stopInfo.second.m_name.empty())
      continue;
    bool isUnique = true;
    for (auto const & title : titles)
    {
      if (title.m_text == stopInfo.second.m_name)
      {
        isUnique = false;
        break;
      }
    }
    if (isUnique)
      titles.emplace_back(stopInfo.second.m_name);
  }

  if (titles.size() > 1)
  {
    auto const vs = static_cast<float>(df::VisualParams::Instance().GetVisualScale());

    size_t summaryRowsCount = 0;
    for (auto & name : titles)
    {
      df::StraightTextLayout layout(strings::MakeUniString(name.m_text), textSize,
                                    false /* isSdf */, textures, dp::Left);
      name.m_pixelSize = layout.GetPixelSize() + m2::PointF(4.0f * vs, 4.0f * vs);
      name.m_rowsCount = layout.GetRowsCount();
      summaryRowsCount += layout.GetRowsCount();
    }

    auto const rightRowsCount = summaryRowsCount > 3 ? (summaryRowsCount + 1) / 2 : summaryRowsCount;
    float rightHeight = 0.0f;
    float leftHeight = 0.0f;
    size_t rowsCount = 0;
    size_t rightTitlesCount = 0;
    for (size_t i = 0; i < titles.size(); ++i)
    {
      if (rowsCount < rightRowsCount)
      {
        rightHeight += titles[i].m_pixelSize.y;
        ++rightTitlesCount;
      }
      else
      {
        leftHeight += titles[i].m_pixelSize.y;
      }
      rowsCount += titles[i].m_rowsCount;
    }

    float currentOffset = -rightHeight / 2.0f;
    for (size_t i = 0; i < rightTitlesCount; ++i)
    {
      titles[i].m_anchor = dp::Left;
      titles[i].m_offset.y = currentOffset + titles[i].m_pixelSize.y / 2.0f;
      currentOffset += titles[i].m_pixelSize.y;
    }

    currentOffset = -leftHeight / 2.0f;
    for (size_t i = rightTitlesCount; i < titles.size(); ++i)
    {
      titles[i].m_anchor = dp::Right;
      titles[i].m_offset.y = currentOffset + titles[i].m_pixelSize.y / 2.0f;
      currentOffset += titles[i].m_pixelSize.y;
    }
  }
  return titles;
}

vector<m2::PointF> GetTransitMarkerSizes(float markerScale, float maxRouteWidth)
{
  auto const vs = static_cast<float>(df::VisualParams::Instance().GetVisualScale());
  vector<m2::PointF> markerSizes;
  markerSizes.reserve(df::kTransitLinesWidthInPixel.size());
  for (auto const halfWidth : df::kTransitLinesWidthInPixel)
  {
    float const d = 2.0f * std::min(halfWidth * vs, maxRouteWidth * 0.5f) * markerScale;
    markerSizes.push_back(m2::PointF(d, d));
  }
  return markerSizes;
}

uint32_t GetRouteId(routing::transit::LineId lineId)
{
  return static_cast<uint32_t>(lineId >> 4);
}

void FillStopParams(TransitDisplayInfo const & transitDisplayInfo, MwmSet::MwmId const & mwmId,
                    routing::transit::Stop const & stop, StopNodeParams & stopParams)
{
  FeatureID featureId;
  std::string title;
  if (stop.GetFeatureId() != routing::transit::kInvalidFeatureId)
  {
    featureId = FeatureID(mwmId, stop.GetFeatureId());
    title = transitDisplayInfo.m_features.at(featureId).m_title;
  }
  stopParams.m_isTransfer = false;
  stopParams.m_pivot = stop.GetPoint();
  for (auto lineId : stop.GetLineIds())
  {
    StopInfo & info = stopParams.m_stopsInfo[GetRouteId(lineId)];
    info.m_featureId = featureId;
    info.m_name = title;
    info.m_lines.insert(lineId);
  }
}

bool FindLongerPath(routing::transit::StopId stop1Id, routing::transit::StopId stop2Id,
                    std::vector<routing::transit::StopId> const & sameStops, size_t & stop1Ind, size_t & stop2Ind)
{
  stop1Ind = std::numeric_limits<size_t>::max();
  stop2Ind = std::numeric_limits<size_t>::max();

  for (size_t stopInd = 0; stopInd < sameStops.size(); ++stopInd)
  {
    if (sameStops[stopInd] == stop1Id)
      stop1Ind = stopInd;
    else if (sameStops[stopInd] == stop2Id)
      stop2Ind = stopInd;
  }

  if (stop1Ind < sameStops.size() || stop2Ind < sameStops.size())
  {
    if (stop1Ind > stop2Ind)
      std::swap(stop1Ind, stop2Ind);

    if (stop1Ind < sameStops.size() && stop2Ind < sameStops.size() && stop2Ind - stop1Ind > 1)
      return true;
  }
  return false;
}
}  // namespace

void TransitSchemeBuilder::UpdateSchemes(TransitDisplayInfos const & transitDisplayInfos,
                                         ref_ptr<dp::TextureManager> textures)
{
  for (auto const & mwmInfo : transitDisplayInfos)
  {
    if (!mwmInfo.second)
      continue;

    auto const & mwmId = mwmInfo.first;
    auto const & transitDisplayInfo = *mwmInfo.second.get();

    MwmSchemeData & scheme = m_schemes[mwmId];

    CollectStops(transitDisplayInfo, mwmId, scheme);
    CollectLines(transitDisplayInfo, scheme);
    CollectShapes(transitDisplayInfo, scheme);

    PrepareScheme(scheme);
    BuildScheme(mwmId, textures);
  }
}

void TransitSchemeBuilder::Clear()
{
  m_schemes.clear();
}

void TransitSchemeBuilder::Clear(MwmSet::MwmId const & mwmId)
{
  m_schemes.erase(mwmId);
}

void TransitSchemeBuilder::RebuildSchemes(ref_ptr<dp::TextureManager> textures)
{
  for (auto const & mwmScheme : m_schemes)
    BuildScheme(mwmScheme.first, textures);
}

void TransitSchemeBuilder::BuildScheme(MwmSet::MwmId const & mwmId, ref_ptr<dp::TextureManager> textures)
{
  if (m_schemes.find(mwmId) == m_schemes.end())
    return;
  ++m_recacheId;
  GenerateShapes(mwmId);
  GenerateStops(mwmId, textures);
}

void TransitSchemeBuilder::CollectStops(TransitDisplayInfo const & transitDisplayInfo,
                                        MwmSet::MwmId const & mwmId, MwmSchemeData & scheme)
{
  for (auto const & stopInfo : transitDisplayInfo.m_stops)
  {
    routing::transit::Stop const & stop = stopInfo.second;
    if (stop.GetTransferId() != routing::transit::kInvalidTransferId)
      continue;
    auto & stopNode = scheme.m_stops[stop.GetId()];
    FillStopParams(transitDisplayInfo, mwmId, stop, stopNode);
  }

  for (auto const & stopInfo : transitDisplayInfo.m_transfers)
  {
    routing::transit::Transfer const & transfer = stopInfo.second;
    auto & stopNode = scheme.m_transfers[transfer.GetId()];

    for (auto stopId : transfer.GetStopIds())
    {
      if (transitDisplayInfo.m_stops.find(stopId) == transitDisplayInfo.m_stops.end())
      {
        LOG(LWARNING, ("Invalid stop", stopId, "in transfer", transfer.GetId()));
        continue;
      }
      routing::transit::Stop const & stop = transitDisplayInfo.m_stops.at(stopId);
      FillStopParams(transitDisplayInfo, mwmId, stop, stopNode);
    }
    stopNode.m_isTransfer = true;
    stopNode.m_pivot = transfer.GetPoint();
  }
}

void TransitSchemeBuilder::CollectLines(TransitDisplayInfo const & transitDisplayInfo, MwmSchemeData & scheme)
{
  std::multimap<size_t, routing::transit::LineId> linesLengths;
  for (auto const & line : transitDisplayInfo.m_lines)
  {
    auto const lineId = line.second.GetId();
    size_t stopsCount = 0;
    auto const & stopsRanges = line.second.GetStopIds();
    for (auto const & stops : stopsRanges)
      stopsCount += stops.size();
    linesLengths.insert(std::make_pair(stopsCount, lineId));
  }
  float depth = kBaseLineDepth;
  for (auto const & pair : linesLengths)
  {
    auto const lineId = pair.second;
    scheme.m_lines[lineId] = LineParams(transitDisplayInfo.m_lines.at(lineId).GetColor(), depth);
    depth += kDepthPerLine;
  }
}

void TransitSchemeBuilder::CollectShapes(TransitDisplayInfo const & transitDisplayInfo, MwmSchemeData & scheme)
{
  std::map<uint32_t, std::vector<routing::transit::LineId>> roads;
  for (auto const & line : transitDisplayInfo.m_lines)
  {
    auto const lineId = line.second.GetId();
    auto const roadId = GetRouteId(lineId);
    roads[roadId].push_back(lineId);
  }

  for (auto const & line : transitDisplayInfo.m_lines)
  {
    auto const lineId = line.second.GetId();
    auto const roadId = GetRouteId(lineId);

    auto const & stopsRanges = line.second.GetStopIds();
    for (auto const & stops : stopsRanges)
    {
      for (size_t i = 1; i < stops.size(); ++i)
        FindShapes(stops[i - 1], stops[i], lineId, roads[roadId], transitDisplayInfo, scheme);
    }
  }
}

void TransitSchemeBuilder::FindShapes(routing::transit::StopId stop1Id, routing::transit::StopId stop2Id,
                                      routing::transit::LineId lineId,
                                      std::vector<routing::transit::LineId> const & sameLines,
                                      TransitDisplayInfo const & transitDisplayInfo, MwmSchemeData & scheme)
{
  bool shapeAdded = false;

  for (auto const & sameLineId : sameLines)
  {
    if (sameLineId == lineId)
      continue;

    auto const & sameLine = transitDisplayInfo.m_lines.at(sameLineId);
    auto const & sameStopsRanges = sameLine.GetStopIds();
    for (auto const & sameStops : sameStopsRanges)
    {
      size_t stop1Ind;
      size_t stop2Ind;
      if (FindLongerPath(stop1Id, stop2Id, sameStops, stop1Ind, stop2Ind))
      {
        shapeAdded = true;
        for (size_t stopInd = stop1Ind; stopInd < stop2Ind; ++stopInd)
          AddShape(transitDisplayInfo, sameStops[stopInd], sameStops[stopInd + 1], lineId, scheme);
      }
      if (stop1Ind < sameStops.size() || stop2Ind < sameStops.size())
        break;
    }

    if (shapeAdded)
      break;
  }

  if (!shapeAdded)
    AddShape(transitDisplayInfo, stop1Id, stop2Id, lineId, scheme);
}

void TransitSchemeBuilder::AddShape(TransitDisplayInfo const & transitDisplayInfo,
                                    routing::transit::StopId stop1Id,
                                    routing::transit::StopId stop2Id,
                                    routing::transit::LineId lineId,
                                    MwmSchemeData & scheme)
{
  auto const stop1It = transitDisplayInfo.m_stops.find(stop1Id);
  ASSERT(stop1It != transitDisplayInfo.m_stops.end(), ());

  auto const stop2It = transitDisplayInfo.m_stops.find(stop2Id);
  ASSERT(stop2It != transitDisplayInfo.m_stops.end(), ());

  auto const transfer1Id = stop1It->second.GetTransferId();
  auto const transfer2Id = stop2It->second.GetTransferId();

  auto shapeId = routing::transit::ShapeId(transfer1Id != routing::transit::kInvalidTransferId ? transfer1Id
                                                                                               : stop1Id,
                                           transfer2Id != routing::transit::kInvalidTransferId ? transfer2Id
                                                                                               : stop2Id);
  auto it = transitDisplayInfo.m_shapes.find(shapeId);
  bool isForward = true;
  if (it == transitDisplayInfo.m_shapes.end())
  {
    isForward = false;
    shapeId = routing::transit::ShapeId(shapeId.GetStop2Id(), shapeId.GetStop1Id());
    it = transitDisplayInfo.m_shapes.find(shapeId);
  }

  if (it == transitDisplayInfo.m_shapes.end())
    return;

  ASSERT(it != transitDisplayInfo.m_shapes.end(), ());

  auto const itScheme = scheme.m_shapes.find(shapeId);
  if (itScheme == scheme.m_shapes.end())
  {
    auto const & polyline = transitDisplayInfo.m_shapes.at(it->first).GetPolyline();
    if (isForward)
      scheme.m_shapes[shapeId].m_forwardLines.push_back(lineId);
    else
      scheme.m_shapes[shapeId].m_backwardLines.push_back(lineId);
    scheme.m_shapes[shapeId].m_polyline = polyline;
  }
  else
  {
    for (auto id : itScheme->second.m_forwardLines)
    {
      if (GetRouteId(id) == GetRouteId(lineId))
        return;
    }
    for (auto id : itScheme->second.m_backwardLines)
    {
      if (GetRouteId(id) == GetRouteId(lineId))
        return;
    }

    if (isForward)
      itScheme->second.m_forwardLines.push_back(lineId);
    else
      itScheme->second.m_backwardLines.push_back(lineId);
  }
}

void TransitSchemeBuilder::PrepareScheme(MwmSchemeData & scheme)
{
  m2::RectD boundingRect;
  for (auto const & shape : scheme.m_shapes)
  {
    auto const stop1 = shape.first.GetStop1Id();
    auto const stop2 = shape.first.GetStop2Id();
    StopNodeParams & params1 = (scheme.m_stops.find(stop1) == scheme.m_stops.end()) ? scheme.m_transfers[stop1]
                                                                                    : scheme.m_stops[stop1];
    StopNodeParams & params2 = (scheme.m_stops.find(stop2) == scheme.m_stops.end()) ? scheme.m_transfers[stop2]
                                                                                    : scheme.m_stops[stop2];

    auto const linesCount = shape.second.m_forwardLines.size() + shape.second.m_backwardLines.size();

    auto const sz = shape.second.m_polyline.size();

    auto const dir1 = (shape.second.m_polyline[1] - shape.second.m_polyline[0]).Normalize();
    auto const dir2 = (shape.second.m_polyline[sz - 2] - shape.second.m_polyline[sz - 1]).Normalize();

    params1.m_shapesInfo[shape.first].m_linesCount = linesCount;
    params1.m_shapesInfo[shape.first].m_direction = dir1;

    params2.m_shapesInfo[shape.first].m_linesCount = linesCount;
    params2.m_shapesInfo[shape.first].m_direction = dir2;

    for (auto const & pt : shape.second.m_polyline)
      boundingRect.Add(pt);
  }
  scheme.m_pivot = boundingRect.Center();
}

void TransitSchemeBuilder::GenerateShapes(MwmSet::MwmId const & mwmId)
{
  MwmSchemeData const & scheme = m_schemes[mwmId];

  uint32_t const kBatchSize = 5000;
  dp::Batcher batcher(kBatchSize, kBatchSize);
  {
    dp::SessionGuard guard(batcher, [this, &mwmId, &scheme](dp::RenderState const & state, drape_ptr<dp::RenderBucket> && b)
    {
      TransitRenderData::Type type = TransitRenderData::Type::Lines;
      if (state.GetProgram<gpu::Program>() == gpu::Program::TransitCircle)
        type = TransitRenderData::Type::LinesCaps;
      TransitRenderData renderData(type, state, m_recacheId, mwmId, scheme.m_pivot, std::move(b));
      m_flushRenderDataFn(std::move(renderData));
    });

    for (auto const & shape : scheme.m_shapes)
    {
      auto const linesCount = shape.second.m_forwardLines.size() + shape.second.m_backwardLines.size();
      auto shapeOffset = -static_cast<float>(linesCount / 2) * 2.0f - 1.0f * static_cast<float>(linesCount % 2) + 1.0f;
      auto const shapeOffsetIncrement = 2.0f;

      std::vector<std::pair<dp::Color, routing::transit::LineId>> coloredLines;
      for (auto lineId : shape.second.m_forwardLines)
      {
        auto const colorName = df::GetTransitColorName(scheme.m_lines.at(lineId).m_color);
        auto const color = GetColorConstant(colorName);
        coloredLines.push_back(std::make_pair(color, lineId));
      }
      for (auto it = shape.second.m_backwardLines.rbegin(); it != shape.second.m_backwardLines.rend(); ++it)
      {
        auto const colorName = df::GetTransitColorName(scheme.m_lines.at(*it).m_color);
        auto const color = GetColorConstant(colorName);
        coloredLines.push_back(std::make_pair(color, *it));
      }

      for (auto const & coloredLine : coloredLines)
      {
        auto const & colorConst = coloredLine.first;
        auto const & lineId = coloredLine.second;
        auto const depth = scheme.m_lines.at(lineId).m_depth;

        GenerateLine(shape.second.m_polyline, scheme.m_pivot, colorConst, shapeOffset,
                     kTransitLineHalfWidth, depth, batcher);
        shapeOffset += shapeOffsetIncrement;
      }
    }
  }
}

void TransitSchemeBuilder::GenerateStops(MwmSet::MwmId const & mwmId, ref_ptr<dp::TextureManager> textures)
{
  MwmSchemeData const & scheme = m_schemes[mwmId];

  auto const flusher = [this, &mwmId, &scheme](dp::RenderState const & state, drape_ptr<dp::RenderBucket> && b)
  {
    TransitRenderData::Type type = TransitRenderData::Type::Stubs;
    if (state.GetProgram<gpu::Program>() == gpu::Program::TransitMarker)
      type = TransitRenderData::Type::Markers;
    else if (state.GetProgram<gpu::Program>() == gpu::Program::TextOutlined)
      type = TransitRenderData::Type::Text;

    TransitRenderData renderData(type, state, m_recacheId, mwmId, scheme.m_pivot, std::move(b));
    m_flushRenderDataFn(std::move(renderData));
  };

  uint32_t const kBatchSize = 5000;
  dp::Batcher batcher(kBatchSize, kBatchSize);
  {
    dp::SessionGuard guard(batcher, flusher);

    float const kStopScale = 2.5f;
    float const kTransferScale = 3.0f;
    std::vector<m2::PointF> const transferMarkerSizes = GetTransitMarkerSizes(kTransferScale, 1000);
    std::vector<m2::PointF> const stopMarkerSizes = GetTransitMarkerSizes(kStopScale, 1000);

    for (auto const & stop : scheme.m_stops)
    {
      GenerateStop(stop.second, scheme.m_pivot, scheme.m_lines, batcher);
      GenerateTitles(stop.second, scheme.m_pivot, stopMarkerSizes, textures, batcher);
    }
    for (auto const & transfer : scheme.m_transfers)
    {
      GenerateTransfer(transfer.second, scheme.m_pivot, batcher);
      GenerateTitles(transfer.second, scheme.m_pivot, transferMarkerSizes, textures, batcher);
    }
  }
}

void TransitSchemeBuilder::GenerateTransfer(StopNodeParams const & params, m2::PointD const & pivot, dp::Batcher & batcher)
{
  m2::PointD const pt = MapShape::ConvertToLocal(params.m_pivot, pivot, kShapeCoordScalar);

  size_t maxLinesCount = 0;
  m2::PointD dir;
  for (auto const & shapeInfo : params.m_shapesInfo)
  {
    if (shapeInfo.second.m_linesCount > maxLinesCount)
    {
      dir = shapeInfo.second.m_direction;
      maxLinesCount = shapeInfo.second.m_linesCount;
    }
  }

  float const kInnerScale = 1.0f;
  float const kOuterScale = 1.5f;
  auto const outerColor = GetColorConstant(kTransitTransferOuterColor);
  auto const innerColor = GetColorConstant(kTransitTransferInnerColor);

  float const widthLinesCount = maxLinesCount > 3 ? 1.6f : 1.0f;
  float const innerScale = maxLinesCount == 1 ? 1.4f : kInnerScale;
  float const outerScale = maxLinesCount == 1 ? 1.9f : kOuterScale;

  GenerateMarker(pt, dir, widthLinesCount, maxLinesCount, outerScale, outerScale,
                 kOuterMarkerDepth, outerColor, batcher);

  GenerateMarker(pt, dir, widthLinesCount, maxLinesCount, innerScale, innerScale,
                 kInnerMarkerDepth, innerColor, batcher);
}

void TransitSchemeBuilder::GenerateStop(StopNodeParams const & params, m2::PointD const & pivot,
                                        std::map<routing::transit::LineId, LineParams> const & lines,
                                        dp::Batcher & batcher)
{
  bool const severalRoads = params.m_stopsInfo.size() > 1;
  if (severalRoads)
  {
    GenerateTransfer(params, pivot, batcher);
    return;
  }

  float const kInnerScale = 0.8f;
  float const kOuterScale = 2.0f;

  auto const lineId = *params.m_stopsInfo.begin()->second.m_lines.begin();
  auto const colorName = df::GetTransitColorName(lines.at(lineId).m_color);
  auto const outerColor = GetColorConstant(colorName);

  auto const innerColor = GetColorConstant(kTransitStopInnerColor);

  m2::PointD const pt = MapShape::ConvertToLocal(params.m_pivot, pivot, kShapeCoordScalar);

  m2::PointD dir = params.m_shapesInfo.begin()->second.m_direction;

  GenerateMarker(pt, dir, 1.0f, 1.0f, kOuterScale, kOuterScale, kOuterMarkerDepth, outerColor, batcher);

  GenerateMarker(pt, dir, 1.0f, 1.0f, kInnerScale, kInnerScale, kInnerMarkerDepth, innerColor, batcher);
}

void TransitSchemeBuilder::GenerateTitles(StopNodeParams const & stopParams, m2::PointD const & pivot,
                                          std::vector<m2::PointF> const & markerSizes,
                                          ref_ptr<dp::TextureManager> textures, dp::Batcher & batcher)
{
  auto const vs = static_cast<float>(df::VisualParams::Instance().GetVisualScale());

  auto const titles = PlaceTitles(stopParams, kTransitMarkTextSize * vs, textures);
  if (titles.empty())
    return;

  auto const featureId = stopParams.m_stopsInfo.begin()->second.m_featureId;

  auto priority = static_cast<uint16_t>(stopParams.m_isTransfer ? Priority::TransferMin : Priority::StopMin);
  priority += static_cast<uint16_t>(stopParams.m_stopsInfo.size());

  auto minVisibleScale = stopParams.m_isTransfer ? kTransferMinZoomLevel : kStopMinZoomLevel;

  bool const isFinalStation = stopParams.m_shapesInfo.size() == 1;
  if (isFinalStation)
  {
    minVisibleScale = std::min(minVisibleScale, kFinalStationMinZoomLevel);
    priority += kFinalStationPriorityInc;
  }

  ASSERT_LESS_OR_EQUAL(priority, static_cast<uint16_t>(stopParams.m_isTransfer ? Priority::TransferMax
                                                                               : Priority::StopMax), ());

  std::vector<m2::PointF> symbolSizes;
  symbolSizes.reserve(markerSizes.size());
  for (auto const & sz : markerSizes)
    symbolSizes.push_back(sz * 1.1f);

  dp::TitleDecl titleDecl;
  titleDecl.m_primaryOptional = true;
  titleDecl.m_primaryTextFont.m_color = df::GetColorConstant(kTransitMarkText);
  titleDecl.m_primaryTextFont.m_outlineColor = df::GetColorConstant(kTransitMarkTextOutline);
  titleDecl.m_primaryTextFont.m_size = kTransitMarkTextSize * vs;
  titleDecl.m_anchor = dp::Left;

  for (auto const & title : titles)
  {
    TextViewParams textParams;
    textParams.m_featureID = featureId;
    textParams.m_tileCenter = pivot;
    textParams.m_titleDecl = titleDecl;
    textParams.m_titleDecl.m_primaryText = title.m_text;
    textParams.m_titleDecl.m_anchor = title.m_anchor;
    textParams.m_depthTestEnabled = false;
    textParams.m_depthLayer = DepthLayer::TransitSchemeLayer;
    textParams.m_specialDisplacement = SpecialDisplacement::TransitScheme;
    textParams.m_specialPriority = priority;
    textParams.m_startOverlayRank = dp::OverlayRank0;
    textParams.m_minVisibleScale = minVisibleScale;

    TextShape(stopParams.m_pivot, textParams, TileKey(), symbolSizes, title.m_offset, dp::Center, kTransitOverlayIndex)
      .Draw(&batcher, textures);
  }

  df::ColoredSymbolViewParams colorParams;
  colorParams.m_radiusInPixels = markerSizes.front().x * 0.5f;
  colorParams.m_color = dp::Color::Transparent();
  colorParams.m_featureID = featureId;
  colorParams.m_tileCenter = pivot;
  colorParams.m_depthTestEnabled = false;
  colorParams.m_depthLayer = DepthLayer::TransitSchemeLayer;
  colorParams.m_specialDisplacement = SpecialDisplacement::TransitScheme;
  colorParams.m_specialPriority = static_cast<uint16_t>(Priority::Stub);
  colorParams.m_startOverlayRank = dp::OverlayRank0;

  ColoredSymbolShape(stopParams.m_pivot, colorParams, TileKey(), kTransitStubOverlayIndex, markerSizes)
    .Draw(&batcher, textures);
}

void TransitSchemeBuilder::GenerateMarker(m2::PointD const & pt, m2::PointD widthDir,
                                          float linesCountWidth, float linesCountHeight,
                                          float scaleWidth, float scaleHeight,
                                          float depth, dp::Color const & color, dp::Batcher & batcher)
{
  using TV = TransitStaticVertex;

  scaleWidth = (scaleWidth - 1.0f) / linesCountWidth + 1.0f;
  scaleHeight = (scaleHeight - 1.0f) / linesCountHeight + 1.0f;

  widthDir.y = -widthDir.y;
  m2::PointD heightDir = widthDir.Ort();

  auto const v1 = -widthDir * scaleWidth * linesCountWidth - heightDir * scaleHeight * linesCountHeight;
  auto const v2 = widthDir * scaleWidth * linesCountWidth - heightDir * scaleHeight * linesCountHeight;
  auto const v3 = widthDir * scaleWidth * linesCountWidth + heightDir * scaleHeight * linesCountHeight;
  auto const v4 = -widthDir * scaleWidth * linesCountWidth + heightDir * scaleHeight * linesCountHeight;

  glsl::vec3 const pos(pt.x, pt.y, depth);
  auto const colorVal = glsl::vec4(color.GetRedF(), color.GetGreenF(), color.GetBlueF(), 1.0f /* alpha */ );

  TGeometryBuffer geometry;
  geometry.reserve(6);
  geometry.emplace_back(pos, TV::TNormal(v1.x, v1.y, -linesCountWidth, -linesCountHeight), colorVal);
  geometry.emplace_back(pos, TV::TNormal(v2.x, v2.y, linesCountWidth, -linesCountHeight), colorVal);
  geometry.emplace_back(pos, TV::TNormal(v3.x, v3.y, linesCountWidth, linesCountHeight), colorVal);
  geometry.emplace_back(pos, TV::TNormal(v1.x, v1.y, -linesCountWidth, -linesCountHeight), colorVal);
  geometry.emplace_back(pos, TV::TNormal(v3.x, v3.y, linesCountWidth, linesCountHeight), colorVal);
  geometry.emplace_back(pos, TV::TNormal(v4.x, v4.y, -linesCountWidth, linesCountHeight), colorVal);

  dp::AttributeProvider provider(1 /* stream count */, static_cast<uint32_t>(geometry.size()));
  provider.InitStream(0 /* stream index */, GetTransitStaticBindingInfo(), make_ref(geometry.data()));
  auto state = CreateRenderState(gpu::Program::TransitMarker, DepthLayer::TransitSchemeLayer);
  batcher.InsertTriangleList(state, make_ref(&provider));
}

void TransitSchemeBuilder::GenerateLine(std::vector<m2::PointD> const & path, m2::PointD const & pivot,
                                        dp::Color const & colorConst, float lineOffset, float halfWidth,
                                        float depth, dp::Batcher & batcher)
{
  using TV = TransitStaticVertex;

  TGeometryBuffer geometry;
  auto const color = glsl::vec4(colorConst.GetRedF(), colorConst.GetGreenF(), colorConst.GetBlueF(), 1.0f /* alpha */);
  size_t const kAverageSize = path.size() * 6;
  size_t const kAverageCapSize = 12;
  geometry.reserve(kAverageSize + kAverageCapSize * 2);

  std::vector<SchemeSegment> segments;
  segments.reserve(path.size() - 1);

  for (size_t i = 1; i < path.size(); ++i)
  {
    if (path[i].EqualDxDy(path[i - 1], 1.0e-5))
      continue;

    SchemeSegment segment;
    segment.m_p1 = glsl::ToVec2(MapShape::ConvertToLocal(path[i - 1], pivot, kShapeCoordScalar));
    segment.m_p2 = glsl::ToVec2(MapShape::ConvertToLocal(path[i], pivot, kShapeCoordScalar));
    CalculateTangentAndNormals(segment.m_p1, segment.m_p2, segment.m_tangent,
                               segment.m_leftNormal, segment.m_rightNormal);

    auto const startPivot = glsl::vec3(segment.m_p1, depth);
    auto const endPivot = glsl::vec3(segment.m_p2, depth);
    auto const offset = lineOffset * segment.m_rightNormal;

    geometry.emplace_back(startPivot, TV::TNormal(segment.m_rightNormal * halfWidth - offset, -halfWidth, 0.0), color);
    geometry.emplace_back(startPivot, TV::TNormal(segment.m_leftNormal * halfWidth - offset, halfWidth, 0.0), color);
    geometry.emplace_back(endPivot, TV::TNormal(segment.m_rightNormal * halfWidth - offset, -halfWidth, 0.0), color);
    geometry.emplace_back(endPivot, TV::TNormal(segment.m_rightNormal * halfWidth - offset, -halfWidth, 0.0), color);
    geometry.emplace_back(startPivot, TV::TNormal(segment.m_leftNormal * halfWidth - offset, halfWidth, 0.0), color);
    geometry.emplace_back(endPivot, TV::TNormal(segment.m_leftNormal * halfWidth - offset, halfWidth, 0.0), color);

    segments.emplace_back(std::move(segment));
  }

  dp::AttributeProvider provider(1 /* stream count */, static_cast<uint32_t>(geometry.size()));
  provider.InitStream(0 /* stream index */, GetTransitStaticBindingInfo(), make_ref(geometry.data()));
  auto state = CreateRenderState(gpu::Program::Transit, DepthLayer::TransitSchemeLayer);
  batcher.InsertTriangleList(state, make_ref(&provider));

  GenerateLineCaps(segments, color, lineOffset, halfWidth, depth, batcher);
}
}  // namespace df
