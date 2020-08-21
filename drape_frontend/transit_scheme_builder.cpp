#include "transit_scheme_builder.hpp"

#include "drape_frontend/batcher_bucket.hpp"
#include "drape_frontend/color_constants.hpp"
#include "drape_frontend/colored_symbol_shape.hpp"
#include "drape_frontend/line_shape_helper.hpp"
#include "drape_frontend/map_shape.hpp"
#include "drape_frontend/render_state_extension.hpp"
#include "drape_frontend/shape_view_params.hpp"
#include "drape_frontend/text_layout.hpp"
#include "drape_frontend/text_shape.hpp"

#include "shaders/programs.hpp"

#include "drape/batcher.hpp"
#include "drape/glsl_func.hpp"
#include "drape/glsl_types.hpp"
#include "drape/render_bucket.hpp"
#include "drape/utils/vertex_decl.hpp"

#include "geometry/mercator.hpp"

#include "base/assert.hpp"
#include "base/string_utils.hpp"

#include <algorithm>

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

float constexpr kTransitMarkTextSize = 11.0f;

static std::unordered_set<std::string> const kSubwayTypes{"subway", "train", "light_rail",
                                                          "monorail"};

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

struct TitleInfo
{
  TitleInfo() = default;
  explicit TitleInfo(std::string const & text) : m_text(text) {}

  std::string m_text;
  size_t m_rowsCount = 0;
  m2::PointF m_pixelSize;
  m2::PointF m_offset;
  dp::Anchor m_anchor = dp::Left;
};

std::vector<TitleInfo> PlaceTitles(StopNodeParamsSubway const & stopParams, float textSize,
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
                                    false /* isSdf */, textures, dp::Left, false /* forceNoWrap */);
      name.m_pixelSize = layout.GetPixelSize() + m2::PointF(4.0f * vs, 4.0f * vs);
      name.m_rowsCount = layout.GetRowsCount();
      summaryRowsCount += layout.GetRowsCount();
    }

    auto const rightRowsCount =
        summaryRowsCount > 3 ? (summaryRowsCount + 1) / 2 : summaryRowsCount;
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

std::vector<TitleInfo> PlaceTitles(StopNodeParamsPT const & stopParams, float textSize,
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
                                    false /* isSdf */, textures, dp::Left, false /* forceNoWrap */);
      name.m_pixelSize = layout.GetPixelSize() + m2::PointF(4.0f * vs, 4.0f * vs);
      name.m_rowsCount = layout.GetRowsCount();
      summaryRowsCount += layout.GetRowsCount();
    }

    auto const rightRowsCount =
        summaryRowsCount > 3 ? (summaryRowsCount + 1) / 2 : summaryRowsCount;
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

void GenerateLineCaps(ref_ptr<dp::GraphicsContext> context,
                      std::vector<SchemeSegment> const & segments, glsl::vec4 const & color,
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
  provider.InitStream(0 /* stream index */, GetTransitStaticBindingInfo(),
                      make_ref(geometry.data()));
  auto state = CreateRenderState(gpu::Program::TransitCircle, DepthLayer::TransitSchemeLayer);
  batcher.InsertTriangleList(context, state, make_ref(&provider));
}

uint32_t GetRouteId(routing::transit::LineId lineId) { return static_cast<uint32_t>(lineId >> 4); }

void FillStopParamsSubway(TransitDisplayInfo const & transitDisplayInfo,
                          MwmSet::MwmId const & mwmId, routing::transit::Stop const & stop,
                          StopNodeParamsSubway & stopParams)
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

void FillStopParamsPT(TransitDisplayInfo const & transitDisplayInfo, MwmSet::MwmId const & mwmId,
                      ::transit::experimental::Stop const & stop, ::transit::IdSet const & lineIds,
                      StopNodeParamsPT & stopParams)
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

  for (auto lineId : lineIds)
  {
    ::transit::TransitId const routeId = transitDisplayInfo.m_linesPT.at(lineId).GetRouteId();
    StopInfo & info = stopParams.m_stopsInfo[routeId];
    info.m_featureId = featureId;
    info.m_name = title;
    info.m_lines.insert(lineId);
  }
}

bool FindLongerPath(routing::transit::StopId stop1Id, routing::transit::StopId stop2Id,
                    std::vector<routing::transit::StopId> const & sameStops, size_t & stop1Ind,
                    size_t & stop2Ind)
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

std::string TransitSchemeBuilder::MwmSchemeData::GetLineColor(::transit::TransitId lineId) const
{
  if (m_transitVersion == ::transit::TransitVersion::OnlySubway)
  {
    return m_linesSubway.at(lineId).m_color;
  }
  else if (m_transitVersion == ::transit::TransitVersion::AllPublicTransport)
  {
    return m_linesPT.at(lineId).m_color;
  }
  UNREACHABLE();
}

float TransitSchemeBuilder::MwmSchemeData::GetLineDepth(::transit::TransitId lineId) const
{
  if (m_transitVersion == ::transit::TransitVersion::OnlySubway)
  {
    return m_linesSubway.at(lineId).m_depth;
  }
  else if (m_transitVersion == ::transit::TransitVersion::AllPublicTransport)
  {
    return m_linesPT.at(lineId).m_depth;
  }
  UNREACHABLE();
}

