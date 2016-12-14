#pragma once

#include "routing/index_graph.hpp"
#include "routing/joint.hpp"
#include "routing/route_point.hpp"

#include "std/set.hpp"
#include "std/utility.hpp"
#include "std/vector.hpp"

namespace routing_test
{
struct RestrictionTest;
}  // namespace routing_test

namespace routing
{
// The problem:
// IndexGraph contains only road points connected in joints.
// So it is possible IndexGraph doesn't contain start and finish.
//
// IndexGraphStarter adds fake start and finish joint ids for AStarAlgorithm.
//
class IndexGraphStarter final
{
public:
  // AStarAlgorithm types aliases:
  using TVertexType = Joint::Id;
  using TEdgeType = JointEdge;

  enum class EndPointType
  {
    Start,
    Finish
  };

  /// \note |startPoint| and |finishPoint| should be points of real features.
  IndexGraphStarter(IndexGraph & graph, RoadPoint const & startPoint,
                    RoadPoint const & finishPoint);

  IndexGraph & GetGraph() const { return m_graph; }
  Joint::Id GetStartJoint() const { return m_start.m_jointId; }
  Joint::Id GetFinishJoint() const { return m_finish.m_jointId; }

  m2::PointD const & GetPoint(Joint::Id jointId);
  m2::PointD const & GetPoint(RoadPoint const & rp);

  void GetOutgoingEdgesList(Joint::Id jointId, vector<JointEdge> & edges)
  {
    GetEdgesList(jointId, true /* isOutgoing */, edges);
  }

  void GetIngoingEdgesList(Joint::Id jointId, vector<JointEdge> & edges)
  {
    GetEdgesList(jointId, false /* isOutgoing */, edges);
  }

  double HeuristicCostEstimate(Joint::Id from, Joint::Id to)
  {
    return m_graph.GetEstimator().CalcHeuristic(GetPoint(from), GetPoint(to));
  }

  // Add intermediate points to route (those don't correspond to any joint).
  //
  // Also convert joint ids to RoadPoints.
  void RedressRoute(vector<Joint::Id> const & route, vector<RoutePoint> & routePoints);

private:
  friend struct routing_test::RestrictionTest;

  struct FakeJoint final
  {
    FakeJoint(RoadPoint const & point, Joint::Id fakeId);

    void SetupJointId(IndexGraph const & graph);
    bool BelongsToGraph() const { return m_jointId != m_fakeId; }

    RoadPoint const m_point;
    Joint::Id const m_fakeId;
    Joint::Id m_jointId;
  };

  template <typename F>
  void ForEachPoint(Joint::Id jointId, F && f) const
  {
    if (jointId == m_start.m_fakeId)
    {
      f(m_start.m_point);
      return;
    }

    if (jointId == m_finish.m_fakeId)
    {
      f(m_finish.m_point);
      return;
    }

    // |jointId| is not a fake start or a fake finish. So it's a real joint.
    if (m_jointsOfFakeEdges.count(jointId) != 0)
    {
      for (DirectedEdge const & e : m_fakeZeroLengthEdges)
      {
        if (jointId == e.GetFrom())
          f(RoadPoint(e.GetFeatureId(), 0 /* point id */));
        if (jointId == e.GetTo())
          f(RoadPoint(e.GetFeatureId(), 1 /* point id */));
      }
    }

    m_graph.ForEachPoint(jointId, forward<F>(f));
  }

  bool IsFakeFeature(uint32_t featureId) const
  {
    return featureId >= m_graph.GetNextFakeFeatureId();
  }

  void AddZeroLengthOnewayFeature(Joint::Id from, Joint::Id to);
  void GetFakeEdgesList(Joint::Id jointId, bool isOutgoing, vector<JointEdge> & edges);

  void GetEdgesList(Joint::Id jointId, bool isOutgoing, vector<JointEdge> & edges);
  void GetStartFinishEdges(FakeJoint const & from, FakeJoint const & to, bool isOutgoing,
                           vector<JointEdge> & edges);
  void GetArrivalFakeEdges(Joint::Id jointId, FakeJoint const & fakeJoint, bool isOutgoing,
                           vector<JointEdge> & edges);

  // Find two road points lying on the same feature.
  // If there are several pairs of such points, return pair with minimal distance.
  void FindPointsWithCommonFeature(Joint::Id jointId0, Joint::Id jointId1, RoadPoint & result0,
                                   RoadPoint & result1);

  void AddFakeZeroLengthEdges(FakeJoint const & fp, EndPointType endPointType);

  // Edges which added to IndexGraph in IndexGraphStarter. Size of |m_fakeZeroLengthEdges| should be
  // relevantly small because some methods working with it have linear complexity.
  vector<DirectedEdge> m_fakeZeroLengthEdges;
  // |m_jointsOfFakeEdges| is set of all joint ids take part in |m_fakeZeroLengthEdges|.
  set<Joint::Id> m_jointsOfFakeEdges;
  uint32_t m_fakeNextFeatureId;
  IndexGraph & m_graph;
  FakeJoint m_start;
  FakeJoint m_finish;
};
}  // namespace routing
