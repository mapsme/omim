#include "transit/world_feed/subway_converter.hpp"

#include "generator/transit_generator.hpp"

#include "routing/fake_feature_ids.hpp"

#include "base/assert.hpp"
#include "base/logging.hpp"

#include <algorithm>
#include <iterator>
#include <limits>

#include "3party/jansson/myjansson.hpp"
#include "3party/opening_hours/opening_hours.hpp"

namespace
{
double constexpr kEps = 1e-5;

uint32_t GetSubwayRouteId(routing::transit::LineId lineId)
{
  return static_cast<uint32_t>(lineId >> 4);
}

// Returns the index of the |point| in the |shape| polyline.
size_t FindPointIndex(std::vector<m2::PointD> const & shape, m2::PointD const & point)
{
  auto it = std::find_if(shape.begin(), shape.end(), [&point](m2::PointD const & p) {
    return base::AlmostEqualAbs(p, point, kEps);
  });

  if (it == shape.end())
  {
    size_t minDistIdx = 0;
    double minDist = std::numeric_limits<double>::max();

    for (size_t i = 0; i < shape.size(); ++i)
    {
      double dist = mercator::DistanceOnEarth(shape[i], point);
      if (dist < minDist)
      {
        minDist = dist;
        minDistIdx = i;
      }
    }

    return minDistIdx;
  }

  return std::distance(shape.begin(), it);
}
}  // namespace