void TransitSchemeBuilder::UpdateSchemes(ref_ptr<dp::GraphicsContext> context,
                                         TransitDisplayInfos const & transitDisplayInfos,
                                         ref_ptr<dp::TextureManager> textures)
{
  for (auto const & [mwmId, transitDisplayInfoPtr] : transitDisplayInfos)
  {
    if (!transitDisplayInfoPtr)
      continue;

    auto const & transitDisplayInfo = *transitDisplayInfoPtr.get();

    MwmSchemeData & scheme = m_schemes[mwmId];
    scheme.m_transitVersion = transitDisplayInfo.m_transitVersion;

    if (scheme.m_transitVersion == ::transit::TransitVersion::OnlySubway)
    {
      CollectStopsSubway(transitDisplayInfo, mwmId, scheme);
      CollectLinesSubway(transitDisplayInfo, scheme);
      CollectShapesSubway(transitDisplayInfo, scheme);

      PrepareSchemeSubway(scheme);
      BuildScheme(context, mwmId, textures);
    }
    else if (scheme.m_transitVersion == ::transit::TransitVersion::AllPublicTransport)
    {
      LinesDataPT const & linesData = CollectLinesPT(transitDisplayInfo, scheme);

      CollectStopsPT(transitDisplayInfo, linesData, mwmId, scheme);
      CollectShapesPT(transitDisplayInfo, scheme);

      PrepareSchemePT(transitDisplayInfo, scheme);
      BuildScheme(context, mwmId, textures);
    }
    else
    {
      LOG(LERROR, (scheme.m_transitVersion));
      UNREACHABLE();
    }
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

void TransitSchemeBuilder::RebuildSchemes(ref_ptr<dp::GraphicsContext> context,
                                          ref_ptr<dp::TextureManager> textures)
{
  for (auto const & mwmScheme : m_schemes)
    BuildScheme(context, mwmScheme.first, textures);
}

void TransitSchemeBuilder::BuildScheme(ref_ptr<dp::GraphicsContext> context,
                                       MwmSet::MwmId const & mwmId,
                                       ref_ptr<dp::TextureManager> textures)
{
  if (m_schemes.find(mwmId) == m_schemes.end())
    return;

  ++m_recacheId;
  GenerateShapes(context, mwmId);
  GenerateStops(context, mwmId, textures);
}

void TransitSchemeBuilder::GenerateLinesSubway(MwmSchemeData const & scheme, dp::Batcher & batcher,
                                               ref_ptr<dp::GraphicsContext> context)
{
  for (auto const & shape : scheme.m_shapesSubway)
  {
    size_t const linesCount =
        shape.second.m_forwardLines.size() + shape.second.m_backwardLines.size();
    float shapeOffset = -static_cast<float>(linesCount / 2) * 2.0f -
                        1.0f * static_cast<float>(linesCount % 2) + 1.0f;
    size_t constexpr shapeOffsetIncrement = 2.0f;

    std::vector<std::pair<dp::Color, routing::transit::LineId>> coloredLines;

    for (auto lineId : shape.second.m_forwardLines)
    {
      auto const colorName = df::GetTransitColorName(scheme.GetLineColor(lineId));
      auto const color = GetColorConstant(colorName);
      coloredLines.emplace_back(color, lineId);
    }

    for (auto it = shape.second.m_backwardLines.rbegin(); it != shape.second.m_backwardLines.rend();
         ++it)
    {
      auto const colorName = df::GetTransitColorName(scheme.GetLineColor(*it));
      auto const color = GetColorConstant(colorName);
      coloredLines.emplace_back(color, *it);
    }

    for (auto const & coloredLine : coloredLines)
    {
      auto const & colorConst = coloredLine.first;
      auto const & lineId = coloredLine.second;
      auto const depth = scheme.GetLineDepth(lineId);

      GenerateLine(context, shape.second.m_polyline, scheme.m_pivot, colorConst, shapeOffset,
                   kTransitLineHalfWidth, depth, batcher);

      shapeOffset += shapeOffsetIncrement;
    }
  }
}

void TransitSchemeBuilder::GenerateLinesPT(MwmSchemeData const & scheme, dp::Batcher & batcher,
                                           ref_ptr<dp::GraphicsContext> context)
{
  for (auto const & [routeId, data] : scheme.m_shapeHierarchyPT)
  {
    dp::Color const color = GetColorConstant(df::GetTransitColorName(data.m_color));

    for (auto const & routeData : data.m_routeShapes)
    {
      auto const start = ms::LatLon(52.54896573047641084, 13.388990046182382088);
      auto const end = ms::LatLon(52.549313637305999691, 13.414450707060183277);

      if (base::AlmostEqualAbs(mercator::ToLatLon(routeData.m_polyline.front()), start, 1e-5) &&
          base::AlmostEqualAbs(mercator::ToLatLon(routeData.m_polyline.back()), end, 1e-5))
        LOG(LINFO, ("TTT route", routeId, "not inverse", "order", routeData.m_order));

      if (base::AlmostEqualAbs(mercator::ToLatLon(routeData.m_polyline.back()), start, 1e-5) &&
          base::AlmostEqualAbs(mercator::ToLatLon(routeData.m_polyline.front()), end, 1e-5))
        LOG(LINFO, ("TTT route", routeId, "inverse", "order", routeData.m_order));

      float const offset = static_cast<float>(routeData.m_order);
      GenerateLine(context, routeData.m_polyline, scheme.m_pivot, color, offset,
                   kTransitLineHalfWidth, kBaseLineDepth, batcher);
    }
  }
}

void TransitSchemeBuilder::CollectStopsSubway(TransitDisplayInfo const & transitDisplayInfo,
                                              MwmSet::MwmId const & mwmId, MwmSchemeData & scheme)
{
  CHECK_EQUAL(transitDisplayInfo.m_transitVersion, ::transit::TransitVersion::OnlySubway, ());

  for (auto const & stopInfo : transitDisplayInfo.m_stopsSubway)
  {
    routing::transit::Stop const & stop = stopInfo.second;
    if (stop.GetTransferId() != routing::transit::kInvalidTransferId)
      continue;
    auto & stopNode = scheme.m_stopsSubway[stop.GetId()];
    FillStopParamsSubway(transitDisplayInfo, mwmId, stop, stopNode);
  }

  for (auto const & stopInfo : transitDisplayInfo.m_transfersSubway)
  {
    routing::transit::Transfer const & transfer = stopInfo.second;
    auto & stopNode = scheme.m_transfersSubway[transfer.GetId()];

    for (auto stopId : transfer.GetStopIds())
    {
      if (transitDisplayInfo.m_stopsSubway.find(stopId) == transitDisplayInfo.m_stopsSubway.end())
      {
        LOG(LWARNING, ("Invalid stop", stopId, "in transfer", transfer.GetId()));
        continue;
      }
      routing::transit::Stop const & stop = transitDisplayInfo.m_stopsSubway.at(stopId);
      FillStopParamsSubway(transitDisplayInfo, mwmId, stop, stopNode);
    }
    stopNode.m_isTransfer = true;
    stopNode.m_pivot = transfer.GetPoint();
  }
}

void TransitSchemeBuilder::CollectStopsPT(TransitDisplayInfo const & transitDisplayInfo,
                                          LinesDataPT const & linesData,
                                          MwmSet::MwmId const & mwmId, MwmSchemeData & scheme)
{
  CHECK_EQUAL(transitDisplayInfo.m_transitVersion, ::transit::TransitVersion::AllPublicTransport,
              ());

  for (auto const & [stopId, lineIds] : linesData.m_stopToLineIds)
  {
    ::transit::experimental::Stop const & stop = transitDisplayInfo.m_stopsPT.at(stopId);

    if (!stop.GetTransferIds().empty())
      continue;

    FillStopParamsPT(transitDisplayInfo, mwmId, stop, lineIds, scheme.m_stopsPT[stopId]);

    scheme.m_stopsPT[stopId].m_isTerminalStop =
        (linesData.m_terminalStops.find(stopId) != linesData.m_terminalStops.end());
  }

  for (auto const & transferInfo : transitDisplayInfo.m_transfersPT)
  {
    ::transit::experimental::Transfer const & transfer = transferInfo.second;
    auto & transferNode = scheme.m_transfersPT[transfer.GetId()];

    for (auto stopId : transfer.GetStopIds())
    {
      auto itIds = linesData.m_stopToLineIds.find(stopId);
      if (itIds == linesData.m_stopToLineIds.end())
        continue;

      auto it = transitDisplayInfo.m_stopsPT.find(stopId);
      if (it == transitDisplayInfo.m_stopsPT.end())
        continue;

      ::transit::experimental::Stop const & stop = it->second;
      FillStopParamsPT(transitDisplayInfo, mwmId, stop, itIds->second, transferNode);
      transferNode.m_stopsInfo.emplace(stopId, StopInfo());
    }

    transferNode.m_isTransfer = true;
    transferNode.m_pivot = transfer.GetPoint();
  }
}

void TransitSchemeBuilder::CollectLinesSubway(TransitDisplayInfo const & transitDisplayInfo,
                                              MwmSchemeData & scheme)
{
  CHECK_EQUAL(transitDisplayInfo.m_transitVersion, ::transit::TransitVersion::OnlySubway, ());

  std::multimap<size_t, routing::transit::LineId> linesLengths;
  for (auto const & line : transitDisplayInfo.m_linesSubway)
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

    scheme.m_linesSubway[lineId] =
        LineParams(transitDisplayInfo.m_linesSubway.at(lineId).GetColor(), depth);

    depth += kDepthPerLine;
  }
}

LinesDataPT TransitSchemeBuilder::CollectLinesPT(TransitDisplayInfo const & transitDisplayInfo,
                                                 MwmSchemeData & scheme)
{
  CHECK_EQUAL(transitDisplayInfo.m_transitVersion, ::transit::TransitVersion::AllPublicTransport,
              ());

  LinesDataPT linesData;

  for (auto const & [lineId, line] : transitDisplayInfo.m_linesPT)
  {
    auto const & lineType = transitDisplayInfo.m_routesPT.at(line.GetRouteId()).GetType();

    // We skip types that are not mentioned for displaying on the layer - buses, ferries, etc.
    if (kSubwayTypes.find(lineType) == kSubwayTypes.end())
      continue;

    for (auto stopId : line.GetStopIds())
      linesData.m_stopToLineIds[stopId].insert(lineId);

    linesData.m_terminalStops.insert(line.GetStopIds().front());
    linesData.m_terminalStops.insert(line.GetStopIds().back());

    scheme.m_linesPT[lineId] =
        LineParams(transitDisplayInfo.m_routesPT.at(line.GetRouteId()).GetColor(), kBaseLineDepth);
    scheme.m_linesPT[lineId].m_stopIds = line.GetStopIds();
  }

  return linesData;
}

void TransitSchemeBuilder::CollectShapesSubway(TransitDisplayInfo const & transitDisplayInfo,
                                               MwmSchemeData & scheme)
{
  CHECK_EQUAL(transitDisplayInfo.m_transitVersion, ::transit::TransitVersion::OnlySubway, ());

  std::map<uint32_t, std::vector<routing::transit::LineId>> roads;

  for (auto const & line : transitDisplayInfo.m_linesSubway)
  {
    auto const lineId = line.second.GetId();
    auto const roadId = GetRouteId(lineId);
    roads[roadId].push_back(lineId);
  }

  for (auto const & line : transitDisplayInfo.m_linesSubway)
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

void TransitSchemeBuilder::CollectShapesPT(TransitDisplayInfo const & transitDisplayInfo,
                                           MwmSchemeData & scheme)
{
  CHECK_EQUAL(transitDisplayInfo.m_transitVersion, ::transit::TransitVersion::AllPublicTransport,
              ());

  using LineAndShapeLink = std::pair<::transit::TransitId, ::transit::ShapeLink>;

  std::map<::transit::TransitId, std::vector<LineAndShapeLink>> routesToLines;

  for (auto const & [lineId, lineParams] : scheme.m_linesPT)
  {
    auto const & line = transitDisplayInfo.m_linesPT.at(lineId);
    auto const & shapeLink = line.GetShapeLink();
    ::transit::TransitId routeId = line.GetRouteId();
    auto const & color = transitDisplayInfo.m_routesPT.at(routeId).GetColor();

    scheme.m_shapeHierarchyPT[routeId].m_color = color;
    routesToLines[routeId].emplace_back(lineId, shapeLink);
  }

  for (auto const & [routeId, linesAndLinks] : routesToLines)
  {
    auto & route = scheme.m_shapeHierarchyPT[routeId];

    for (auto const & [lineId, shapeLink] : linesAndLinks)
    {
      auto const & shape = transitDisplayInfo.m_shapesPT.at(shapeLink.m_shapeId).GetPolyline();
      auto itMetadata = transitDisplayInfo.m_linesMetadataPT.find(lineId);
      if (itMetadata == transitDisplayInfo.m_linesMetadataPT.end())
        continue;

      for (auto const & part : itMetadata->second.GetLineSegmentsOrder())
      {
        RouteSegment rs;
        rs.m_polyline =
            ::transit::GetPolylinePart(shape, part.m_segment.m_startIdx, part.m_segment.m_endIdx);
        rs.m_order = part.m_order;
        route.m_routeShapes.push_back(rs);
      }
    }
  }
}

void TransitSchemeBuilder::FindShapes(routing::transit::StopId stop1Id,
                                      routing::transit::StopId stop2Id,
                                      routing::transit::LineId lineId,
                                      std::vector<routing::transit::LineId> const & sameLines,
                                      TransitDisplayInfo const & transitDisplayInfo,
                                      MwmSchemeData & scheme)
{
  bool shapeAdded = false;

  for (auto const & sameLineId : sameLines)
  {
    if (sameLineId == lineId)
      continue;

    auto const & sameLine = transitDisplayInfo.m_linesSubway.at(sameLineId);
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
  auto const stop1It = transitDisplayInfo.m_stopsSubway.find(stop1Id);
  ASSERT(stop1It != transitDisplayInfo.m_stopsSubway.end(), (stop1Id));

  auto const stop2It = transitDisplayInfo.m_stopsSubway.find(stop2Id);
  ASSERT(stop2It != transitDisplayInfo.m_stopsSubway.end(), (stop2Id));

  auto const transfer1Id = stop1It->second.GetTransferId();
  auto const transfer2Id = stop2It->second.GetTransferId();

  auto shapeId = routing::transit::ShapeId(transfer1Id != routing::transit::kInvalidTransferId ? transfer1Id
                                                                                               : stop1Id,
                                           transfer2Id != routing::transit::kInvalidTransferId ? transfer2Id
                                                                                               : stop2Id);
  auto it = transitDisplayInfo.m_shapesSubway.find(shapeId);
  bool isForward = true;
  if (it == transitDisplayInfo.m_shapesSubway.end())
  {
    isForward = false;
    shapeId = routing::transit::ShapeId(shapeId.GetStop2Id(), shapeId.GetStop1Id());
    it = transitDisplayInfo.m_shapesSubway.find(shapeId);
  }

  if (it == transitDisplayInfo.m_shapesSubway.end())
    return;

  auto const itScheme = scheme.m_shapesSubway.find(shapeId);
  if (itScheme == scheme.m_shapesSubway.end())
  {
    auto const & polyline = transitDisplayInfo.m_shapesSubway.at(it->first).GetPolyline();
    if (isForward)
      scheme.m_shapesSubway[shapeId].m_forwardLines.push_back(lineId);
    else
      scheme.m_shapesSubway[shapeId].m_backwardLines.push_back(lineId);
    scheme.m_shapesSubway[shapeId].m_polyline = polyline;
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

void TransitSchemeBuilder::PrepareSchemeSubway(MwmSchemeData & scheme)
{
  m2::RectD boundingRect;

  for (auto const & shape : scheme.m_shapesSubway)
  {
    auto const stop1 = shape.first.GetStop1Id();
    auto const stop2 = shape.first.GetStop2Id();
    StopNodeParamsSubway & params1 =
        (scheme.m_stopsSubway.find(stop1) == scheme.m_stopsSubway.end())
            ? scheme.m_transfersSubway[stop1]
            : scheme.m_stopsSubway[stop1];
    StopNodeParamsSubway & params2 =
        (scheme.m_stopsSubway.find(stop2) == scheme.m_stopsSubway.end())
            ? scheme.m_transfersSubway[stop2]
            : scheme.m_stopsSubway[stop2];

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

void TransitSchemeBuilder::PrepareSchemePT(TransitDisplayInfo const & transitDisplayInfo,
                                           MwmSchemeData & scheme)
{
  m2::RectD boundingRect;
  std::unordered_set<::transit::TransitId> handledStopIds;

  for (auto const & [lineId, lineData] : scheme.m_linesPT)
  {
    for (size_t i = 0; i < lineData.m_stopIds.size() - 1; ++i)
    {
      ::transit::TransitId stop1Id = lineData.m_stopIds[i];
      ::transit::TransitId stop2Id = lineData.m_stopIds[i + 1];

      if (!handledStopIds.insert(stop1Id).second && !handledStopIds.insert(stop2Id).second)
        continue;

      StopNodeParamsPT & params1 = (scheme.m_stopsPT.find(stop1Id) == scheme.m_stopsPT.end())
                                       ? scheme.m_transfersPT[stop1Id]
                                       : scheme.m_stopsPT[stop1Id];

      StopNodeParamsPT & params2 = (scheme.m_stopsPT.find(stop2Id) == scheme.m_stopsPT.end())
                                       ? scheme.m_transfersPT[stop2Id]
                                       : scheme.m_stopsPT[stop2Id];

      size_t constexpr minLinesCount = 1;

      params1.m_shapeInfo.m_linesCount = std::max(minLinesCount, params1.m_stopsInfo.size());
      params2.m_shapeInfo.m_linesCount = std::max(minLinesCount, params2.m_stopsInfo.size());

      auto it = transitDisplayInfo.m_edgesPT.find(::transit::EdgeId(stop1Id, stop2Id, lineId));

      if (it == transitDisplayInfo.m_edgesPT.end())
      {
        params1.m_shapeInfo.m_direction = (params2.m_pivot - params1.m_pivot).Normalize();
        params2.m_shapeInfo.m_direction = (params1.m_pivot - params2.m_pivot).Normalize();
      }
      else
      {
        ::transit::ShapeLink const & shapeLink = it->second.m_shapeLink;
        auto const & polyline = transitDisplayInfo.m_shapesPT.at(shapeLink.m_shapeId).GetPolyline();

        size_t startIndex = std::min(shapeLink.m_startIndex, shapeLink.m_endIndex);
        size_t endIndex = std::max(shapeLink.m_startIndex, shapeLink.m_endIndex);

        params1.m_shapeInfo.m_direction =
            (polyline[startIndex + 1] - polyline[startIndex]).Normalize();
        params2.m_shapeInfo.m_direction = (polyline[endIndex] - polyline[endIndex - 1]).Normalize();

        if (shapeLink.m_startIndex > shapeLink.m_endIndex)
          std::swap(params1.m_shapeInfo.m_direction, params2.m_shapeInfo.m_direction);

        for (size_t j = shapeLink.m_startIndex; j <= shapeLink.m_endIndex; ++j)
          boundingRect.Add(polyline[j]);
      }
    }
  }

  for (auto & [transferId, transferData] : scheme.m_transfersPT)
  {
    if (!transferData.m_isTransfer)
      continue;

    auto const & transfer = transitDisplayInfo.m_transfersPT.at(transferId);
    std::vector<m2::PointD> pivots;

    for (::transit::TransitId stopId : transfer.GetStopIds())
    {
      auto const & stop = transitDisplayInfo.m_stopsPT.at(stopId);
      pivots.push_back(stop.GetPoint());

      if (pivots.size() == 2)
        break;
    }

    CHECK_EQUAL(pivots.size(), 2, ());

    transferData.m_shapeInfo.m_direction = (pivots[1] - pivots[0]).Normalize();
    transferData.m_shapeInfo.m_linesCount = transfer.GetStopIds().size();
  }

  scheme.m_pivot = boundingRect.Center();
}

void TransitSchemeBuilder::GenerateShapes(ref_ptr<dp::GraphicsContext> context, MwmSet::MwmId const & mwmId)
{
  MwmSchemeData const & scheme = m_schemes[mwmId];

  uint32_t constexpr kBatchSize = 65000;
  dp::Batcher batcher(kBatchSize, kBatchSize);
  batcher.SetBatcherHash(static_cast<uint64_t>(BatcherBucket::Transit));
  {
    dp::SessionGuard guard(context, batcher, [this, &mwmId, &scheme](dp::RenderState const & state,
                                                                     drape_ptr<dp::RenderBucket> && b)
    {
      TransitRenderData::Type type = TransitRenderData::Type::Lines;
      if (state.GetProgram<gpu::Program>() == gpu::Program::TransitCircle)
        type = TransitRenderData::Type::LinesCaps;
      TransitRenderData renderData(type, state, m_recacheId, mwmId, scheme.m_pivot, std::move(b));
      m_flushRenderDataFn(std::move(renderData));
    });

    if (scheme.m_transitVersion == ::transit::TransitVersion::OnlySubway)
      GenerateLinesSubway(scheme, batcher, context);
    else if (scheme.m_transitVersion == ::transit::TransitVersion::AllPublicTransport)
      GenerateLinesPT(scheme, batcher, context);
    else
    {
      LOG(LERROR, (scheme.m_transitVersion));
      UNREACHABLE();
    }
  }
}

void TransitSchemeBuilder::GenerateStops(ref_ptr<dp::GraphicsContext> context, MwmSet::MwmId const & mwmId,
                                         ref_ptr<dp::TextureManager> textures)
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

  uint32_t constexpr kBatchSize = 5000;
  dp::Batcher batcher(kBatchSize, kBatchSize);

  batcher.SetBatcherHash(static_cast<uint64_t>(BatcherBucket::Transit));
  if (scheme.m_transitVersion == ::transit::TransitVersion::OnlySubway)
  {
    GenerateLocationsWithTitles(context, textures, batcher, flusher, scheme, scheme.m_stopsSubway,
                                scheme.m_transfersSubway, scheme.m_linesSubway);
  }
  else if (scheme.m_transitVersion == ::transit::TransitVersion::AllPublicTransport)
  {
    GenerateLocationsWithTitles(context, textures, batcher, flusher, scheme, scheme.m_stopsPT,
                                scheme.m_transfersPT, scheme.m_linesPT);
  }
  else
  {
    LOG(LERROR, (scheme.m_transitVersion));
    UNREACHABLE();
  }
}

void TransitSchemeBuilder::GenerateMarker(ref_ptr<dp::GraphicsContext> context,
                                          m2::PointD const & pt, m2::PointD widthDir,
                                          float linesCountWidth, float linesCountHeight,
                                          float scaleWidth, float scaleHeight, float depth,
                                          dp::Color const & color, dp::Batcher & batcher)
{
  using TV = TransitStaticVertex;

  scaleWidth = (scaleWidth - 1.0f) / linesCountWidth + 1.0f;
  scaleHeight = (scaleHeight - 1.0f) / linesCountHeight + 1.0f;

  widthDir.y = -widthDir.y;
  m2::PointD heightDir = widthDir.Ort();

  auto const v1 =
      -widthDir * scaleWidth * linesCountWidth - heightDir * scaleHeight * linesCountHeight;
  auto const v2 =
      widthDir * scaleWidth * linesCountWidth - heightDir * scaleHeight * linesCountHeight;
  auto const v3 =
      widthDir * scaleWidth * linesCountWidth + heightDir * scaleHeight * linesCountHeight;
  auto const v4 =
      -widthDir * scaleWidth * linesCountWidth + heightDir * scaleHeight * linesCountHeight;

  glsl::vec3 const pos(pt.x, pt.y, depth);
  auto const colorVal =
      glsl::vec4(color.GetRedF(), color.GetGreenF(), color.GetBlueF(), 1.0f /* alpha */);

  TGeometryBuffer geometry;
  geometry.reserve(6);
  geometry.emplace_back(pos, TV::TNormal(v1.x, v1.y, -linesCountWidth, -linesCountHeight),
                        colorVal);
  geometry.emplace_back(pos, TV::TNormal(v2.x, v2.y, linesCountWidth, -linesCountHeight), colorVal);
  geometry.emplace_back(pos, TV::TNormal(v3.x, v3.y, linesCountWidth, linesCountHeight), colorVal);
  geometry.emplace_back(pos, TV::TNormal(v1.x, v1.y, -linesCountWidth, -linesCountHeight),
                        colorVal);
  geometry.emplace_back(pos, TV::TNormal(v3.x, v3.y, linesCountWidth, linesCountHeight), colorVal);
  geometry.emplace_back(pos, TV::TNormal(v4.x, v4.y, -linesCountWidth, linesCountHeight), colorVal);

  dp::AttributeProvider provider(1 /* stream count */, static_cast<uint32_t>(geometry.size()));
  provider.InitStream(0 /* stream index */, GetTransitStaticBindingInfo(),
                      make_ref(geometry.data()));
  auto state = CreateRenderState(gpu::Program::TransitMarker, DepthLayer::TransitSchemeLayer);
  batcher.InsertTriangleList(context, state, make_ref(&provider));
}

void TransitSchemeBuilder::GenerateLine(ref_ptr<dp::GraphicsContext> context,
                                        std::vector<m2::PointD> const & path,
                                        m2::PointD const & pivot, dp::Color const & colorConst,
                                        float lineOffset, float halfWidth, float depth,
                                        dp::Batcher & batcher)
{
  using TV = TransitStaticVertex;

  TGeometryBuffer geometry;
  auto const color = glsl::vec4(colorConst.GetRedF(), colorConst.GetGreenF(), colorConst.GetBlueF(),
                                1.0f /* alpha */);
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
    CalculateTangentAndNormals(segment.m_p1, segment.m_p2, segment.m_tangent, segment.m_leftNormal,
                               segment.m_rightNormal);

    auto const startPivot = glsl::vec3(segment.m_p1, depth);
    auto const endPivot = glsl::vec3(segment.m_p2, depth);
    auto const offset = lineOffset * segment.m_rightNormal;

    geometry.emplace_back(startPivot,
                          TV::TNormal(segment.m_rightNormal * halfWidth - offset, -halfWidth, 0.0),
                          color);
    geometry.emplace_back(
        startPivot, TV::TNormal(segment.m_leftNormal * halfWidth - offset, halfWidth, 0.0), color);
    geometry.emplace_back(
        endPivot, TV::TNormal(segment.m_rightNormal * halfWidth - offset, -halfWidth, 0.0), color);
    geometry.emplace_back(
        endPivot, TV::TNormal(segment.m_rightNormal * halfWidth - offset, -halfWidth, 0.0), color);
    geometry.emplace_back(
        startPivot, TV::TNormal(segment.m_leftNormal * halfWidth - offset, halfWidth, 0.0), color);
    geometry.emplace_back(
        endPivot, TV::TNormal(segment.m_leftNormal * halfWidth - offset, halfWidth, 0.0), color);

    segments.emplace_back(std::move(segment));
  }

  dp::AttributeProvider provider(1 /* stream count */, static_cast<uint32_t>(geometry.size()));
  provider.InitStream(0 /* stream index */, GetTransitStaticBindingInfo(),
                      make_ref(geometry.data()));
  auto state = CreateRenderState(gpu::Program::Transit, DepthLayer::TransitSchemeLayer);
  batcher.InsertTriangleList(context, state, make_ref(&provider));

  GenerateLineCaps(context, segments, color, lineOffset, halfWidth, depth, batcher);
}

void TransitSchemeBuilder::GenerateStop(
    ref_ptr<dp::GraphicsContext> context, StopNodeParamsSubway const & stopParams,
    m2::PointD const & pivot, std::map<routing::transit::LineId, LineParams> const & lines,
    dp::Batcher & batcher)
{
  bool const severalRoads = stopParams.m_stopsInfo.size() > 1;

  if (severalRoads)
  {
    GenerateTransfer(context, stopParams, pivot, batcher);
    return;
  }

  float const kInnerScale = 0.8f;
  float const kOuterScale = 2.0f;

  auto const lineId = *stopParams.m_stopsInfo.begin()->second.m_lines.begin();
  auto const colorName = df::GetTransitColorName(lines.at(lineId).m_color);
  auto const outerColor = GetColorConstant(colorName);

  auto const innerColor = GetColorConstant(kTransitStopInnerColor);

  m2::PointD const pt = MapShape::ConvertToLocal(stopParams.m_pivot, pivot, kShapeCoordScalar);

  m2::PointD dir = stopParams.m_shapesInfo.begin()->second.m_direction;

  GenerateMarker(context, pt, dir, 1.0f, 1.0f, kOuterScale, kOuterScale, kOuterMarkerDepth,
                 outerColor, batcher);

  GenerateMarker(context, pt, dir, 1.0f, 1.0f, kInnerScale, kInnerScale, kInnerMarkerDepth,
                 innerColor, batcher);
}

void TransitSchemeBuilder::GenerateStop(
    ref_ptr<dp::GraphicsContext> context, StopNodeParamsPT const & stopParams,
    m2::PointD const & pivot, std::map<routing::transit::LineId, LineParams> const & lines,
    dp::Batcher & batcher)
{
  bool const severalRoads = stopParams.m_stopsInfo.size() > 1;

  if (severalRoads)
  {
    GenerateTransfer(context, stopParams, pivot, batcher);
    return;
  }

  float const kInnerScale = 0.8f;
  float const kOuterScale = 2.0f;

  ::transit::TransitId lineId = *stopParams.m_stopsInfo.begin()->second.m_lines.begin();

  std::string const colorName = df::GetTransitColorName(lines.at(lineId).m_color);
  auto const outerColor = GetColorConstant(colorName);

  auto const innerColor = GetColorConstant(kTransitStopInnerColor);

  m2::PointD const pt = MapShape::ConvertToLocal(stopParams.m_pivot, pivot, kShapeCoordScalar);

  m2::PointD dir = stopParams.m_shapeInfo.m_direction;

  GenerateMarker(context, pt, dir, 1.0f, 1.0f, kOuterScale, kOuterScale, kOuterMarkerDepth,
                 outerColor, batcher);

  GenerateMarker(context, pt, dir, 1.0f, 1.0f, kInnerScale, kInnerScale, kInnerMarkerDepth,
                 innerColor, batcher);
}

void TransitSchemeBuilder::GenerateTitles(ref_ptr<dp::GraphicsContext> context,
                                          StopNodeParamsSubway const & stopParams,
                                          m2::PointD const & pivot,
                                          std::vector<m2::PointF> const & markerSizes,
                                          ref_ptr<dp::TextureManager> textures,
                                          dp::Batcher & batcher)
{
  auto const vs = static_cast<float>(df::VisualParams::Instance().GetVisualScale());

  auto const titles = PlaceTitles(stopParams, kTransitMarkTextSize * vs, textures);
  if (titles.empty())
    return;

  auto const featureId = stopParams.m_stopsInfo.begin()->second.m_featureId;

  auto priority =
      static_cast<uint16_t>(stopParams.m_isTransfer ? Priority::TransferMin : Priority::StopMin);
  priority += static_cast<uint16_t>(stopParams.m_stopsInfo.size());

  auto minVisibleScale = stopParams.m_isTransfer ? kTransferMinZoomLevel : kStopMinZoomLevel;

  bool const isFinalStation = stopParams.m_shapesInfo.size() == 1;
  if (isFinalStation)
  {
    minVisibleScale = std::min(minVisibleScale, kFinalStationMinZoomLevel);
    priority += kFinalStationPriorityInc;
  }

  ASSERT_LESS_OR_EQUAL(
      priority,
      static_cast<uint16_t>(stopParams.m_isTransfer ? Priority::TransferMax : Priority::StopMax),
      ());

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
    textParams.m_specialDisplacement = SpecialDisplacement::SpecialModeUserMark;
    textParams.m_specialPriority = priority;
    textParams.m_startOverlayRank = dp::OverlayRank0;
    textParams.m_minVisibleScale = minVisibleScale;

    TextShape(stopParams.m_pivot, textParams, TileKey(), symbolSizes, title.m_offset, dp::Center,
              kTransitOverlayIndex)
        .Draw(context, &batcher, textures);
  }

  df::ColoredSymbolViewParams colorParams;
  colorParams.m_radiusInPixels = markerSizes.front().x * 0.5f;
  colorParams.m_color = dp::Color::Transparent();
  colorParams.m_featureID = featureId;
  colorParams.m_tileCenter = pivot;
  colorParams.m_depthTestEnabled = false;
  colorParams.m_depthLayer = DepthLayer::TransitSchemeLayer;
  colorParams.m_specialDisplacement = SpecialDisplacement::SpecialModeUserMark;
  colorParams.m_specialPriority = static_cast<uint16_t>(Priority::Stub);
  colorParams.m_startOverlayRank = dp::OverlayRank0;

  ColoredSymbolShape(stopParams.m_pivot, colorParams, TileKey(), kTransitStubOverlayIndex,
                     markerSizes)
      .Draw(context, &batcher, textures);
}

void TransitSchemeBuilder::GenerateTitles(ref_ptr<dp::GraphicsContext> context,
                                          StopNodeParamsPT const & stopParams,
                                          m2::PointD const & pivot,
                                          std::vector<m2::PointF> const & markerSizes,
                                          ref_ptr<dp::TextureManager> textures,
                                          dp::Batcher & batcher)
{
  auto const vs = static_cast<float>(df::VisualParams::Instance().GetVisualScale());

  auto const titles = PlaceTitles(stopParams, kTransitMarkTextSize * vs, textures);
  if (titles.empty())
    return;

  auto const featureId = stopParams.m_stopsInfo.begin()->second.m_featureId;

  auto priority =
      static_cast<uint16_t>(stopParams.m_isTransfer ? Priority::TransferMin : Priority::StopMin);
  priority += static_cast<uint16_t>(stopParams.m_stopsInfo.size());

  auto minVisibleScale = stopParams.m_isTransfer ? kTransferMinZoomLevel : kStopMinZoomLevel;

  if (stopParams.m_isTerminalStop)
  {
    minVisibleScale = std::min(minVisibleScale, kFinalStationMinZoomLevel);
    priority += kFinalStationPriorityInc;
  }

  ASSERT_LESS_OR_EQUAL(
      priority,
      static_cast<uint16_t>(stopParams.m_isTransfer ? Priority::TransferMax : Priority::StopMax),
      ());

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
    textParams.m_specialDisplacement = SpecialDisplacement::SpecialModeUserMark;
    textParams.m_specialPriority = priority;
    textParams.m_startOverlayRank = dp::OverlayRank0;
    textParams.m_minVisibleScale = minVisibleScale;

    TextShape(stopParams.m_pivot, textParams, TileKey(), symbolSizes, title.m_offset, dp::Center,
              kTransitOverlayIndex)
        .Draw(context, &batcher, textures);
  }

  df::ColoredSymbolViewParams colorParams;
  colorParams.m_radiusInPixels = markerSizes.front().x * 0.5f;
  colorParams.m_color = dp::Color::Transparent();
  colorParams.m_featureID = featureId;
  colorParams.m_tileCenter = pivot;
  colorParams.m_depthTestEnabled = false;
  colorParams.m_depthLayer = DepthLayer::TransitSchemeLayer;
  colorParams.m_specialDisplacement = SpecialDisplacement::SpecialModeUserMark;
  colorParams.m_specialPriority = static_cast<uint16_t>(Priority::Stub);
  colorParams.m_startOverlayRank = dp::OverlayRank0;

  ColoredSymbolShape(stopParams.m_pivot, colorParams, TileKey(), kTransitStubOverlayIndex,
                     markerSizes)
      .Draw(context, &batcher, textures);
}

void TransitSchemeBuilder::GenerateTransfer(ref_ptr<dp::GraphicsContext> context,
                                            StopNodeParamsSubway const & stopParams,
                                            m2::PointD const & pivot, dp::Batcher & batcher)
{
  m2::PointD const pt = MapShape::ConvertToLocal(stopParams.m_pivot, pivot, kShapeCoordScalar);

  size_t maxLinesCount = 0;
  m2::PointD dir;
  for (auto const & shapeInfo : stopParams.m_shapesInfo)
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

  GenerateMarker(context, pt, dir, widthLinesCount, maxLinesCount, outerScale, outerScale,
                 kOuterMarkerDepth, outerColor, batcher);

  GenerateMarker(context, pt, dir, widthLinesCount, maxLinesCount, innerScale, innerScale,
                 kInnerMarkerDepth, innerColor, batcher);
}

void TransitSchemeBuilder::GenerateTransfer(ref_ptr<dp::GraphicsContext> context,
                                            StopNodeParamsPT const & stopParams,
                                            m2::PointD const & pivot, dp::Batcher & batcher)
{
  m2::PointD const pt = MapShape::ConvertToLocal(stopParams.m_pivot, pivot, kShapeCoordScalar);

  size_t const maxLinesCount = stopParams.m_shapeInfo.m_linesCount;
  m2::PointD const & dir = stopParams.m_shapeInfo.m_direction;

  float const kInnerScale = 1.0f;
  float const kOuterScale = 1.5f;

  auto const outerColor = GetColorConstant(kTransitTransferOuterColor);
  auto const innerColor = GetColorConstant(kTransitTransferInnerColor);

  float const widthLinesCount = maxLinesCount > 3 ? 1.6f : 1.0f;
  float const innerScale = maxLinesCount == 1 ? 1.4f : kInnerScale;
  float const outerScale = maxLinesCount == 1 ? 1.9f : kOuterScale;

  GenerateMarker(context, pt, dir, widthLinesCount, maxLinesCount, outerScale, outerScale,
                 kOuterMarkerDepth, outerColor, batcher);

  GenerateMarker(context, pt, dir, widthLinesCount, maxLinesCount, innerScale, innerScale,
                 kInnerMarkerDepth, innerColor, batcher);
}
}  // namespace df
