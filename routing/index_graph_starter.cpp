#include "routing/index_graph_starter.hpp"

#include "routing/routing_exceptions.hpp"

namespace routing
{
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

m2::PointD const & IndexGraphStarter::GetPoint(RoadPoint const & rp)
{
  if (!IsFakeFeature(rp.GetFeatureId()))
    return m_graph.GetPoint(rp);

  for (DirectedEdge const & e : m_fakeZeroLengthEdges)
  {
    ASSERT_EQUAL(GetPoint(e.GetFrom()), GetPoint(e.GetTo()), ());
    if (e.GetFeatureId() == rp.GetFeatureId())
      return GetPoint(e.GetFrom());
  }
  CHECK(false,
        (rp, "is a point of a fake feature but it's not contained in |m_fakeZeroLengthEdges|"));
  return m_graph.GetPoint(rp);
}

void IndexGraphStarter::RedressRoute(vector<Joint::Id> const & route,
                                     vector<RoadPoint> & roadPoints)
{
  if (route.size() < 2)
  {
    if (route.size() == 1)
      roadPoints.emplace_back(m_start.m_point);
    return;
  }

  roadPoints.reserve(route.size() * 2);

  for (size_t i = 0; i < route.size() - 1; ++i)
  {
    Joint::Id const prevJoint = route[i];
    Joint::Id const nextJoint = route[i + 1];

    RoadPoint rp0;
    RoadPoint rp1;
    FindPointsWithCommonFeature(prevJoint, nextJoint, rp0, rp1);
    if (i == 0)
      roadPoints.emplace_back(rp0);

    uint32_t const featureId = rp0.GetFeatureId();
    uint32_t const pointFrom = rp0.GetPointId();
    uint32_t const pointTo = rp1.GetPointId();

    if (pointFrom < pointTo)
    {
      for (uint32_t pointId = pointFrom + 1; pointId < pointTo; ++pointId)
        roadPoints.emplace_back(featureId, pointId);
    }
    else if (pointFrom > pointTo)
    {
      for (uint32_t pointId = pointFrom - 1; pointId > pointTo; --pointId)
        roadPoints.emplace_back(featureId, pointId);
    }
    else
    {
      CHECK(false,
            ("Wrong equality pointFrom = pointTo =", pointFrom, ", featureId = ", featureId));
    }

    roadPoints.emplace_back(rp1);
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