namespace transit
{
std::string const kHashPrefix = "mapsme_transit";
std::string const kDefaultLang = "default";
std::string const kSubwayRouteType = "subway";
std::string const kDefaultHours = "24/7";

SubwayConverter::SubwayConverter(std::string const & subwayJson, WorldFeed & feed)
  : m_subwayJson(subwayJson), m_feed(feed)
{
}

bool SubwayConverter::Convert()
{
  routing::transit::OsmIdToFeatureIdsMap emptyMapping;
  routing::transit::DeserializeFromJson(emptyMapping, m_subwayJson, m_graphData);

  if (!ConvertNetworks())
    return false;

  if (!SplitEdges())
    return false;

  if (!ConvertLinesBasedData())
    return false;

  m_feed.ModifyLinesAndShapes();

  MirrorReversedLines();

  if (!ConvertStops())
    return false;

  if (!ConvertTransfers())
    return false;

  // In contrast to the GTFS gates OSM gates for subways shouldn't be empty.
  if (!ConvertGates())
    return false;

  if (!ConvertEdges())
    return false;

  m_feed.SplitFeedIntoRegions();

  PrepareLinesMetadata();

  return true;
}

bool SubwayConverter::ConvertNetworks()
{
  auto const & networksSubway = m_graphData.GetNetworks();
  m_feed.m_networks.m_data.reserve(networksSubway.size());

  for (auto const & networkSubway : networksSubway)
  {
    // Subway network id is city id index approximately in interval (0, 400).
    TransitId const networkId = networkSubway.GetId();
    CHECK(!routing::FakeFeatureIds::IsTransitFeature(networkId), (networkId));

    Translations const title{{kDefaultLang, networkSubway.GetTitle()}};
    m_feed.m_networks.m_data.emplace(networkId, title);
  }

  LOG(LINFO,
      ("Converted", m_feed.m_networks.m_data.size(), "networks from subways to public transport."));

  return !m_feed.m_networks.m_data.empty();
}

bool SubwayConverter::SplitEdges()
{
  for (auto const & edgeSubway : m_graphData.GetEdges())
  {
    if (edgeSubway.GetTransfer())
      m_edgesTransferSubway.emplace_back(edgeSubway);
    else
      m_edgesSubway.emplace_back(edgeSubway);
  }

  return !m_edgesSubway.empty() && !m_edgesTransferSubway.empty();
}

std::pair<TransitId, RouteData> SubwayConverter::MakeRoute(
    routing::transit::Line const & lineSubway)
{
  uint32_t routeSubwayId = GetSubwayRouteId(lineSubway.GetId());

  std::string const routeHash = BuildHash(kHashPrefix, std::to_string(lineSubway.GetNetworkId()),
                                          std::to_string(routeSubwayId));

  TransitId const routeId = m_feed.m_idGenerator.MakeId(routeHash);

  RouteData routeData;
  routeData.m_title = {{kDefaultLang, lineSubway.GetNumber()}};
  routeData.m_routeType = kSubwayRouteType;
  routeData.m_networkId = lineSubway.GetNetworkId();
  routeData.m_color = lineSubway.GetColor();

  return {routeId, routeData};
}

std::pair<TransitId, GateData> SubwayConverter::MakeGate(routing::transit::Gate const & gateSubway)
{
  // This id is used only for storing gates in gtfs_converter tool. It is not saved to json.
  TransitId const gateId =
      m_feed.m_idGenerator.MakeId(BuildHash(kHashPrefix, std::to_string(gateSubway.GetOsmId())));
  GateData gateData;

  gateData.m_isEntrance = gateSubway.GetEntrance();
  gateData.m_isExit = gateSubway.GetExit();
  gateData.m_point = gateSubway.GetPoint();
  gateData.m_osmId = gateSubway.GetOsmId();

  for (auto stopIdSubway : gateSubway.GetStopIds())
  {
    gateData.m_weights.emplace_back(TimeFromGateToStop(m_stopIdMapping[stopIdSubway] /* stopId */,
                                                       gateSubway.GetWeight() /* timeSeconds */));
  }

  return {gateId, gateData};
}

std::pair<TransitId, TransferData> SubwayConverter::MakeTransfer(
    routing::transit::Transfer const & transferSubway)
{
  TransitId const transferId =
      m_feed.m_idGenerator.MakeId(BuildHash(kHashPrefix, std::to_string(transferSubway.GetId())));

  TransferData transferData;
  transferData.m_point = transferSubway.GetPoint();

  for (auto stopIdSubway : transferSubway.GetStopIds())
    transferData.m_stopsIds.emplace_back(m_stopIdMapping[stopIdSubway]);

  return {transferId, transferData};
}

std::pair<TransitId, LineData> SubwayConverter::MakeLine(routing::transit::Line const & lineSubway,
                                                         TransitId routeId)
{
  TransitId const lineId = lineSubway.GetId();
  CHECK(!routing::FakeFeatureIds::IsTransitFeature(lineId), (lineId));

  LineData lineData;
  lineData.m_routeId = routeId;
  lineData.m_title = {{kDefaultLang, lineSubway.GetTitle()}};
  lineData.m_intervals = {LineInterval(lineSubway.GetInterval() /* headwayS */,
                                       osmoh::OpeningHours(kDefaultHours) /* timeIntervals */)};
  lineData.m_serviceDays = osmoh::OpeningHours(kDefaultHours);

  return {lineId, lineData};
}

std::pair<EdgeId, EdgeData> SubwayConverter::MakeEdge(routing::transit::Edge const & edgeSubway)
{
  auto const lineId = edgeSubway.GetLineId();
  EdgeId const edgeId(m_stopIdMapping[edgeSubway.GetStop1Id()],
                      m_stopIdMapping[edgeSubway.GetStop2Id()], lineId);
  EdgeData edgeData;
  edgeData.m_weight = edgeSubway.GetWeight();

  auto const it = m_edgesOnShapes.find(edgeId);
  CHECK(it != m_edgesOnShapes.end(), (lineId));

  m2::PointD const pointStart = it->second.first;
  m2::PointD const pointEnd = it->second.second;

  edgeData.m_shapeLink.m_shapeId = m_feed.m_lines.m_data[lineId].m_shapeLink.m_shapeId;

  auto itShape = m_feed.m_shapes.m_data.find(edgeData.m_shapeLink.m_shapeId);
  CHECK(itShape != m_feed.m_shapes.m_data.end(), ("Shape does not exist."));

  auto const & shapePoints = itShape->second.m_points;
  CHECK(!shapePoints.empty(), ("Shape is empty."));

  edgeData.m_shapeLink.m_startIndex = FindPointIndex(shapePoints, pointStart);
  edgeData.m_shapeLink.m_endIndex = FindPointIndex(shapePoints, pointEnd);

  return {edgeId, edgeData};
}

std::pair<EdgeTransferId, size_t> SubwayConverter::MakeEdgeTransfer(
    routing::transit::Edge const & edgeSubway)
{
  EdgeTransferId const edgeTransferId(m_stopIdMapping[edgeSubway.GetStop1Id()] /* fromStopId */,
                                      m_stopIdMapping[edgeSubway.GetStop2Id()] /* toStopId */);
  return {edgeTransferId, edgeSubway.GetWeight()};
}

std::pair<TransitId, StopData> SubwayConverter::MakeStop(routing::transit::Stop const & stopSubway)
{
  TransitId const stopId = m_stopIdMapping[stopSubway.GetId()];

  StopData stopData;
  stopData.m_point = stopSubway.GetPoint();
  stopData.m_osmId = stopSubway.GetOsmId();

  if (stopSubway.GetFeatureId() != routing::transit::kInvalidFeatureId)
    stopData.m_featureId = stopSubway.GetFeatureId();

  return {stopId, stopData};
}

bool SubwayConverter::ConvertLinesBasedData()
{
  auto const & linesSubway = m_graphData.GetLines();
  m_feed.m_lines.m_data.reserve(linesSubway.size());
  m_feed.m_shapes.m_data.reserve(linesSubway.size());

  std::unordered_map<TransitId, IdList> routesToLines;

  for (auto const & lineSubway : linesSubway)
  {
    auto const lineId = lineSubway.GetId();
    auto const [routeId, routeData] = MakeRoute(lineSubway);
    auto & lineIdList = routesToLines[routeId];
    auto it = std::find(lineIdList.begin(), lineIdList.end(), lineId);
    if (it != lineIdList.end())
      lineIdList.push_back(lineId);
  }

  auto const & shapesSubway = m_graphData.GetShapes();

  for (auto const & lineSubway : linesSubway)
  {
    auto const [routeId, routeData] = MakeRoute(lineSubway);

    m_feed.m_routes.m_data.emplace(routeId, routeData);

    auto [lineId, lineData] = MakeLine(lineSubway, routeId);

    TransitId const shapeId = m_feed.m_idGenerator.MakeId(
        BuildHash(kHashPrefix, std::string("shape"), std::to_string(lineId)));
    lineData.m_shapeId = shapeId;

    ShapeData shapeData;
    shapeData.m_lineIds.insert(lineId);

    CHECK_EQUAL(lineSubway.GetStopIds().size(), 1, ("Line shouldn't be split into ranges."));

    auto const & stopIdsSubway = lineSubway.GetStopIds().front();

    CHECK_GREATER(stopIdsSubway.size(), 1, ("Range must include at least two stops."));

    for (size_t i = 0; i < stopIdsSubway.size(); ++i)
    {
      auto const stopIdSubway = stopIdsSubway[i];
      std::string const stopHash = BuildHash(kHashPrefix, std::to_string(stopIdSubway));
      TransitId const stopId = m_feed.m_idGenerator.MakeId(stopHash);

      lineData.m_stopIds.emplace_back(stopId);
      m_stopIdMapping.emplace(stopIdSubway, stopId);

      if (i == 0)
        continue;

      auto const stopIdSubwayPrev = stopIdsSubway[i - 1];

      CHECK(stopIdSubwayPrev != stopIdSubway, (stopIdSubway));

      auto const & edge = FindEdge(stopIdSubwayPrev, stopIdSubway, lineId);

      CHECK_LESS_OR_EQUAL(edge.GetShapeIds().size(), 1, (edge));

      EdgePoints edgePoints;

      m2::PointD const prevPoint = FindById(m_graphData.GetStops(), stopIdSubwayPrev)->GetPoint();
      m2::PointD const curPoint = FindById(m_graphData.GetStops(), stopIdSubway)->GetPoint();

      if (edge.GetShapeIds().empty())
      {
        edgePoints = EdgePoints(prevPoint, curPoint);
      }
      else
      {
        routing::transit::ShapeId shapeIdSubway = edge.GetShapeIds().back();

        auto polyline = FindById(shapesSubway, shapeIdSubway)->GetPolyline();

        CHECK(polyline.size() > 1, ());

        double const distToPrevStop = mercator::DistanceOnEarth(polyline.front(), prevPoint);
        double const distToNextStop = mercator::DistanceOnEarth(polyline.front(), curPoint);

        if (distToPrevStop > distToNextStop)
          std::reverse(polyline.begin(), polyline.end());

        // We remove duplicate point from the shape before appending polyline to it.
        if (!shapeData.m_points.empty() && shapeData.m_points.back() == polyline.front())
          shapeData.m_points.pop_back();

        shapeData.m_points.insert(shapeData.m_points.end(), polyline.begin(), polyline.end());

        edgePoints = EdgePoints(polyline.front(), polyline.back());
      }

      EdgeId const curEdge(m_stopIdMapping[stopIdSubwayPrev], stopId, lineId);

      if (!m_edgesOnShapes.emplace(curEdge, edgePoints).second)
      {
        LOG(LWARNING, ("Edge duplicate in subways. stop1_id", stopIdSubwayPrev, "stop2_id",
                       stopIdSubway, "line_id", lineId));
      }
    }

    m_feed.m_lines.m_data.emplace(lineId, lineData);
    m_feed.m_shapes.m_data.emplace(shapeId, shapeData);
  }

  LOG(LDEBUG, ("Converted", m_feed.m_routes.m_data.size(), "routes,", m_feed.m_lines.m_data.size(),
               "lines from subways to public transport."));

  return !m_feed.m_lines.m_data.empty();
}

bool SubwayConverter::ConvertStops()
{
  auto const & stopsSubway = m_graphData.GetStops();
  m_feed.m_stops.m_data.reserve(stopsSubway.size());

  for (auto const & stopSubway : stopsSubway)
    m_feed.m_stops.m_data.emplace(MakeStop(stopSubway));

  LOG(LINFO,
      ("Converted", m_feed.m_stops.m_data.size(), "stops from subways to public transport."));

  return !m_feed.m_stops.m_data.empty();
}

bool SubwayConverter::ConvertTransfers()
{
  auto const & transfersSubway = m_graphData.GetTransfers();
  m_feed.m_transfers.m_data.reserve(transfersSubway.size());

  for (auto const & transferSubway : transfersSubway)
  {
    auto const [transferId, transferData] = MakeTransfer(transferSubway);
    m_feed.m_transfers.m_data.emplace(transferId, transferData);

    // All stops are already present in |m_feed| before the |ConvertTransfers()| call.
    for (auto stopId : transferData.m_stopsIds)
      LinkTransferIdToStop(m_feed.m_stops.m_data.at(stopId), transferId);
  }

  LOG(LINFO, ("Converted", m_feed.m_transfers.m_data.size(),
              "transfers from subways to public transport."));

  return !m_feed.m_transfers.m_data.empty();
}

bool SubwayConverter::ConvertGates()
{
  auto const & gatesSubway = m_graphData.GetGates();
  m_feed.m_gates.m_data.reserve(gatesSubway.size());

  for (auto const & gateSubway : gatesSubway)
  {
    auto const [gateId, gateData] = MakeGate(gateSubway);
    m_feed.m_gates.m_data.emplace(gateId, gateData);
  }

  LOG(LINFO,
      ("Converted", m_feed.m_gates.m_data.size(), "gates from subways to public transport."));

  return !m_feed.m_gates.m_data.empty();
}

bool SubwayConverter::ConvertEdges()
{
  for (auto const & edgeSubway : m_edgesSubway)
    m_feed.m_edges.m_data.emplace(MakeEdge(edgeSubway));

  LOG(LINFO,
      ("Converted", m_feed.m_edges.m_data.size(), "edges from subways to public transport."));

  for (auto const & edgeTransferSubway : m_edgesTransferSubway)
    m_feed.m_edgesTransfers.m_data.emplace(MakeEdgeTransfer(edgeTransferSubway));

  LOG(LINFO, ("Converted", m_feed.m_edgesTransfers.m_data.size(),
              "edges from subways to transfer edges in public transport."));

  return !m_feed.m_edges.m_data.empty() && !m_feed.m_edgesTransfers.m_data.empty();
}

void SubwayConverter::MirrorReversedLines()
{
  for (auto & [lineId, lineData] : m_feed.m_lines.m_data)
  {
    if (lineData.m_shapeLink.m_startIndex < lineData.m_shapeLink.m_endIndex)
      continue;

    auto revStopIds = lineData.m_stopIds;
    std::reverse(revStopIds.begin(), revStopIds.end());

    bool reversed = false;

    for (auto const & [lineIdStraight, lineDataStraight] : m_feed.m_lines.m_data)
    {
      if (lineIdStraight == lineId ||
          lineDataStraight.m_shapeLink.m_startIndex > lineDataStraight.m_shapeLink.m_endIndex ||
          lineDataStraight.m_shapeLink.m_shapeId != lineData.m_shapeLink.m_shapeId)
        continue;

      if (revStopIds == lineDataStraight.m_stopIds)
      {
        lineData.m_shapeLink = lineDataStraight.m_shapeLink;
        LOG(LDEBUG, ("Reversed line", lineId, "to line", lineIdStraight, "shapeLink",
                     lineData.m_shapeLink));
        reversed = true;
        break;
      }
    }

    if (!reversed)
    {
      std::swap(lineData.m_shapeLink.m_startIndex, lineData.m_shapeLink.m_endIndex);
      LOG(LDEBUG, ("Reversed line", lineId, "shapeLink", lineData.m_shapeLink));
    }
  }
}

std::vector<LineSchemeData> SubwayConverter::GetLinesOnScheme(
    std::unordered_map<TransitId, IdList> const & lineIdToStops) const
{
  // Route id to shape link and one of line ids with this link.
  std::map<TransitId, std::map<ShapeLink, TransitId>> routeToLines;

  for (auto const & lineAndStops : lineIdToStops)
  {
    TransitId const lineId = lineAndStops.first;
    LineData const & line = m_feed.m_lines.m_data.at(lineId);

    TransitId const routeId = line.m_routeId;
    ShapeLink const & newShapeLink = line.m_shapeLink;

    auto [it, inserted] = routeToLines.emplace(routeId, std::map<ShapeLink, TransitId>());

    if (inserted)
    {
      it->second[newShapeLink] = lineId;
      continue;
    }

    bool insert = true;
    std::vector<ShapeLink> linksForRemoval;

    for (auto const & [shapeLink, lineId] : it->second)
    {
      if (shapeLink.m_shapeId != newShapeLink.m_shapeId)
        continue;

      // New shape link is fully included into the existing one.
      if (shapeLink.m_startIndex <= newShapeLink.m_startIndex &&
          shapeLink.m_endIndex >= newShapeLink.m_endIndex)
      {
        insert = false;
        continue;
      }

      // Existing shape link is fully included into the new one. It should be removed.
      if (newShapeLink.m_startIndex <= shapeLink.m_startIndex &&
          newShapeLink.m_endIndex >= shapeLink.m_endIndex)
        linksForRemoval.push_back(shapeLink);
    }

    for (auto const sl : linksForRemoval)
      it->second.erase(sl);

    if (insert)
      it->second[newShapeLink] = lineId;
  }

  std::vector<LineSchemeData> linesOnScheme;

  for (auto const & [routeId, linksToLines] : routeToLines)
  {
    CHECK(!linksToLines.empty(), (routeId));

    for (auto const & [shapeLink, lineId] : linksToLines)
    {
      LineSchemeData data;
      data.m_lineId = lineId;
      data.m_routeId = routeId;
      data.m_shapeLink = shapeLink;

      linesOnScheme.push_back(data);
    }
  }

  return linesOnScheme;
}

enum class LineSegmentState
{
  Start = 0,
  Finish
};

struct LineSegmentInfo
{
  LineSegmentInfo() = default;
  LineSegmentInfo(LineSegmentState const & state, bool codirectional)
    : m_state(state), m_codirectional(codirectional)
  {
  }

