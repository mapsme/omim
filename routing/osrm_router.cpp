#include "car_model.hpp"
#include "cross_mwm_router.hpp"
#include "online_cross_fetcher.hpp"
#include "osrm2feature_map.hpp"
#include "osrm_router.hpp"
#include "turns_generator.hpp"

#include "platform/country_file.hpp"
#include "platform/platform.hpp"

#include "geometry/angles.hpp"
#include "geometry/distance.hpp"
#include "geometry/distance_on_sphere.hpp"

#include "indexer/ftypes_matcher.hpp"
#include "indexer/mercator.hpp"
#include "indexer/index.hpp"
#include "indexer/scales.hpp"

#include "coding/reader_wrapper.hpp"

#include "base/logging.hpp"
#include "base/math.hpp"
#include "base/scope_guard.hpp"
#include "base/timer.hpp"

#include "std/algorithm.hpp"
#include "std/limits.hpp"
#include "std/string.hpp"

#include "3party/osrm/osrm-backend/data_structures/query_edge.hpp"
#include "3party/osrm/osrm-backend/data_structures/internal_route_result.hpp"
#include "3party/osrm/osrm-backend/descriptors/description_factory.hpp"

#define INTERRUPT_WHEN_CANCELLED(DELEGATE) \
  do                               \
  {                                \
    if (DELEGATE.IsCancelled())    \
      return Cancelled;            \
  } while (false)

namespace routing
{

namespace
{
size_t constexpr kMaxNodeCandidatesCount = 10;
double constexpr kFeatureFindingRectSideRadiusMeters = 1000.0;
double constexpr kMwmLoadedProgress = 10.0f;
double constexpr kPointsFoundProgress = 15.0f;
double constexpr kCrossPathFoundProgress = 50.0f;
double constexpr kPathFoundProgress = 70.0f;
} //  namespace
// TODO (ldragunov) Switch all RawRouteData and incapsulate to own omim types.
using RawRouteData = InternalRouteResult;

namespace
{

class Point2PhantomNode
{

public:
  Point2PhantomNode(OsrmFtSegMapping const & mapping, Index const * pIndex,
                    m2::PointD const & direction, TDataFacade const & facade)
      : m_direction(direction), m_mapping(mapping), m_pIndex(pIndex), m_dataFacade(facade)
  {
  }

  struct Candidate
  {
    double m_dist;
    uint32_t m_segIdx;
    uint32_t m_fid;
    m2::PointD m_point;

    Candidate() : m_dist(numeric_limits<double>::max()), m_fid(kInvalidFid) {}
  };

  static void FindNearestSegment(FeatureType const & ft, m2::PointD const & point, Candidate & res)
  {
    ft.ParseGeometry(FeatureType::BEST_GEOMETRY);

    size_t const count = ft.GetPointsCount();
    uint32_t const featureId = ft.GetID().m_index;
    ASSERT_GREATER(count, 1, ());
    for (size_t i = 1; i < count; ++i)
    {
      m2::ProjectionToSection<m2::PointD> segProj;
      segProj.SetBounds(ft.GetPoint(i - 1), ft.GetPoint(i));

      m2::PointD const pt = segProj(point);
      double const d = point.SquareLength(pt);
      if (d < res.m_dist)
      {
        res.m_dist = d;
        res.m_fid = featureId;
        res.m_segIdx = static_cast<uint32_t>(i - 1);
        res.m_point = pt;
      }
    }
  }

  void SetPoint(m2::PointD const & pt)
  {
    m_point = pt;
  }

  bool HasCandidates() const
  {
    return !m_candidates.empty();
  }

  void operator() (FeatureType const & ft)
  {
    static CarModel const carModel;
    if (ft.GetFeatureType() != feature::GEOM_LINE || !carModel.IsRoad(ft))
      return;

    Candidate res;

    FindNearestSegment(ft, m_point, res);

    if (!m_mwmId.IsAlive())
      m_mwmId = ft.GetID().m_mwmId;
    ASSERT_EQUAL(m_mwmId, ft.GetID().m_mwmId, ());

    if (res.m_fid != kInvalidFid)
      m_candidates.push_back(res);
  }

