#pragma once

#include "routing/cross_mwm_graph.hpp"
#include "routing/edge_estimator.hpp"
#include "routing/geometry.hpp"
#include "routing/index_graph_loader.hpp"
#include "routing/road_graph.hpp"
#include "routing/route.hpp"
#include "routing/segment.hpp"
#include "routing/transit_graph_loader.hpp"
#include "routing/transit_info.hpp"
#include "routing/world_graph.hpp"

#include "routing_common/num_mwm_id.hpp"

#include "transit/transit_types.hpp"

#include "geometry/point2d.hpp"

#include <memory>
#include <vector>

namespace routing
{
// WorldGraph for transit + pedestrian routing
class TransitWorldGraph final : public WorldGraph
{
public:
  TransitWorldGraph(std::unique_ptr<CrossMwmGraph> crossMwmGraph,
                    std::unique_ptr<IndexGraphLoader> indexLoader,
                    std::unique_ptr<TransitGraphLoader> transitLoader,
                    std::shared_ptr<EdgeEstimator> estimator);

  // WorldGraph overrides:
  ~TransitWorldGraph() override = default;

  void GetEdgeList(Segment const & segment, bool isOutgoing,
                   std::vector<SegmentEdge> & edges) override;
  bool CheckLength(RouteWeight const & weight, double startToFinishDistanceM) const override
  {
    return weight.GetWeight() - weight.GetTransitTime() <=
           MaxPedestrianTimeSec(startToFinishDistanceM);
  }
  Junction const & GetJunction(Segment const & segment, bool front) override;
  m2::PointD const & GetPoint(Segment const & segment, bool front) override;
  // All transit features are oneway.
  bool IsOneWay(NumMwmId mwmId, uint32_t featureId) override;
  // All transit features are allowed for through passage.
  bool IsPassThroughAllowed(NumMwmId mwmId, uint32_t featureId) override;
  void ClearCachedGraphs() override;
  void SetMode(Mode mode) override { m_mode = mode; }
  Mode GetMode() const override { return m_mode; }
  void GetOutgoingEdgesList(Segment const & segment, std::vector<SegmentEdge> & edges) override;
  void GetIngoingEdgesList(Segment const & segment, std::vector<SegmentEdge> & edges) override;
  RouteWeight HeuristicCostEstimate(Segment const & from, Segment const & to) override;
  RouteWeight HeuristicCostEstimate(m2::PointD const & from, m2::PointD const & to) override;
  RouteWeight CalcSegmentWeight(Segment const & segment) override;
  RouteWeight CalcLeapWeight(m2::PointD const & from, m2::PointD const & to) const override;
  RouteWeight CalcOffroadWeight(m2::PointD const & from, m2::PointD const & to) const override;
  double CalcSegmentETA(Segment const & segment) override;
  bool LeapIsAllowed(NumMwmId mwmId) const override;
  std::vector<Segment> const & GetTransitions(NumMwmId numMwmId, bool isEnter) override;
  std::unique_ptr<TransitInfo> GetTransitInfo(Segment const & segment) override;
  std::vector<RouteSegment::SpeedCamera> GetSpeedCamInfo(Segment const & segment) override;

private:
  // WorldGraph overrides:
  void GetTwinsInner(Segment const & s, bool isOutgoing, std::vector<Segment> & twins) override;

  static double MaxPedestrianTimeSec(double startToFinishDistanceM)
  {
    // @todo(bykoianko) test and adjust constants.
    // 50 min + 3 additional minutes per 1 km for now.
    return 50 * 60 + (startToFinishDistanceM / 1000) * 3 * 60;
  }

  RoadGeometry const & GetRealRoadGeometry(NumMwmId mwmId, uint32_t featureId);
  void AddRealEdges(Segment const & segment, bool isOutgoing, vector<SegmentEdge> & edges);
  IndexGraph & GetIndexGraph(NumMwmId mwmId);
  TransitGraph & GetTransitGraph(NumMwmId mwmId);

  std::unique_ptr<CrossMwmGraph> m_crossMwmGraph;
  std::unique_ptr<IndexGraphLoader> m_indexLoader;
  std::unique_ptr<TransitGraphLoader> m_transitLoader;
  std::shared_ptr<EdgeEstimator> m_estimator;
  Mode m_mode = Mode::NoLeaps;
  std::vector<Segment> const kEmptyTransitions = {};
};
}  // namespace routing