  LineSegmentState m_state = LineSegmentState::Start;
  bool m_codirectional = false;
};

struct LinePointState
{
  std::map<TransitId, LineSegmentInfo> parallelLineStates;
  m2::PointD m_firstPoint;
};

using LineGeometry = std::vector<m2::PointD>;
using RouteToLinepartsCache = std::map<TransitId, std::vector<LineGeometry>>;

bool Equal(LineGeometry const & line1, LineGeometry const & line2)
{
  if (line1.size() != line2.size())
    return false;

  for (size_t i = 0; i < line1.size(); ++i)
  {
    if (!base::AlmostEqualAbs(line1[i], line2[i], kEps))
      return false;
  }

  return true;
}

bool AddToCache(TransitId routeId, LineGeometry const & linePart, RouteToLinepartsCache & cache)
{
  auto [it, inserted] = cache.emplace(routeId, std::vector<LineGeometry>());

  if (inserted)
  {
    it->second.push_back(linePart);
    return true;
  }

  std::vector<m2::PointD> linePartRev = linePart;
  std::reverse(linePartRev.begin(), linePartRev.end());

  for (LineGeometry const & cachedPart : it->second)
  {
    if (Equal(cachedPart, linePart) || Equal(cachedPart, linePartRev))
      return false;
  }

  it->second.push_back(linePart);
  return true;
}

void SubwayConverter::CalculateLinePriorities(std::vector<LineSchemeData> const & linesOnScheme)
{
  RouteToLinepartsCache routeSegmentsCache;

  for (auto const & lineSchemeData : linesOnScheme)
  {
    auto const lineId = lineSchemeData.m_lineId;
    std::map<size_t, LinePointState> linePoints;

    for (auto const & linePart : lineSchemeData.m_lineParts)
    {
      auto & startPointState = linePoints[linePart.m_segment.m_startIdx];
      auto & endPointState = linePoints[linePart.m_segment.m_endIdx];

      startPointState.m_firstPoint = linePart.m_firstPoint;

      for (auto const & [parallelLineId, parallelFirstPoint] : linePart.m_commonLines)
      {
        bool const codirectional =
            base::AlmostEqualAbs(linePart.m_firstPoint, parallelFirstPoint, kEps);

        startPointState.parallelLineStates[parallelLineId] =
            LineSegmentInfo(LineSegmentState::Start, codirectional);
        endPointState.parallelLineStates[parallelLineId] =
            LineSegmentInfo(LineSegmentState::Finish, codirectional);
      }
    }

    linePoints.emplace(0, LinePointState());
    linePoints.emplace(lineSchemeData.m_shapeLink.m_endIndex, LinePointState());

    std::map<TransitId, bool> parallelLines;

    for (auto it = linePoints.begin(); it != linePoints.end(); ++it)
    {
      auto itNext = std::next(it);
      if (itNext == linePoints.end())
        break;

      auto & startLinePointState = it->second;
      size_t startIndex = it->first;
      size_t endIndex = itNext->first;

      for (auto const & [id, info] : startLinePointState.parallelLineStates)
      {
        if (info.m_state == LineSegmentState::Start)
        {
          auto [itParLine, insertedParLine] = parallelLines.emplace(id, info.m_codirectional);
          if (!insertedParLine)
            CHECK_EQUAL(itParLine->second, info.m_codirectional, ());
        }
        else
        {
          parallelLines.erase(id);
        }
      }

      TransitId routeId = m_feed.m_lines.m_data.at(lineId).m_routeId;

      std::map<TransitId, bool> routeIds{{routeId, true /* codirectional */}};

      for (auto const & [id, codirectional] : parallelLines)
        routeIds.emplace(m_feed.m_lines.m_data.at(id).m_routeId, codirectional);

      LineSegmentOrder lso;
      lso.m_segment = LineSegment(startIndex, endIndex);

      auto const & polyline =
          m_feed.m_shapes.m_data.at(lineSchemeData.m_shapeLink.m_shapeId).m_points;

      if (!AddToCache(routeId,
                      GetPolylinePart(polyline, lso.m_segment.m_startIdx, lso.m_segment.m_endIdx),
                      routeSegmentsCache))
      {
        continue;
      }

      auto itRoute = routeIds.find(routeId);
      CHECK(itRoute != routeIds.end(), ());

      size_t const index = std::distance(routeIds.begin(), itRoute);

      lso.m_order = CalcSegmentOrder(index, routeIds.size());

      bool reversed = false;

      if (index > 0 && !routeIds.begin()->second)
      {
        lso.m_order = -lso.m_order;
        reversed = true;
      }

      m_feed.m_linesMetadata.m_data[lineId].push_back(lso);

      LOG(LINFO,
          ("routeId", routeId, "lineId", lineId, "start", startIndex, "end", endIndex, "len",
           endIndex - startIndex + 1, "order", lso.m_order, "index", index, "reversed", reversed,
           "|| lines count:", parallelLines.size(), "routes count:", routeIds.size()));
    }
  }
}

void SubwayConverter::PrepareLinesMetadata()
{
  for (auto const & [region, lineIdToStops] : m_feed.m_splitting.m_lines)
  {
    LOG(LINFO, ("Handling", region, "region"));

    std::vector<LineSchemeData> linesOnScheme = GetLinesOnScheme(lineIdToStops);

    for (size_t i = 0; i < linesOnScheme.size() - 1; ++i)
    {
      auto & line1 = linesOnScheme[i];

      auto const & shapeLink1 = line1.m_shapeLink;

      auto const polyline1 =
          GetPolylinePart(m_feed.m_shapes.m_data.at(shapeLink1.m_shapeId).m_points,
                          shapeLink1.m_startIndex, shapeLink1.m_endIndex);

      for (size_t j = i + 1; j < linesOnScheme.size(); ++j)
      {
        auto & line2 = linesOnScheme[j];

        if (line1.m_routeId == line2.m_routeId)
          continue;

        auto const & shapeLink2 = line2.m_shapeLink;

        if (line1.m_shapeLink.m_shapeId == line2.m_shapeLink.m_shapeId)
        {
          CHECK_LESS(shapeLink1.m_startIndex, shapeLink1.m_endIndex, ());
          CHECK_LESS(shapeLink2.m_startIndex, shapeLink2.m_endIndex, ());

          std::optional<LineSegment> inter =
              GetIntersection(shapeLink1.m_startIndex, shapeLink1.m_endIndex,
                              shapeLink2.m_startIndex, shapeLink2.m_endIndex);

          if (inter != std::nullopt)
          {
            LineSegment const segment = inter.value();
            m2::PointD const & startPoint = polyline1[segment.m_startIdx];

            UpdateLinePart(line1.m_lineParts, segment, startPoint, line2.m_lineId, startPoint);
            UpdateLinePart(line2.m_lineParts, segment, startPoint, line1.m_lineId, startPoint);
          }
        }
        else
        {
          auto polyline2 = GetPolylinePart(m_feed.m_shapes.m_data.at(shapeLink2.m_shapeId).m_points,
                                           shapeLink2.m_startIndex, shapeLink2.m_endIndex);

          auto [segments1, segments2] = FindIntersections(polyline1, polyline2);

          if (segments1.empty())
          {
            auto polyline2Rev = polyline2;
            std::reverse(polyline2Rev.begin(), polyline2Rev.end());
            std::tie(segments1, segments2) = FindIntersections(polyline1, polyline2Rev);

            if (!segments1.empty())
            {
              for (auto & seg : segments2)
              {
                size_t len = seg.m_endIdx - seg.m_startIdx + 1;
                seg.m_endIdx = polyline2.size() - seg.m_startIdx - 1;
                seg.m_startIdx = seg.m_endIdx - len + 1;

                CHECK_GREATER(seg.m_endIdx, seg.m_startIdx, ());
                CHECK_GREATER(polyline2.size(), seg.m_endIdx, ());
              }
            }
          }

          CHECK_EQUAL(segments1.size(), segments2.size(), ());

          if (!segments1.empty())
          {
            for (size_t k = 0; k < segments1.size(); ++k)
            {
              m2::PointD const & startPoint1 = polyline1[segments1[k].m_startIdx];
              m2::PointD const & endPoint1 = polyline1[segments1[k].m_endIdx];

              m2::PointD const & startPoint2 = polyline2[segments2[k].m_startIdx];
              m2::PointD const & endPoint2 = polyline2[segments2[k].m_endIdx];

              CHECK(base::AlmostEqualAbs(startPoint1, startPoint2, kEps) &&
                            base::AlmostEqualAbs(endPoint1, endPoint2, kEps) ||
                        base::AlmostEqualAbs(startPoint1, endPoint2, kEps) &&
                            base::AlmostEqualAbs(endPoint1, startPoint2, kEps),
                    ());

              segments1[k].m_startIdx += shapeLink1.m_startIndex;
              segments1[k].m_endIdx += shapeLink1.m_startIndex;

              segments2[k].m_startIdx += shapeLink2.m_startIndex;
              segments2[k].m_endIdx += shapeLink2.m_startIndex;

              UpdateLinePart(line1.m_lineParts, segments1[k], startPoint1, line2.m_lineId,
                             startPoint2);
              UpdateLinePart(line2.m_lineParts, segments2[k], startPoint2, line1.m_lineId,
                             startPoint1);
            }

            LOG(LINFO, ("Line", line1.m_lineId, "route", line1.m_routeId, "overlaps line",
                        line2.m_lineId, "route", line2.m_routeId));
          }
        }
      }
    }

    LOG(LINFO, ("Started calculating line priorities in", region));
    CalculateLinePriorities(linesOnScheme);
    LOG(LINFO, ("Prepared metadata for lines in", region));
  }
}

routing::transit::Edge SubwayConverter::FindEdge(routing::transit::StopId stop1Id,
                                                 routing::transit::StopId stop2Id,
                                                 routing::transit::LineId lineId) const
{
  auto const itEdge = std::find_if(m_edgesSubway.begin(), m_edgesSubway.end(),
                                   [stop1Id, stop2Id, lineId](routing::transit::Edge const & edge) {
                                     return edge.GetStop1Id() == stop1Id &&
                                            edge.GetStop2Id() == stop2Id &&
                                            edge.GetLineId() == lineId;
                                   });

  CHECK(itEdge != m_edgesSubway.end(), (stop1Id, stop2Id, lineId));

  return *itEdge;
}
}  // namespace transit