  double CalculateDistance(OsrmMappingTypes::FtSeg const & s) const
  {
    ASSERT_NOT_EQUAL(s.m_pointStart, s.m_pointEnd, ());

    Index::FeaturesLoaderGuard loader(*m_pIndex, m_mwmId);
    FeatureType ft;
    loader.GetFeatureByIndex(s.m_fid, ft);
    ft.ParseGeometry(FeatureType::BEST_GEOMETRY);

    double distMeters = 0.0;
    size_t const n = max(s.m_pointEnd, s.m_pointStart);
    size_t i = min(s.m_pointStart, s.m_pointEnd) + 1;
    do
    {
      distMeters += MercatorBounds::DistanceOnEarth(ft.GetPoint(i - 1), ft.GetPoint(i));
      ++i;
    } while (i <= n);

    return distMeters;
  }

  /// Calculates part of a node weight in the OSRM format. Projection point @segPt divides node on
  /// two parts. So we find weight of a part, set by the @offsetToRight parameter.
  void CalculateWeight(OsrmMappingTypes::FtSeg const & seg, m2::PointD const & segPt, NodeID & nodeId, int & weight, bool offsetToRight) const
  {
    // nodeId can be INVALID_NODE_ID when reverse node is absent. This node has no weight.
    if (nodeId == INVALID_NODE_ID || m_dataFacade.GetOutDegree(nodeId) == 0)
    {
      weight = 0;
      return;
    }

    // Distance from the node border to the projection point in meters.
    double distance = 0;
    auto const range = m_mapping.GetSegmentsRange(nodeId);
    OsrmMappingTypes::FtSeg segment;
    OsrmMappingTypes::FtSeg currentSegment;

    size_t startIndex = offsetToRight ? range.second - 1 : range.first;
    size_t endIndex = offsetToRight ? range.first - 1 : range.second;
    int indexIncrement = offsetToRight ? -1 : 1;

    for (size_t segmentIndex = startIndex; segmentIndex != endIndex; segmentIndex += indexIncrement)
    {
      m_mapping.GetSegmentByIndex(segmentIndex, segment);
      if (!segment.IsValid())
        continue;

      auto segmentLeft = min(segment.m_pointStart, segment.m_pointEnd);
      auto segmentRight = max(segment.m_pointEnd, segment.m_pointStart);

      // seg.m_pointEnd - seg.m_pointStart == 1, so check
      // just a case, when seg is inside s
      if ((seg.m_pointStart != segmentLeft || seg.m_pointEnd != segmentRight) &&
          (segmentLeft <= seg.m_pointStart && segmentRight >= seg.m_pointEnd))
      {
        currentSegment.m_fid = segment.m_fid;

        if (segment.m_pointStart < segment.m_pointEnd)
        {
          if (offsetToRight)
          {
            currentSegment.m_pointEnd = seg.m_pointEnd;
            currentSegment.m_pointStart = segment.m_pointStart;
          }
          else
          {
            currentSegment.m_pointStart = seg.m_pointStart;
            currentSegment.m_pointEnd = segment.m_pointEnd;
          }
        }
        else
        {
          if (offsetToRight)
          {
            currentSegment.m_pointStart = segment.m_pointEnd;
            currentSegment.m_pointEnd = seg.m_pointEnd;
          }
          else
          {
            currentSegment.m_pointEnd = seg.m_pointStart;
            currentSegment.m_pointStart = segment.m_pointStart;
          }
        }

        distance += CalculateDistance(currentSegment);
        break;
      }
      else
        distance += CalculateDistance(segment);
    }

    Index::FeaturesLoaderGuard loader(*m_pIndex, m_mwmId);
    FeatureType ft;
    loader.GetFeatureByIndex(seg.m_fid, ft);
    ft.ParseGeometry(FeatureType::BEST_GEOMETRY);

    // node.m_seg always forward ordered (m_pointStart < m_pointEnd)
    distance -= MercatorBounds::DistanceOnEarth(ft.GetPoint(offsetToRight ? seg.m_pointEnd : seg.m_pointStart), segPt);

    // Offset is measured in decades of seconds. We don't know about speed restrictions on the road.
    // So we find it by a whole edge weight.

    // Whole node distance in meters.
    double fullDistanceM = 0.0;
    for (size_t segmentIndex = startIndex; segmentIndex != endIndex; segmentIndex += indexIncrement)
    {
      m_mapping.GetSegmentByIndex(segmentIndex, segment);
      if (!segment.IsValid())
        continue;
      fullDistanceM += CalculateDistance(segment);
    }

    ASSERT_GREATER(fullDistanceM, 0, ("No valid segments on the edge."));
    double const ratio = (fullDistanceM == 0) ? 0 : distance / fullDistanceM;

    auto const beginEdge = m_dataFacade.BeginEdges(nodeId);
    auto const endEdge = m_dataFacade.EndEdges(nodeId);
    // Minimal OSRM edge weight in decades of seconds.
    EdgeWeight minWeight = m_dataFacade.GetEdgeData(beginEdge, nodeId).distance;
    for (EdgeID i = beginEdge + 1; i != endEdge; ++i)
      minWeight = min(m_dataFacade.GetEdgeData(i, nodeId).distance, minWeight);

    weight = max(static_cast<int>(minWeight * ratio), 0);
  }

