#pragma once

#include "routing/index_graph.hpp"
#include "routing/joint.hpp"
#include "routing/route_point.hpp"

#include "std/set.hpp"
#include "std/type_traits.hpp"
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
  // of joints (JointEdge). But this space is too restrictive, as it's
  // hard to add penalties for left turns, U-turns, restrictions and
  // so on. Therefore, we run A* algorithm on a different graph G' =
  // (V', E'), where each vertex from V' is a pair of vertices
  // (previous vertex, current vertex) from V.
  //
  // Following laws hold:
  //
  // 1. (s, s) in V', where s is the start joint.
  //
  // 2. (t, t) in V', where t is the finish joint.
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
  //
  // TODO (@bykoianko): replace this after join of DirectedEdge and
  // JointEdge by a merged edge type.
  class TVertexType final
  {
  public:
    static_assert(is_integral<Joint::Id>::value, "Please, revisit noexcept specifiers below.");

    TVertexType() = default;
    TVertexType(Joint::Id prev, Joint::Id curr, double weight) noexcept;

    bool operator==(TVertexType const & rhs) const noexcept;
    bool operator<(TVertexType const & rhs) const noexcept;

    inline Joint::Id GetPrev() const noexcept { return m_prev; }
    inline Joint::Id GetCurr() const noexcept { return m_curr; }
    inline double GetWeight() const noexcept { return m_weight; }

  private:
    // A joint we came from.
    Joint::Id m_prev = Joint::kInvalidId;

    // A joint we are currently on.
    Joint::Id m_curr = Joint::kInvalidId;

    // A weight between m_prev and m_curr.
    double m_weight = 0.0;
  };

  class TEdgeType final
  {
  public:
    TEdgeType(TVertexType const & target, double weight) noexcept
      : m_target(target), m_weight(weight)
    {
    }

    TVertexType GetTarget() const noexcept { return m_target; }
    double GetWeight() const noexcept { return m_weight; }

  private:
    TVertexType const m_target;
    double const m_weight;
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
    return TVertexType(s, s, 0.0 /* weight */);
  }

  TVertexType GetFinishVertex() const
  {
    auto const t = GetFinishJoint();
    return TVertexType(t, t, 0.0 /* weight */);
  }

  m2::PointD const & GetPoint(Joint::Id jointId);
  m2::PointD const & GetPoint(RoadPoint const & rp);

  // Clears |edges| and fills with all outgoing edges from |u|.
  void GetOutgoingEdgesList(TVertexType const & u, vector<TEdgeType> & edges);

  // Clears |edges| and fills with all ingoing edges to |u|.
  void GetIngoingEdgesList(TVertexType const & u, vector<TEdgeType> & edges);

  double HeuristicCostEstimate(TVertexType const & from, TVertexType const & to)
  {
    return m_graph.GetEstimator().CalcHeuristic(GetPoint(from.GetCurr()), GetPoint(to.GetCurr()));
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

  // Returns true if there is a U-turn from |u| to |v|.
  bool IsUTurn(TVertexType const & u, TVertexType const & v);

  // Get's all possible penalties when passing from |u| to |v|.
  double GetPenalties(TVertexType const & u, TVertexType const & v);

  // Returns original (before restrictions) feature for a |vertex|.
  uint32_t GetOriginalFeature(TVertexType const & vertex);

  // Returns a pair of original (before restrictions) road points for
  // a |vertex|.
  pair<RoadPoint, RoadPoint> GetOriginalEndPoints(TVertexType const & vertex);

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

string DebugPrint(IndexGraphStarter::TVertexType const & u);
}  // namespace routing
