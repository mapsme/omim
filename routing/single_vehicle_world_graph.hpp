#pragma once

#include "routing/cross_mwm_graph.hpp"
#include "routing/edge_estimator.hpp"
#include "routing/geometry.hpp"
#include "routing/index_graph.hpp"
#include "routing/index_graph_loader.hpp"
#include "routing/joint_segment.hpp"
#include "routing/road_graph.hpp"
#include "routing/route.hpp"
#include "routing/segment.hpp"
#include "routing/transit_info.hpp"
#include "routing/world_graph.hpp"

#include "routing_common/num_mwm_id.hpp"

#include "geometry/point2d.hpp"

#include <memory>
#include <vector>

namespace routing
{
class SingleVehicleWorldGraph final : public WorldGraph
{
public:
  SingleVehicleWorldGraph(std::unique_ptr<CrossMwmGraph> crossMwmGraph,
                          std::unique_ptr<IndexGraphLoader> loader,
                          std::shared_ptr<EdgeEstimator> estimator);

  // WorldGraph overrides:
  ~SingleVehicleWorldGraph() override = default;

  void GetEdgeList(Segment const & segment, bool isOutgoing,
                   std::vector<SegmentEdge> & edges) override;

  void GetEdgeList(JointSegment const & parentJoint, Segment const & parent, bool isOutgoing,
                   std::vector<JointEdge> & jointEdges, std::vector<RouteWeight> & parentWeights) override;

  bool CheckLength(RouteWeight const &, double) const override { return true; }
  Junction const & GetJunction(Segment const & segment, bool front) override;
  m2::PointD const & GetPoint(Segment const & segment, bool front) override;
  bool IsOneWay(NumMwmId mwmId, uint32_t featureId) override;
  bool IsPassThroughAllowed(NumMwmId mwmId, uint32_t featureId) override;
  void ClearCachedGraphs() override { m_loader->Clear(); }
  void SetMode(WorldGraphMode mode) override { m_mode = mode; }
  WorldGraphMode GetMode() const override { return m_mode; }
  void GetOutgoingEdgesList(Segment const & segment, std::vector<SegmentEdge> & edges) override;
  void GetIngoingEdgesList(Segment const & segment, std::vector<SegmentEdge> & edges) override;
  RouteWeight HeuristicCostEstimate(Segment const & from, Segment const & to) override;
  RouteWeight HeuristicCostEstimate(m2::PointD const & from, m2::PointD const & to) override;
  RouteWeight HeuristicCostEstimate(Segment const & from, m2::PointD const & to) override;
  RouteWeight CalcSegmentWeight(Segment const & segment) override;
  RouteWeight CalcLeapWeight(m2::PointD const & from, m2::PointD const & to) const override;
  RouteWeight CalcOffroadWeight(m2::PointD const & from, m2::PointD const & to) const override;
  double CalcSegmentETA(Segment const & segment) override;
  std::vector<Segment> const & GetTransitions(NumMwmId numMwmId, bool isEnter) override;

  /// \returns true if feature, associated with segment satisfies users conditions.
  bool IsRoutingOptionsGood(Segment const & segment) override;
  RoutingOptions GetRoutingOptions(Segment const & segment) override;
  void SetRoutingOptions(RoutingOptions routingOptions) override { m_avoidRoutingOptions = routingOptions; }

  std::unique_ptr<TransitInfo> GetTransitInfo(Segment const & segment) override;
  std::vector<RouteSegment::SpeedCamera> GetSpeedCamInfo(Segment const & segment) override;

  // This method should be used for tests only
  IndexGraph & GetIndexGraphForTests(NumMwmId numMwmId)
  {
    return m_loader->GetIndexGraph(numMwmId);
  }

  IndexGraph & GetIndexGraph(NumMwmId numMwmId) override
  {
    return m_loader->GetIndexGraph(numMwmId);
  }

  void SetAStarParents(bool forward, std::map<Segment, Segment> & parents) override
  {
    if (forward)
      m_parentsForSegments.forward = &parents;
    else
      m_parentsForSegments.backward = &parents;
  }

  void SetAStarParents(bool forward, std::map<JointSegment, JointSegment> & parents) override
  {
    if (forward)
      m_parentsForJoints.forward = &parents;
    else
      m_parentsForJoints.backward = &parents;
  }

private:
  // Retrieves the same |jointEdges|, but into others mwms.
  // If they are cross mwm edges, of course.
  void CheckAndProcessTransitFeatures(Segment const & parent,
                                      std::vector<JointEdge> & jointEdges,
                                      std::vector<RouteWeight> & parentWeights,
                                      bool isOutgoing);
  // WorldGraph overrides:
  void GetTwinsInner(Segment const & s, bool isOutgoing, std::vector<Segment> & twins) override;

  RoadGeometry const & GetRoadGeometry(NumMwmId mwmId, uint32_t featureId);

  std::unique_ptr<CrossMwmGraph> m_crossMwmGraph;
  std::unique_ptr<IndexGraphLoader> m_loader;
  std::shared_ptr<EdgeEstimator> m_estimator;
  RoutingOptions m_avoidRoutingOptions = RoutingOptions();
  WorldGraphMode m_mode = WorldGraphMode::NoLeaps;

  template <typename Vertex>
  struct AStarParents
  {
    using ParentType = std::map<Vertex, Vertex>;
    static ParentType kEmpty;
    ParentType * forward = &kEmpty;
    ParentType * backward = &kEmpty;
  };

  AStarParents<Segment> m_parentsForSegments;
  AStarParents<JointSegment> m_parentsForJoints;
};

template <>
SingleVehicleWorldGraph::AStarParents<Segment>::ParentType
SingleVehicleWorldGraph::AStarParents<Segment>::kEmpty;

template <>
SingleVehicleWorldGraph::AStarParents<JointSegment>::ParentType
SingleVehicleWorldGraph::AStarParents<JointSegment>::kEmpty;
}  // namespace routing