  void CalculateOffsets(FeatureGraphNode & node) const
  {
    //CalculateWeight(node.segment, node.segmentPoint, node.node.forward_node_id, node.node.forward_weight, true /* offsetToRight */);
    //CalculateWeight(node.segment, node.segmentPoint, node.node.reverse_node_id, node.node.reverse_weight, false /* offsetToRight */);

    // need to initialize weights for correct work of PhantomNode::GetForwardWeightPlusOffset
    // and PhantomNode::GetReverseWeightPlusOffset
    node.node.forward_offset = 0;
    node.node.reverse_offset = 0;
    node.node.forward_weight = 0;
    node.node.reverse_weight = 0;
  }

  void MakeResult(TFeatureGraphNodeVec & res, size_t maxCount, string const & mwmName)
  {
    if (!m_mwmId.IsAlive())
      return;

    vector<OsrmMappingTypes::FtSeg> segments;

    segments.resize(maxCount);

    OsrmFtSegMapping::FtSegSetT segmentSet;
    sort(m_candidates.begin(), m_candidates.end(), [] (Candidate const & r1, Candidate const & r2)
    {
      return (r1.m_dist < r2.m_dist);
    });

    size_t const n = min(m_candidates.size(), maxCount);
    for (size_t j = 0; j < n; ++j)
    {
      OsrmMappingTypes::FtSeg & seg = segments[j];
      Candidate const & c = m_candidates[j];

      seg.m_fid = c.m_fid;
      seg.m_pointStart = c.m_segIdx;
      seg.m_pointEnd = c.m_segIdx + 1;

      segmentSet.insert(&seg);
    }

    OsrmFtSegMapping::OsrmNodesT nodes;
    m_mapping.GetOsrmNodes(segmentSet, nodes);

    res.clear();
    res.resize(maxCount);

    for (size_t j = 0; j < maxCount; ++j)
    {
      size_t const idx = j;

      if (!segments[idx].IsValid())
        continue;

      auto it = nodes.find(segments[idx].Store());
      if (it == nodes.end())
        continue;

      FeatureGraphNode & node = res[idx];

      if (!m_direction.IsAlmostZero())
      {
        // Filter income nodes by direction mode
        OsrmMappingTypes::FtSeg const & node_seg = segments[idx];
        FeatureType feature;
        Index::FeaturesLoaderGuard loader(*m_pIndex, m_mwmId);
        loader.GetFeatureByIndex(node_seg.m_fid, feature);
        feature.ParseGeometry(FeatureType::BEST_GEOMETRY);
        m2::PointD const featureDirection = feature.GetPoint(node_seg.m_pointEnd) - feature.GetPoint(node_seg.m_pointStart);
        bool const sameDirection = (m2::DotProduct(featureDirection, m_direction) / (featureDirection.Length() * m_direction.Length()) > 0);
        if (sameDirection)
        {
          node.node.forward_node_id = it->second.first;
          node.node.reverse_node_id = INVALID_NODE_ID;
        }
        else
        {
          node.node.forward_node_id = INVALID_NODE_ID;
          node.node.reverse_node_id = it->second.second;
        }
      }
      else
      {
        node.node.forward_node_id = it->second.first;
        node.node.reverse_node_id = it->second.second;
      }

      node.segment = segments[idx];
      node.segmentPoint = m_candidates[j].m_point;
      node.mwmName = mwmName;

      CalculateOffsets(node);
    }
    res.erase(remove_if(res.begin(), res.end(), [](FeatureGraphNode const & f)
                        {
                          return f.mwmName.empty();
                        }),
                        res.end());
  }

private:
  m2::PointD m_point;
  m2::PointD const m_direction;
  OsrmFtSegMapping const & m_mapping;
  buffer_vector<Candidate, 128> m_candidates;
  MwmSet::MwmId m_mwmId;
  Index const * m_pIndex;
  TDataFacade const & m_dataFacade;

