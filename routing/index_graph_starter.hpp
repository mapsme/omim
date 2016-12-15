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
  // IndexGraph is a G = (V, E), where V is a Joint, and E is a pair
  // of joints. But this space is too restrictive, as it's not
  // possible to add penalties for left turns, U-turns, restrictions
  // and so on. Therefore, we run A* algorithm on a different graph G'
  // = (V', E'), where each vertex from V' is a pair of vertices
  // (previous vertex, current vertex) from V. Following laws hold:
  //
  // 1. (s, s) in V', where s is a start joint.
  //
  // 2. (t, t) in V', where t is a finish joint.
  //
  // 3. If there is an edge (u, v) in E and edge (v, w) in E, then
  // there is an edge from (u, v) to (v, w) in E', and the weight of
  // this edge is the same as the weight of (v, w) from E.
  //
  // 4. If there is an edge (s, u) in E then there is an edge from (s,
  // s) to (s, u) in E' and the weight of this edge is the same as the
  // weigth of (s, u) from E.
  //
  // 5. If there is an edge (u, t) in E then there is an edge from (u,
  // t) to (t, t) in E' of zero length.
  //
  // 6. (s, s) from V' and (t, t) from V' are start and finish
  // vertices correspondingly.
  //
  // As one can see, vertices in the new space are actually edges from
  // the original space.
  using TVertexType = pair<Joint::Id, Joint::Id>;

  struct TEdgeType
  {
  public:
    TEdgeType(TVertexType const & target, double weight) : m_target(target), m_weight(weight) {}

    TVertexType GetTarget() const { return m_target; }
    double GetWeight() const { return m_weight; }

  private:
    TVertexType m_target;
    double m_weight;
  };

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

  TVertexType GetStartVertex() const
  {
    auto const s = GetStartJoint();
    return make_pair(s, s);
  }

  TVertexType GetFinishVertex() const
  {
    auto const t = GetFinishJoint();
    return make_pair(t, t);
  }

  m2::PointD const & GetPoint(Joint::Id jointId);
  m2::PointD const & GetPoint(RoadPoint const & rp);

  // Clears |edges| and fills with all outgoing edges from |u|.
  void GetOutgoingEdgesList(TVertexType const & u, vector<TEdgeType> & edges);

  // Clears |edges| and fills with all ingoing edges to |u|.
  void GetIngoingEdgesList(TVertexType const & u, vector<TEdgeType> & edges);

  double HeuristicCostEstimate(TVertexType const & from, TVertexType const & to)
  {
    return m_graph.GetEstimator().CalcHeuristic(GetPoint(from.second), GetPoint(to.second));
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

  DirectedEdge const & FindFakeEdge(uint32_t fakeFeatureId);
  RoadGeometry GetFakeRoadGeometry(uint32_t fakeFeatureId);

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

  // Applies all possible penalties to the |weight| and returns
  // it. Return value is always not less that the |weight|.
  double ApplyPenalties(TVertexType const & u, TVertexType const & v, double weight);

  // Returns a pair of original (before restrictions) road points for
  // a pair of joints lying on the same feature.
  pair<RoadPoint, RoadPoint> GetOriginal(Joint::Id u, Joint::Id v);

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
