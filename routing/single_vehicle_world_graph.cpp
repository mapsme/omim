#include "routing/single_vehicle_world_graph.hpp"

#include <algorithm>
#include <utility>

#include "boost/optional.hpp"

namespace routing
{
using namespace std;

template <>
SingleVehicleWorldGraph::AStarParents<Segment>::ParentType
SingleVehicleWorldGraph::AStarParents<Segment>::kEmpty = {};

template <>
SingleVehicleWorldGraph::AStarParents<JointSegment>::ParentType
SingleVehicleWorldGraph::AStarParents<JointSegment>::kEmpty = {};

SingleVehicleWorldGraph::SingleVehicleWorldGraph(unique_ptr<CrossMwmGraph> crossMwmGraph,
                                                 unique_ptr<IndexGraphLoader> loader,
                                                 shared_ptr<EdgeEstimator> estimator)
  : m_crossMwmGraph(move(crossMwmGraph)), m_loader(move(loader)), m_estimator(std::move(estimator))
{
  CHECK(m_loader, ());
  CHECK(m_estimator, ());
}

void SingleVehicleWorldGraph::CheckAndProcessTransitFeatures(Segment const & parent,
                                                             vector<JointEdge> & jointEdges,
                                                             vector<RouteWeight> & parentWeights,
                                                             bool isOutgoing)
{
  bool opposite = !isOutgoing;
  vector<JointEdge> newCrossMwmEdges;
  for (size_t i = 0; i < jointEdges.size(); ++i)
  {
    JointSegment const & target = jointEdges[i].GetTarget();
    if (!m_crossMwmGraph->IsFeatureTransit(target.GetMwmId(), target.GetFeatureId()))
      continue;

    auto & currentIndexGraph = GetIndexGraph(parent.GetMwmId());

    vector<Segment> twins;
    m_crossMwmGraph->GetTwinFeature(target.GetSegment(true /* start */), isOutgoing, twins);
    for (auto const & twin : twins)
    {
      NumMwmId const twinMwmId = twin.GetMwmId();

      uint32_t const twinFeatureId = twin.GetFeatureId();

      Segment const start(twinMwmId, twinFeatureId, target.GetSegmentId(!opposite), target.IsForward());

      auto & twinIndexGraph = GetIndexGraph(twinMwmId);

      vector<uint32_t> lastPoints;
      twinIndexGraph.GetLastPointsForJoint({start}, isOutgoing, lastPoints);
      ASSERT_EQUAL(lastPoints.size(), 1, ());

      if (auto edge = currentIndexGraph.GetJointEdgeByLastPoint(parent,
                                                                target.GetSegment(!opposite),
                                                                isOutgoing, lastPoints.back()))
      {
        newCrossMwmEdges.emplace_back(*edge);
        newCrossMwmEdges.back().GetTarget().GetFeatureId() = twinFeatureId;
        newCrossMwmEdges.back().GetTarget().GetMwmId() = twinMwmId;

        parentWeights.emplace_back(parentWeights[i]);
      }
    }
  }

  jointEdges.insert(jointEdges.end(), newCrossMwmEdges.begin(), newCrossMwmEdges.end());
}

void SingleVehicleWorldGraph::GetEdgeList(JointSegment const & parentJoint,
                                          Segment const & parent, bool isOutgoing,
                                          vector<JointEdge> & jointEdges,
                                          vector<RouteWeight> & parentWeights)
{
  // Fake segments aren't processed here. All work must be done
  // on the IndexGraphStarterJoints abstraction-level.
  if (!parent.IsRealSegment())
    return;

  auto & parents = isOutgoing ? *m_parentsForJoints.forward : *m_parentsForJoints.backward;
  auto & indexGraph = GetIndexGraph(parent.GetMwmId());
  indexGraph.GetEdgeList(parentJoint, parent, isOutgoing, jointEdges, parentWeights, parents);

  if (m_mode != WorldGraphMode::JointSingleMwm)
    CheckAndProcessTransitFeatures(parent, jointEdges, parentWeights, isOutgoing);
}

void SingleVehicleWorldGraph::GetEdgeList(Segment const & segment, bool isOutgoing,
                                          vector<SegmentEdge> & edges)
{
  if (m_mode == WorldGraphMode::LeapsOnly)
  {
    CHECK(m_crossMwmGraph, ());
    // Ingoing edges listing is not supported for leaps because we do not have enough information
    // to calculate |segment| weight. See https://jira.mail.ru/browse/MAPSME-5743 for details.
    CHECK(isOutgoing, ("Ingoing edges listing is not supported for LeapsOnly mode."));
    if (m_crossMwmGraph->IsTransition(segment, isOutgoing))
      GetTwins(segment, isOutgoing, edges);
    else
      m_crossMwmGraph->GetOutgoingEdgeList(segment, edges);

    return;
  }

  auto & parents = isOutgoing ? *m_parentsForSegments.forward : *m_parentsForSegments.backward;
  IndexGraph & indexGraph = m_loader->GetIndexGraph(segment.GetMwmId());
  indexGraph.GetEdgeList(segment, isOutgoing, edges, parents);

  if (m_mode != WorldGraphMode::SingleMwm && m_crossMwmGraph && m_crossMwmGraph->IsTransition(segment, isOutgoing))
    GetTwins(segment, isOutgoing, edges);
}

Junction const & SingleVehicleWorldGraph::GetJunction(Segment const & segment, bool front)
{
  return GetRoadGeometry(segment.GetMwmId(), segment.GetFeatureId())
         .GetJunction(segment.GetPointId(front));
}

m2::PointD const & SingleVehicleWorldGraph::GetPoint(Segment const & segment, bool front)
{
  return GetJunction(segment, front).GetPoint();
}

bool SingleVehicleWorldGraph::IsOneWay(NumMwmId mwmId, uint32_t featureId)
{
  return GetRoadGeometry(mwmId, featureId).IsOneWay();
}

bool SingleVehicleWorldGraph::IsPassThroughAllowed(NumMwmId mwmId, uint32_t featureId)
{
  return GetRoadGeometry(mwmId, featureId).IsPassThroughAllowed();
}

void SingleVehicleWorldGraph::GetOutgoingEdgesList(Segment const & segment,
                                                   vector<SegmentEdge> & edges)
{
  edges.clear();
  GetEdgeList(segment, true /* isOutgoing */, edges);
}

void SingleVehicleWorldGraph::GetIngoingEdgesList(Segment const & segment,
                                                  vector<SegmentEdge> & edges)
{
  edges.clear();
  GetEdgeList(segment, false /* isOutgoing */, edges);
}

RouteWeight SingleVehicleWorldGraph::HeuristicCostEstimate(Segment const & from, Segment const & to)
{
  return HeuristicCostEstimate(GetPoint(from, true /* front */), GetPoint(to, true /* front */));
}


RouteWeight SingleVehicleWorldGraph::HeuristicCostEstimate(Segment const & from, m2::PointD const & to)
{
  return HeuristicCostEstimate(GetPoint(from, true /* front */), to);
}

RouteWeight SingleVehicleWorldGraph::HeuristicCostEstimate(m2::PointD const & from,
                                                           m2::PointD const & to)
{
  return RouteWeight(m_estimator->CalcHeuristic(from, to));
}

RouteWeight SingleVehicleWorldGraph::CalcSegmentWeight(Segment const & segment)
{
  return RouteWeight(m_estimator->CalcSegmentWeight(
      segment, GetRoadGeometry(segment.GetMwmId(), segment.GetFeatureId())));
}

RouteWeight SingleVehicleWorldGraph::CalcLeapWeight(m2::PointD const & from,
                                                    m2::PointD const & to) const
{
  return RouteWeight(m_estimator->CalcLeapWeight(from, to));
}

RouteWeight SingleVehicleWorldGraph::CalcOffroadWeight(m2::PointD const & from,
                                                       m2::PointD const & to) const
{
  return RouteWeight(m_estimator->CalcOffroadWeight(from, to));
}

double SingleVehicleWorldGraph::CalcSegmentETA(Segment const & segment)
{
  return m_estimator->CalcSegmentETA(segment, GetRoadGeometry(segment.GetMwmId(), segment.GetFeatureId()));
}

vector<Segment> const & SingleVehicleWorldGraph::GetTransitions(NumMwmId numMwmId, bool isEnter)
{
  return m_crossMwmGraph->GetTransitions(numMwmId, isEnter);
}

unique_ptr<TransitInfo> SingleVehicleWorldGraph::GetTransitInfo(Segment const &) { return {}; }

vector<RouteSegment::SpeedCamera> SingleVehicleWorldGraph::GetSpeedCamInfo(Segment const & segment)
{
  ASSERT(segment.IsRealSegment(), ());
  return m_loader->GetSpeedCameraInfo(segment);
}

RoadGeometry const & SingleVehicleWorldGraph::GetRoadGeometry(NumMwmId mwmId, uint32_t featureId)
{
  return m_loader->GetGeometry(mwmId).GetRoad(featureId);
}

void SingleVehicleWorldGraph::GetTwinsInner(Segment const & segment, bool isOutgoing,
                                            vector<Segment> & twins)
{
  m_crossMwmGraph->GetTwins(segment, isOutgoing, twins);
}

bool SingleVehicleWorldGraph::IsRoutingOptionsGood(Segment const & segment)
{
  auto const & geometry = GetRoadGeometry(segment.GetMwmId(), segment.GetFeatureId());
  return geometry.SuitableForOptions(m_avoidRoutingOptions);
}

RoutingOptions SingleVehicleWorldGraph::GetRoutingOptions(Segment const & segment)
{
  ASSERT(segment.IsRealSegment(), ());

  auto const & geometry = GetRoadGeometry(segment.GetMwmId(), segment.GetFeatureId());
  return geometry.GetRoutingOptions();
}

void SingleVehicleWorldGraph::SetAStarParents(bool forward, std::map<Segment, Segment> & parents)
{
  if (forward)
    m_parentsForSegments.forward = &parents;
  else
    m_parentsForSegments.backward = &parents;
}

void SingleVehicleWorldGraph::SetAStarParents(bool forward, std::map<JointSegment, JointSegment> & parents)
{
  if (forward)
    m_parentsForJoints.forward = &parents;
  else
    m_parentsForJoints.backward = &parents;
}

template <typename VertexType>
bool SingleVehicleWorldGraph::IsWavesConnectibleImpl(std::map<VertexType, VertexType> & forwardParents,
                                                     VertexType const & commonVertex,
                                                     std::map<VertexType, VertexType> & backwardParents,
                                                     std::function<uint32_t(VertexType const &)> && fakeFeatureConverter)
{
//  LOG(LINFO, ("commonVertex =", commonVertex));
  std::vector<VertexType> chain;
  NumMwmId mwmId = kFakeNumMwmId;
  auto const fillUntilNextFeatureId = [&](VertexType const & cur, std::map<VertexType, VertexType> & parents)
  {
    if (cur.IsRealSegment())
      mwmId = cur.GetMwmId();

    auto startFeatureId = cur.GetFeatureId();
    auto it = parents.find(cur);
    while (it != parents.end() && it->second.GetFeatureId() == startFeatureId)
    {
      chain.emplace_back(it->second);
      it = parents.find(it->second);
    }

    if (it == parents.end())
      return false;

    chain.emplace_back(it->second);
    return true;
  };

  auto const fillParents = [&](VertexType const & start, std::map<VertexType, VertexType> & parents)
  {
    VertexType current = start;
    static uint32_t constexpr kStepCountOneSide = 5;
    for (uint32_t i = 0; i < kStepCountOneSide; ++i)
    {
      if (!fillUntilNextFeatureId(current, parents))
        break;

      current = chain.back();
    }
  };

  fillParents(commonVertex, forwardParents);

  std::reverse(chain.begin(), chain.end());
  chain.emplace_back(commonVertex);

  fillParents(commonVertex, backwardParents);

  if (mwmId == kFakeNumMwmId)
    return true;

//  LOG(LINFO, ("CHAIN START"));
//  for (auto const & v : chain)
//  {
//    if (v.IsRealSegment())
//      LOG(LINFO, (v, MercatorBounds::ToLatLon(GetPoint(v.GetSegment(true), true))));
//  }

  if (fakeFeatureConverter)
  {
    for (size_t i = 0; i < chain.size(); ++i)
    {
      if (!chain[i].IsRealSegment())
        chain[i].GetFeatureId() = fakeFeatureConverter(chain[i]);
    }
  }

//  LOG(LINFO, ("AFTER CONVERT"));
//  for (auto const & v : chain)
//    LOG(LINFO, (v));
//  LOG(LINFO, ("CHAIN END"));

  std::map<VertexType, VertexType> parents;
  for (size_t i = 1; i < chain.size(); ++i)
    parents[chain[i]] = chain[i - 1];

  auto & currentIndexGraph = GetIndexGraph(mwmId);
  for (size_t i = 1; i < chain.size(); ++i)
  {
    auto const & parent = chain[i - 1];
    uint32_t parentFeatureId = chain[i - 1].GetFeatureId();
    uint32_t currentFeatureId = chain[i].GetFeatureId();

    if (parentFeatureId != currentFeatureId &&
        currentIndexGraph.IsRestricted(parent, parentFeatureId, currentFeatureId, true /* isOutgoing */,
                                       parents))
    {
//      LOG(LINFO, ("Can not go though:"));
//      for (size_t j = 0; j <= i; ++j)
//        LOG(LINFO, (chain[j]));
//      LOG(LINFO, ("================================"));
      return false;
    }
  }

  LOG(LINFO, ("ALL OK"));
  return true;
}

bool SingleVehicleWorldGraph::IsWavesConnectible(ParentSegments & forwardParents,
                                                 Segment const & commonVertex,
                                                 ParentSegments & backwardParents,
                                                 std::function<uint32_t(Segment const &)> && fakeFeatureConverter)
{
  return IsWavesConnectibleImpl(forwardParents, commonVertex, backwardParents, std::move(fakeFeatureConverter));
}

bool SingleVehicleWorldGraph::IsWavesConnectible(ParentJoints & forwardParents,
                                                 JointSegment const & commonVertex,
                                                 ParentJoints & backwardParents,
                                                 std::function<uint32_t(JointSegment const &)> && fakeFeatureConverter)
{
  return IsWavesConnectibleImpl(forwardParents, commonVertex, backwardParents, std::move(fakeFeatureConverter));
}
}  // namespace routing