  DISALLOW_COPY(Point2PhantomNode);
};
} // namespace

// static
bool OsrmRouter::CheckRoutingAbility(m2::PointD const & startPoint, m2::PointD const & finalPoint,
                                     TCountryFileFn const & countryFileFn, Index * index)
{
  RoutingIndexManager manager(countryFileFn, index);
  return manager.GetMappingByPoint(startPoint)->IsValid() &&
         manager.GetMappingByPoint(finalPoint)->IsValid();
}

OsrmRouter::OsrmRouter(Index * index, TCountryFileFn const & countryFileFn)
    : m_pIndex(index), m_indexManager(countryFileFn, index)
{
}

string OsrmRouter::GetName() const
{
  return "vehicle";
}

void OsrmRouter::ClearState()
{
  m_cachedTargets.clear();
  m_cachedTargetPoint = m2::PointD::Zero();
  m_indexManager.Clear();
}

bool OsrmRouter::FindRouteFromCases(TFeatureGraphNodeVec const & source,
                                    TFeatureGraphNodeVec const & target, TDataFacade & facade,
                                    RawRoutingResult & rawRoutingResult)
{
  /// @todo (ldargunov) make more complex nearest edge turnaround
  for (auto const & targetEdge : target)
    for (auto const & sourceEdge : source)
      if (FindSingleRoute(sourceEdge, targetEdge, facade, rawRoutingResult))
        return true;
  return false;
}

void FindGraphNodeOffsets(uint32_t const nodeId, m2::PointD const & point,
                          Index const * pIndex, TRoutingMappingPtr & mapping,
                          FeatureGraphNode & graphNode)
{
  graphNode.segmentPoint = point;

  Point2PhantomNode::Candidate best;

  auto range = mapping->m_segMapping.GetSegmentsRange(nodeId);
  for (size_t i = range.first; i < range.second; ++i)
  {
    OsrmMappingTypes::FtSeg s;
    mapping->m_segMapping.GetSegmentByIndex(i, s);
    if (!s.IsValid())
      continue;

    FeatureType ft;
    Index::FeaturesLoaderGuard loader(*pIndex, mapping->GetMwmId());
    loader.GetFeatureByIndex(s.m_fid, ft);

    Point2PhantomNode::Candidate mappedSeg;
    Point2PhantomNode::FindNearestSegment(ft, point, mappedSeg);

    OsrmMappingTypes::FtSeg seg;
    seg.m_fid = mappedSeg.m_fid;
    seg.m_pointStart = mappedSeg.m_segIdx;
    seg.m_pointEnd = mappedSeg.m_segIdx + 1;
    if (!s.IsIntersect(seg))
      continue;

    if (mappedSeg.m_dist < best.m_dist)
      best = mappedSeg;
  }

  CHECK_NOT_EQUAL(best.m_fid, kInvalidFid, ());

  graphNode.segment.m_fid = best.m_fid;
  graphNode.segment.m_pointStart = best.m_segIdx;
  graphNode.segment.m_pointEnd = best.m_segIdx + 1;
}

void CalculatePhantomNodeForCross(TRoutingMappingPtr & mapping, FeatureGraphNode & graphNode,
                         Index const * pIndex, bool forward)
{
  if (graphNode.segment.IsValid())
    return;

  uint32_t nodeId;
  if (forward)
    nodeId = graphNode.node.forward_node_id;
  else
    nodeId = graphNode.node.reverse_node_id;

  CHECK_NOT_EQUAL(nodeId, INVALID_NODE_ID, ());

  mapping->LoadCrossContext();
  MappingGuard guard(mapping);
  UNUSED_VALUE(guard);

  m2::PointD point = m2::PointD::Zero();
  if (forward)
  {
    auto inIters = mapping->m_crossContext.GetIngoingIterators();
    for (auto iter = inIters.first; iter != inIters.second; ++iter)
    {
      if (iter->m_nodeId != nodeId)
        continue;
      point = iter->m_point;
      break;
    }
  }
  else
  {
    auto outIters = mapping->m_crossContext.GetOutgoingIterators();
    for (auto iter = outIters.first; iter != outIters.second; ++iter)
    {
      if (iter->m_nodeId != nodeId)
        continue;
      point = iter->m_point;
      break;
    }
  }

  CHECK(!point.IsAlmostZero(), ());

  FindGraphNodeOffsets(nodeId, MercatorBounds::FromLatLon(point.y, point.x),
                       pIndex, mapping, graphNode);
}

// TODO (ldragunov) move this function to cross mwm router
// TODO (ldragunov) process case when the start and the finish points are placed on the same edge.
OsrmRouter::ResultCode OsrmRouter::MakeRouteFromCrossesPath(TCheckedPath const & path,
                                                            RouterDelegate const & delegate,
                                                            Route & route)
{
  Route::TTurns TurnsDir;
  Route::TTimes Times;
  vector<m2::PointD> Points;
  for (RoutePathCross cross : path)
  {
    ASSERT_EQUAL(cross.startNode.mwmName, cross.finalNode.mwmName, ());
    RawRoutingResult routingResult;
    TRoutingMappingPtr mwmMapping = m_indexManager.GetMappingByName(cross.startNode.mwmName);
    ASSERT(mwmMapping->IsValid(), ());
    MappingGuard mwmMappingGuard(mwmMapping);
    UNUSED_VALUE(mwmMappingGuard);
    CalculatePhantomNodeForCross(mwmMapping, cross.startNode, m_pIndex, true /* forward */);
    CalculatePhantomNodeForCross(mwmMapping, cross.finalNode, m_pIndex, false /* forward */);
    if (!FindSingleRoute(cross.startNode, cross.finalNode, mwmMapping->m_dataFacade, routingResult))
      return OsrmRouter::RouteNotFound;

    if (!Points.empty())
    {
      // Remove road end point and turn instruction.
      Points.pop_back();
      TurnsDir.pop_back();
      Times.pop_back();
    }

    // Get annotated route.
    Route::TTurns mwmTurnsDir;
    Route::TTimes mwmTimes;
    vector<m2::PointD> mwmPoints;
    MakeTurnAnnotation(routingResult, mwmMapping, delegate, mwmPoints, mwmTurnsDir, mwmTimes);
    // Connect annotated route.
    auto const pSize = static_cast<uint32_t>(Points.size());
    for (auto turn : mwmTurnsDir)
    {
      if (turn.m_index == 0)
        continue;
      turn.m_index += pSize;
      TurnsDir.push_back(turn);
    }

    double const estimationTime = Times.size() ? Times.back().second : 0.0;
    for (auto time : mwmTimes)
    {
      if (time.first == 0)
        continue;
      time.first += pSize;
      time.second += estimationTime;
      Times.push_back(time);
    }

    Points.insert(Points.end(), mwmPoints.begin(), mwmPoints.end());
  }

  route.SetGeometry(Points.begin(), Points.end());
  route.SetTurnInstructions(TurnsDir);
  route.SetSectionTimes(Times);
  return OsrmRouter::NoError;
}

OsrmRouter::ResultCode OsrmRouter::CalculateRoute(m2::PointD const & startPoint,
                                                  m2::PointD const & startDirection,
                                                  m2::PointD const & finalPoint,
                                                  RouterDelegate const & delegate, Route & route)
{
  my::HighResTimer timer(true);
  m_indexManager.Clear();  // TODO (Dragunov) make proper index manager cleaning

  TRoutingMappingPtr startMapping = m_indexManager.GetMappingByPoint(startPoint);
  TRoutingMappingPtr targetMapping = m_indexManager.GetMappingByPoint(finalPoint);

  if (!startMapping->IsValid())
  {
    ResultCode const code = startMapping->GetError();
    if (code != NoError)
    {
      route.AddAbsentCountry(startMapping->GetCountryName());
      return code;
    }
    return IRouter::StartPointNotFound;
  }
  if (!targetMapping->IsValid())
  {
    ResultCode const code = targetMapping->GetError();
    if (code != NoError)
    {
      route.AddAbsentCountry(targetMapping->GetCountryName());
      return code;
    }
    return IRouter::EndPointNotFound;
  }

  MappingGuard startMappingGuard(startMapping);
  MappingGuard finalMappingGuard(targetMapping);
  UNUSED_VALUE(startMappingGuard);
  UNUSED_VALUE(finalMappingGuard);
  LOG(LINFO, ("Duration of the MWM loading", timer.ElapsedNano()));
  timer.Reset();

  delegate.OnProgress(kMwmLoadedProgress);

  // 3. Find start/end nodes.
  TFeatureGraphNodeVec startTask;

  {
    ResultCode const code = FindPhantomNodes(startPoint, startDirection,
                                             startTask, kMaxNodeCandidatesCount, startMapping);
    if (code != NoError)
      return code;
  }
  {
    if (finalPoint != m_cachedTargetPoint)
    {
      ResultCode const code =
          FindPhantomNodes(finalPoint, m2::PointD::Zero(),
                           m_cachedTargets, kMaxNodeCandidatesCount, targetMapping);
      if (code != NoError)
        return code;
      m_cachedTargetPoint = finalPoint;
    }
  }
  INTERRUPT_WHEN_CANCELLED(delegate);

  LOG(LINFO, ("Duration of the start/stop points lookup", timer.ElapsedNano()));
  timer.Reset();
  delegate.OnProgress(kPointsFoundProgress);

  // 4. Find route.
  RawRoutingResult routingResult;

  // 4.1 Single mwm case
  if (startMapping->GetMwmId() == targetMapping->GetMwmId())
  {
    LOG(LINFO, ("Single mwm routing case"));
    m_indexManager.ForEachMapping([](pair<string, TRoutingMappingPtr> const & indexPair)
                                  {
                                    indexPair.second->FreeCrossContext();
                                  });
    if (!FindRouteFromCases(startTask, m_cachedTargets, startMapping->m_dataFacade,
                            routingResult))
    {
      return RouteNotFound;
    }
    INTERRUPT_WHEN_CANCELLED(delegate);
    delegate.OnProgress(kPathFoundProgress);

    // 5. Restore route.

    Route::TTurns turnsDir;
    Route::TTimes times;
    vector<m2::PointD> points;

    MakeTurnAnnotation(routingResult, startMapping, delegate, points, turnsDir, times);

    route.SetGeometry(points.begin(), points.end());
    route.SetTurnInstructions(turnsDir);
    route.SetSectionTimes(times);

    return NoError;
  }
  else //4.2 Multiple mwm case
  {
    LOG(LINFO, ("Multiple mwm routing case"));
    TCheckedPath finalPath;
    ResultCode code = CalculateCrossMwmPath(startTask, m_cachedTargets, m_indexManager, delegate,
                                            finalPath);
    timer.Reset();
    INTERRUPT_WHEN_CANCELLED(delegate);
    delegate.OnProgress(kCrossPathFoundProgress);

    // 5. Make generate answer
    if (code == NoError)
    {
      auto code = MakeRouteFromCrossesPath(finalPath, delegate, route);
      // Manually free all cross context allocations before geometry unpacking.
      m_indexManager.ForEachMapping([](pair<string, TRoutingMappingPtr> const & indexPair)
                                    {
                                      indexPair.second->FreeCrossContext();
                                    });
      LOG(LINFO, ("Make final route", timer.ElapsedNano()));
      timer.Reset();
      return code;
    }
    return OsrmRouter::RouteNotFound;
  }
}

IRouter::ResultCode OsrmRouter::FindPhantomNodes(m2::PointD const & point,
                                                 m2::PointD const & direction,
                                                 TFeatureGraphNodeVec & res, size_t maxCount,
                                                 TRoutingMappingPtr const & mapping)
{
  ASSERT(mapping, ());
  Point2PhantomNode getter(mapping->m_segMapping, m_pIndex, direction, mapping->m_dataFacade);
  getter.SetPoint(point);

  m_pIndex->ForEachInRectForMWM(getter, MercatorBounds::RectByCenterXYAndSizeInMeters(
                                            point, kFeatureFindingRectSideRadiusMeters),
                                scales::GetUpperScale(), mapping->GetMwmId());

  if (!getter.HasCandidates())
    return RouteNotFound;

  getter.MakeResult(res, maxCount, mapping->GetCountryName());
  return NoError;
}

// @todo(vbykoianko) This method shall to be refactored. It shall be split into several
// methods. All the functionality shall be moved to the turns_generator unit.

// @todo(vbykoianko) For the time being MakeTurnAnnotation generates the turn annotation
// and the route polyline at the same time. It is better to generate it separately
// to be able to use the route without turn annotation.
OsrmRouter::ResultCode OsrmRouter::MakeTurnAnnotation(
    RawRoutingResult const & routingResult, TRoutingMappingPtr const & mapping,
    RouterDelegate const & delegate, vector<m2::PointD> & points, Route::TTurns & turnsDir,
    Route::TTimes & times)
{
  ASSERT(mapping, ());

  typedef OsrmMappingTypes::FtSeg TSeg;
  TSeg const & segBegin = routingResult.sourceEdge.segment;
  TSeg const & segEnd = routingResult.targetEdge.segment;

  double estimatedTime = 0;

  LOG(LDEBUG, ("Shortest path length:", routingResult.shortestPathLength));

  //! @todo: Improve last segment time calculation
  CarModel carModel;
#ifdef DEBUG
  size_t lastIdx = 0;
#endif

  for (auto const & pathSegments : routingResult.unpackedPathSegments)
  {
    INTERRUPT_WHEN_CANCELLED(delegate);

    // Get all computed route coordinates.
    size_t const numSegments = pathSegments.size();
    for (size_t segmentIndex = 0; segmentIndex < numSegments; ++segmentIndex)
    {
      RawPathData const & pathData = pathSegments[segmentIndex];

      if (segmentIndex > 0 && !points.empty())
      {
        turns::TurnItem turnItem;
        turnItem.m_index = static_cast<uint32_t>(points.size() - 1);

        turns::TurnInfo turnInfo(*mapping, pathSegments[segmentIndex - 1].node, pathSegments[segmentIndex].node);
        turns::GetTurnDirection(*m_pIndex, turnInfo, turnItem);

        // ETA information.
        // Osrm multiples seconds to 10, so we need to divide it back.
        double const nodeTimeSeconds = pathData.segmentWeight / 10.0;

#ifdef DEBUG
        double distMeters = 0.0;
        for (size_t k = lastIdx + 1; k < points.size(); ++k)
          distMeters += MercatorBounds::DistanceOnEarth(points[k - 1], points[k]);
        LOG(LDEBUG, ("Speed:", 3.6 * distMeters / nodeTimeSeconds, "kmph; Dist:", distMeters, "Time:",
                     nodeTimeSeconds, "s", lastIdx, "e", points.size(), "source:", turnItem.m_sourceName,
                     "target:", turnItem.m_targetName));
        lastIdx = points.size();
#endif
        estimatedTime += nodeTimeSeconds;
        times.push_back(Route::TTimeItem(points.size(), estimatedTime));

        //  Lane information.
        if (turnItem.m_turn != turns::TurnDirection::NoTurn)
        {
          turnItem.m_lanes = turns::GetLanesInfo(pathSegments[segmentIndex - 1].node,
                                          *mapping, turns::GetLastSegmentPointIndex, *m_pIndex);
          turnsDir.push_back(move(turnItem));
        }
      }

      buffer_vector<TSeg, 8> buffer;
      mapping->m_segMapping.ForEachFtSeg(pathData.node, MakeBackInsertFunctor(buffer));

      auto FindIntersectingSeg = [&buffer] (TSeg const & seg) -> size_t
      {
        ASSERT(seg.IsValid(), ());
        auto const it = find_if(buffer.begin(), buffer.end(), [&seg] (OsrmMappingTypes::FtSeg const & s)
        {
          return s.IsIntersect(seg);
        });

        ASSERT(it != buffer.end(), ());
        return distance(buffer.begin(), it);
      };

      //m_mapping.DumpSegmentByNode(path_data.node);

      //Do not put out node geometry (we do not have it)!
      size_t startK = 0, endK = buffer.size();
      if (segmentIndex == 0)
      {
        if (!segBegin.IsValid())
          continue;
        startK = FindIntersectingSeg(segBegin);
      }
      if (segmentIndex + 1 == numSegments)
      {
        if (!segEnd.IsValid())
          continue;
        endK = FindIntersectingSeg(segEnd) + 1;
      }

      for (size_t k = startK; k < endK; ++k)
      {
        TSeg const & seg = buffer[k];

        FeatureType ft;
        Index::FeaturesLoaderGuard loader(*m_pIndex, mapping->GetMwmId());
        loader.GetFeatureByIndex(seg.m_fid, ft);
        ft.ParseGeometry(FeatureType::BEST_GEOMETRY);

        auto startIdx = seg.m_pointStart;
        auto endIdx = seg.m_pointEnd;
        bool const needTime = (segmentIndex == 0) || (segmentIndex == numSegments - 1);

        if (segmentIndex == 0 && k == startK && segBegin.IsValid())
          startIdx = (seg.m_pointEnd > seg.m_pointStart) ? segBegin.m_pointStart : segBegin.m_pointEnd;
        if (segmentIndex == numSegments - 1 && k == endK - 1 && segEnd.IsValid())
          endIdx = (seg.m_pointEnd > seg.m_pointStart) ? segEnd.m_pointEnd : segEnd.m_pointStart;

        if (seg.m_pointEnd > seg.m_pointStart)
        {
          for (auto idx = startIdx; idx <= endIdx; ++idx)
          {
            points.push_back(ft.GetPoint(idx));
            if (needTime && idx > startIdx)
              estimatedTime += MercatorBounds::DistanceOnEarth(ft.GetPoint(idx - 1), ft.GetPoint(idx)) / carModel.GetSpeed(ft);
          }
        }
        else
        {
          for (auto idx = startIdx; idx > endIdx; --idx)
          {
            if (needTime)
              estimatedTime += MercatorBounds::DistanceOnEarth(ft.GetPoint(idx - 1), ft.GetPoint(idx)) / carModel.GetSpeed(ft);
            points.push_back(ft.GetPoint(idx));
          }
          points.push_back(ft.GetPoint(endIdx));
        }
      }
    }
  }

  if (points.size() < 2)
    return RouteNotFound;

  if (routingResult.sourceEdge.segment.IsValid())
    points.front() = routingResult.sourceEdge.segmentPoint;
  if (routingResult.targetEdge.segment.IsValid())
    points.back() = routingResult.targetEdge.segmentPoint;

  times.push_back(Route::TTimeItem(points.size() - 1, estimatedTime));
  if (routingResult.targetEdge.segment.IsValid())
  {
    turnsDir.emplace_back(
        turns::TurnItem(static_cast<uint32_t>(points.size()) - 1, turns::TurnDirection::ReachedYourDestination));
  }
  turns::FixupTurns(points, turnsDir);

#ifdef DEBUG
  for (auto t : turnsDir)
  {
    LOG(LDEBUG, (turns::GetTurnString(t.m_turn), ":", t.m_index, t.m_sourceName, "-", t.m_targetName, "exit:", t.m_exitNum));
  }

  size_t last = 0;
  double lastTime = 0;
  for (Route::TTimeItem & t : times)
  {
    double dist = 0;
    for (size_t i = last + 1; i <= t.first; ++i)
      dist += MercatorBounds::DistanceOnEarth(points[i - 1], points[i]);

    double time = t.second - lastTime;

    LOG(LDEBUG, ("distance:", dist, "start:", last, "end:", t.first, "Time:", time, "Speed:", 3.6 * dist / time));
    last = t.first;
    lastTime = t.second;
  }
#endif
  LOG(LDEBUG, ("Estimated time:", estimatedTime, "s"));
  return OsrmRouter::NoError;
}
}  // namespace routing
