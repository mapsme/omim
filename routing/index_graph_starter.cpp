#include "routing/index_graph_starter.hpp"

#include "routing/routing_exceptions.hpp"

#include "std/sstream.hpp"

namespace routing
{
namespace
{
int Compare(uint32_t a, uint32_t b) noexcept
{
  if (a < b)
    return -1;
  if (a > b)
    return 1;
  return 0;
}
}  // namespace

// IndexGraphStarter::TVertexType ------------------------------------------------------------------
IndexGraphStarter::TVertexType::TVertexType(Joint::Id prev, Joint::Id curr, double weight) noexcept
  : m_prev(prev), m_curr(curr), m_weight(weight)
{
}

bool IndexGraphStarter::TVertexType::operator==(TVertexType const & rhs) const noexcept
{
  return m_prev == rhs.m_prev && m_curr == rhs.m_curr && m_weight == rhs.m_weight;
}

bool IndexGraphStarter::TVertexType::operator<(TVertexType const & rhs) const noexcept
{
  if (m_curr != rhs.m_curr)
    return m_curr < rhs.m_curr;
  if (m_prev != rhs.m_prev)
    return m_prev < rhs.m_prev;
  return m_weight < rhs.m_weight;
}

string DebugPrint(IndexGraphStarter::TVertexType const & u)
{
  ostringstream os;
  os << "Vertex [ prev:" << u.GetPrev() << ", curr:" << u.GetCurr() << " , weight:" << u.GetWeight()
     << " ]";
  return os.str();
}

// IndexGraphStarter -------------------------------------------------------------------------------
IndexGraphStarter::IndexGraphStarter(IndexGraph & graph, RoadPoint const & startPoint,
                                     RoadPoint const & finishPoint)
  : m_fakeNextFeatureId(graph.GetNextFakeFeatureId())
  , m_graph(graph)
  , m_start(startPoint, graph.GetNumJoints())
  , m_finish(finishPoint, graph.GetNumJoints() + 1)
{
  CHECK(!IndexGraph::IsFakeFeature(startPoint.GetFeatureId()), ());
  CHECK(!IndexGraph::IsFakeFeature(finishPoint.GetFeatureId()), ());

  m_start.SetupJointId(graph);

  if (startPoint == finishPoint)
    m_finish.m_jointId = m_start.m_jointId;
  else
    m_finish.SetupJointId(graph);

  // After adding restrictions on IndexGraph some edges could be blocked some other could
  // be copied. It's possible that start or finish could be matched on a new fake (or blocked)
  // edge that may spoil route geometry or even worth prevent from building some routes.
  // To overcome it some edge from start and to finish should be added below.
  AddFakeZeroLengthEdges(m_start, EndPointType::Start);
  AddFakeZeroLengthEdges(m_finish, EndPointType::Finish);
}

m2::PointD const & IndexGraphStarter::GetPoint(Joint::Id jointId)
{
  if (jointId == m_start.m_fakeId)
    return m_graph.GetPoint(m_start.m_point);

  if (jointId == m_finish.m_fakeId)
    return m_graph.GetPoint(m_finish.m_point);

  return m_graph.GetPoint(jointId);
}

DirectedEdge const & IndexGraphStarter::FindFakeEdge(uint32_t fakeFeatureId)
{
  ASSERT(IsFakeFeature(fakeFeatureId), ("Feature", fakeFeatureId, "is not a fake one"));
  for (DirectedEdge const & e : m_fakeZeroLengthEdges)
  {
    ASSERT_EQUAL(GetPoint(e.GetFrom()), GetPoint(e.GetTo()), ());
    if (e.GetFeatureId() == fakeFeatureId)
      return e;
  }
  CHECK(false, ("Fake feature:", fakeFeatureId, "has to be contained in |m_fakeZeroLengthEdges|"));
  return m_fakeZeroLengthEdges.front();
}

m2::PointD const & IndexGraphStarter::GetPoint(RoadPoint const & rp)
{
  if (!IsFakeFeature(rp.GetFeatureId()))
    return m_graph.GetPoint(rp);

  // Fake edges have zero length so coords of "point from" and "point to" are equal.
  return GetPoint(FindFakeEdge(rp.GetFeatureId()).GetFrom());
}

void IndexGraphStarter::GetOutgoingEdgesList(TVertexType const & u, vector<TEdgeType> & edges)
{
  vector<JointEdge> jes;
  GetEdgesList(u.GetCurr(), true /* isOutgoing */, jes);

  edges.clear();
  edges.reserve(jes.size());
  for (auto const & je : jes)
  {
    TVertexType const v(u.GetCurr(), je.GetTarget(), je.GetWeight());
    double const weight = v.GetWeight() + GetPenalties(u, v);
    edges.emplace_back(v, weight);
  }

  if (u.GetCurr() == GetFinishJoint())
    edges.emplace_back(GetFinishVertex(), 0 /* weight */);
}

void IndexGraphStarter::GetIngoingEdgesList(TVertexType const & u, vector<TEdgeType> & edges)
{
  vector<JointEdge> jes;
  GetEdgesList(u.GetPrev(), false /* isOutgoing */, jes);

  edges.clear();
  edges.reserve(jes.size());
  for (auto const & je : jes)
  {
    TVertexType const v(je.GetTarget(), u.GetPrev(), je.GetWeight());
    double const weight = u.GetWeight() + GetPenalties(v, u);
    edges.emplace_back(v, weight);
  }

  if (u.GetPrev() == GetStartJoint())
    edges.emplace_back(GetStartVertex(), u.GetWeight());
}

uint32_t IndexGraphStarter::GetOriginalFeature(TVertexType const & vertex)
{
  RoadPoint rp0;
  RoadPoint rp1;
  FindPointsWithCommonFeature(vertex.GetPrev(), vertex.GetCurr(), rp0, rp1);
  return rp0.GetFeatureId();
}

pair<RoadPoint, RoadPoint> IndexGraphStarter::GetOriginalEndPoints(TVertexType const & vertex)
{
  DirectedEdge const e(vertex.GetPrev(), vertex.GetCurr(), GetOriginalFeature(vertex));
  DirectedEdge const p = m_graph.GetParent(e);

  RoadPoint rp0;
  RoadPoint rp1;
  FindPointsWithCommonFeature(p.GetFrom(), p.GetTo(), rp0, rp1);
  return make_pair(rp0, rp1);
}

RoadGeometry IndexGraphStarter::GetFakeRoadGeometry(uint32_t fakeFeatureId)
{
  DirectedEdge const & e = FindFakeEdge(fakeFeatureId);
  ASSERT_EQUAL(GetPoint(e.GetFrom()), GetPoint(e.GetTo()), ());
  // Note. |e| is a zero length edge so speed could be any number which is not equal to zero.
  return RoadGeometry(true /* one way */, 1.0 /* speed */,
      RoadGeometry::Points({GetPoint(e.GetFrom()), GetPoint(e.GetTo())}));
}

void IndexGraphStarter::RedressRoute(vector<Joint::Id> const & route,
                                     vector<RoutePoint> & routePoints)
{
  if (route.size() < 2)
  {
    if (route.size() == 1)
      routePoints.emplace_back(m_start.m_point, 0.0 /* time */);
    return;
  }

  routePoints.reserve(route.size() * 2);

  EdgeEstimator const & estimator = m_graph.GetEstimator();

  double routeTime = 0.0;
  for (size_t i = 0; i < route.size() - 1; ++i)
  {
    Joint::Id const prevJoint = route[i];
    Joint::Id const nextJoint = route[i + 1];

    RoadPoint rp0;
    RoadPoint rp1;
    FindPointsWithCommonFeature(prevJoint, nextJoint, rp0, rp1);
    if (i == 0)
      routePoints.emplace_back(rp0, 0.0 /* time */);

    uint32_t const featureId = rp0.GetFeatureId();
    uint32_t const pointFrom = rp0.GetPointId();
    uint32_t const pointTo = rp1.GetPointId();

    RoadGeometry const roadGeometry = IsFakeFeature(featureId) ? GetFakeRoadGeometry(featureId)
                                                               : m_graph.GetRoad(featureId);

    uint32_t const step = pointFrom < pointTo ? 1 : -1;

    for (uint32_t prevPointId = pointFrom; prevPointId != pointTo; prevPointId += step)
    {
      uint32_t const pointId = prevPointId + step;
      routeTime += estimator.CalcEdgesWeight(featureId, roadGeometry, prevPointId, pointId);
      routePoints.emplace_back(featureId, pointId, routeTime);
    }
  }
}

void IndexGraphStarter::AddZeroLengthOnewayFeature(Joint::Id from, Joint::Id to)
{
  if (from == to)
    return;

  m_jointsOfFakeEdges.insert(from);
  m_jointsOfFakeEdges.insert(to);
  m_fakeZeroLengthEdges.emplace_back(from, to, m_fakeNextFeatureId++);
}

void IndexGraphStarter::GetFakeEdgesList(Joint::Id jointId, bool isOutgoing,
                                         vector<JointEdge> & edges)
{
  if (m_jointsOfFakeEdges.count(jointId) == 0)
    return;

  for (DirectedEdge const e : m_fakeZeroLengthEdges)
  {
    ASSERT_EQUAL(GetPoint(e.GetFrom()), GetPoint(e.GetTo()), ());
    if (isOutgoing)
    {
      if (e.GetFrom() == jointId)
        edges.emplace_back(e.GetTo(), 0 /* weight */);
    }
    else
    {
      if (e.GetTo() == jointId)
        edges.emplace_back(e.GetFrom(), 0 /* weight */);
    }
  }
}

void IndexGraphStarter::GetEdgesList(Joint::Id jointId, bool isOutgoing, vector<JointEdge> & edges)
{
  edges.clear();

  // Note. Fake edges adding here may be ingoing or outgoing edges for start or finish.
  // Or it may be "arrival edges". That mean |jointId| is not start of finish but
  // a node which is connected with start of finish by an edge.
  GetFakeEdgesList(jointId, isOutgoing, edges);

  if (jointId == m_start.m_fakeId)
  {
    GetStartFinishEdges(m_start, m_finish, isOutgoing, edges);
    return;
  }

  if (jointId == m_finish.m_fakeId)
  {
    GetStartFinishEdges(m_finish, m_start, isOutgoing, edges);
    return;
  }

  m_graph.GetEdgeList(jointId, isOutgoing, false /* graphWithoutRestrictions */, edges);
  GetArrivalFakeEdges(jointId, m_start, isOutgoing, edges);
  GetArrivalFakeEdges(jointId, m_finish, isOutgoing, edges);
}

void IndexGraphStarter::GetStartFinishEdges(IndexGraphStarter::FakeJoint const & from,
                                            IndexGraphStarter::FakeJoint const & to,
                                            bool isOutgoing, vector<JointEdge> & edges)
{
  ASSERT(!from.BelongsToGraph(), ());
  m_graph.GetNeighboringEdges(from.m_point, isOutgoing, false /* graphWithoutRestrictions */,
                              edges);

  if (!to.BelongsToGraph() && from.m_point.GetFeatureId() == to.m_point.GetFeatureId())
  {
    m_graph.GetDirectedEdge(from.m_point.GetFeatureId(), from.m_point.GetPointId(),
                            to.m_point.GetPointId(), to.m_jointId, isOutgoing, edges);
  }
}

void IndexGraphStarter::GetArrivalFakeEdges(Joint::Id jointId,
                                            IndexGraphStarter::FakeJoint const & fakeJoint,
                                            bool isOutgoing, vector<JointEdge> & edges)
{
  if (fakeJoint.BelongsToGraph())
    return;

  if (!m_graph.JointLiesOnRoad(jointId, fakeJoint.m_point.GetFeatureId()))
    return;

  vector<JointEdge> startEdges;
  m_graph.GetNeighboringEdges(fakeJoint.m_point, !isOutgoing, false /* graphWithoutRestrictions */,
                              startEdges);
  for (JointEdge const & edge : startEdges)
  {
    if (edge.GetTarget() == jointId)
      edges.emplace_back(fakeJoint.m_jointId, edge.GetWeight());
  }
}

void IndexGraphStarter::FindPointsWithCommonFeature(Joint::Id jointId0, Joint::Id jointId1,
                                                    RoadPoint & result0, RoadPoint & result1)
{
  bool found = false;
  double minWeight = -1.0;

  auto const foundFn = [&](RoadPoint const & rp0, RoadPoint const & rp1) {
    result0 = rp0;
    result1 = rp1;
    found = true;
  };

  // |m_graph| edges.
  ForEachPoint(jointId0, [&](RoadPoint const & rp0) {
    ForEachPoint(jointId1, [&](RoadPoint const & rp1) {
      if (rp0.GetFeatureId() != rp1.GetFeatureId())
        return;

      if (IsFakeFeature(rp0.GetFeatureId()))
      {
        // Fake edge has alway two points. They are oneway and have zero wight.
        if (rp0.GetPointId() == 0 && rp1.GetPointId() == 1)
        {
          foundFn(rp0, rp1);
          minWeight = 0;
        }
        return;
      }

      RoadGeometry const & road = m_graph.GetRoad(rp0.GetFeatureId());
      if (road.IsOneWay() && rp0.GetPointId() > rp1.GetPointId())
        return;

      if (found)
      {
        if (minWeight < 0.0)
        {
          // CalcEdgesWeight is very expensive.
          // So calculate it only if second common feature found.
          RoadGeometry const & prevRoad = m_graph.GetRoad(result0.GetFeatureId());
          minWeight = m_graph.GetEstimator().CalcEdgesWeight(
              rp0.GetFeatureId(), prevRoad, result0.GetPointId(), result1.GetPointId());
        }

        double const weight = m_graph.GetEstimator().CalcEdgesWeight(
            rp0.GetFeatureId(), road, rp0.GetPointId(), rp1.GetPointId());
        if (weight < minWeight)
        {
          minWeight = weight;
          result0 = rp0;
          result1 = rp1;
        }
      }
      else
      {
        foundFn(rp0, rp1);
      }
    });
  });

  CHECK(found, ("Can't find common feature for joints", jointId0, jointId1));
}

void IndexGraphStarter::AddFakeZeroLengthEdges(FakeJoint const & fj, EndPointType endPointType)
{
  CHECK(!IndexGraph::IsFakeFeature(fj.m_point.GetFeatureId()), ());

  bool const isJoint = fj.BelongsToGraph();

  vector<DirectedEdge> edges;
  if (isJoint)
  {
    m_graph.GetEdgeList(fj.m_jointId, endPointType == EndPointType::Start /* is outgoing */,
                        true /* graphWithoutRestrictions */, edges);
  }
  else
  {
    m_graph.GetIntermediatePointEdges(fj.m_point, true /* graphWithoutRestrictions */, edges);
  }

  for (DirectedEdge const & edge : edges)
  {
    m_graph.ForEachEdgeMappingNode(edge, [&](DirectedEdge const & e) {
      if (edge == e)
        return;

      switch (endPointType)
      {
      case EndPointType::Start:
        AddZeroLengthOnewayFeature(isJoint ? edge.GetFrom() : edge.GetTo(),
                                   isJoint ? e.GetFrom() : e.GetTo());
        return;
      case EndPointType::Finish:
        AddZeroLengthOnewayFeature(isJoint ? e.GetTo() : e.GetFrom(),
                                   isJoint ? edge.GetTo() : edge.GetFrom());
        return;
      }
    });
  }
}

bool IndexGraphStarter::IsUTurn(TVertexType const & u, TVertexType const & v)
{
  if (u.GetCurr() != v.GetPrev())
    return false;

  auto const pu = GetOriginalEndPoints(u);
  auto const pv = GetOriginalEndPoints(v);
  if (pu.first.GetFeatureId() != pv.first.GetFeatureId())
    return false;

  auto const su = Compare(pu.first.GetPointId(), pu.second.GetPointId());
  auto const sv = Compare(pv.first.GetPointId(), pv.second.GetPointId());
  return su != 0 && sv != 0 && su != sv;
}

double IndexGraphStarter::GetPenalties(TVertexType const & u, TVertexType const & v)
{
  if (u == GetStartVertex() || u == GetFinishVertex())
    return 0;
  if (v == GetStartVertex() || v == GetFinishVertex())
    return 0;

  double penalty = 0;
  if (IsUTurn(u, v))
    penalty += m_graph.GetEstimator().GetUTurnPenalty();
  return penalty;
}

// IndexGraphStarter::FakeJoint --------------------------------------------------------------------
IndexGraphStarter::FakeJoint::FakeJoint(RoadPoint const & point, Joint::Id fakeId)
  : m_point(point), m_fakeId(fakeId), m_jointId(Joint::kInvalidId)
{
}

void IndexGraphStarter::FakeJoint::SetupJointId(IndexGraph const & graph)
{
  m_jointId = graph.GetJointId(m_point);
  if (m_jointId == Joint::kInvalidId)
    m_jointId = m_fakeId;
}
}  // namespace routing
