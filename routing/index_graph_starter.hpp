#pragma once

#include "routing/index_graph.hpp"
#include "routing/joint.hpp"

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
  friend struct routing_test::RestrictionTest;

  // AStarAlgorithm types aliases:
  using TVertexType = Joint::Id;
  using TEdgeType = JointEdge;

  enum class EndPointType { Start, Finish };

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
  void RedressRoute(vector<Joint::Id> const & route, vector<RoadPoint> & roadPoints);

  bool IsAdditionalFeature(uint32_t featureId) const { return featureId >= m_graph.GetNextFakeFeatureId(); }

private:
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

    m_graph.ForEachPoint(jointId, forward<F>(f));
  }

  void AddZeroLengthOnewayFeature(Joint::Id from, Joint::Id to);
  void GetAdditionalEdgesList(Joint::Id jointId, bool isOutgoing, vector<JointEdge> & edges);

  void GetEdgesList(Joint::Id jointId, bool isOutgoing, vector<JointEdge> & edges);
  void GetFakeEdges(FakeJoint const & from, FakeJoint const & to, bool isOutgoing,
                    vector<JointEdge> & edges);
  void GetArrivalFakeEdges(Joint::Id jointId, FakeJoint const & fakeJoint, bool isOutgoing,
                           vector<JointEdge> & edges);

  // Find two road points lying on the same feature.
  // If there are several pairs of such points, return pair with minimal distance.
  void FindPointsWithCommonFeature(Joint::Id jointId0, Joint::Id jointId1, RoadPoint & result0,
                                   RoadPoint & result1);

  void AddAdditionalZeroEdges(FakeJoint const & fp, EndPointType endPointType);

  // Edges which added to IndexGraph in IndexGraphStarter. Size of |m_additionalZeroEdges| should be
  // relevantly small because some methods working with it have linear complexity.
  vector<DirectedEdge> m_additionalZeroEdges;
  // |m_jointsOfAdditionalEdges| is set of all joint ids take part in |m_additionalZeroEdges|.
  set<Joint::Id> m_jointsOfAdditionalEdges;
  uint32_t m_additionalNextFeatureId;
  IndexGraph & m_graph;
  FakeJoint m_start;
  FakeJoint m_finish;
};
}  // namespace routing
