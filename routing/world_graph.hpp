#pragma once

#include "routing/edge_estimator.hpp"
#include "routing/geometry.hpp"
#include "routing/index_graph.hpp"
#include "routing/joint_segment.hpp"
#include "routing/road_graph.hpp"
#include "routing/route.hpp"
#include "routing/routing_options.hpp"
#include "routing/segment.hpp"
#include "routing/transit_info.hpp"

#include "routing_common/num_mwm_id.hpp"

#include "geometry/point2d.hpp"

#include <functional>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace routing
{
enum class WorldGraphMode
{
  LeapsOnly,       // Mode for building a cross mwm route containing only leaps. In case of start and
                   // finish they (start and finish) will be connected with all transition segments of
                   // their mwm with leap (fake) edges.
  NoLeaps,         // Mode for building route and getting outgoing/ingoing edges without leaps at all.
  SingleMwm,       // Mode for building route and getting outgoing/ingoing edges within mwm source
                   // segment belongs to.
  Joints,          // Mode for building route with jumps between Joints.
  JointSingleMwm,  // Like |SingleMwm|, but in |Joints| mode.

  Undefined        // Default mode, until initialization.
};

class WorldGraph
{
public:
  // AStarAlgorithm and CheckGraphConnectivity() types aliases:
  using Vertex = IndexGraph::Vertex;
  using Edge = IndexGraph::Edge;
  using Weight = IndexGraph::Weight;

  virtual ~WorldGraph() = default;

  virtual void GetEdgeList(Segment const & segment, bool isOutgoing, bool useRoutingOptions,
                           std::vector<SegmentEdge> & edges) = 0;
  virtual void GetEdgeList(JointSegment const & vertex, Segment const & segment, bool isOutgoing,
                           std::vector<JointEdge> & edges, std::vector<RouteWeight> & parentWeights) = 0;

  // Checks whether path length meets restrictions. Restrictions may depend on the distance from
  // start to finish of the route.
  virtual bool CheckLength(RouteWeight const & weight, double startToFinishDistanceM) const = 0;

  virtual Junction const & GetJunction(Segment const & segment, bool front) = 0;
  virtual m2::PointD const & GetPoint(Segment const & segment, bool front) = 0;
  virtual bool IsOneWay(NumMwmId mwmId, uint32_t featureId) = 0;

  // Checks whether feature is allowed for through passage.
  virtual bool IsPassThroughAllowed(NumMwmId mwmId, uint32_t featureId) = 0;

  // Clear memory used by loaded graphs.
  virtual void ClearCachedGraphs() = 0;
  virtual void SetMode(WorldGraphMode mode) = 0;
  virtual WorldGraphMode GetMode() const = 0;

  virtual void GetOutgoingEdgesList(Segment const & segment, std::vector<SegmentEdge> & edges) = 0;
  virtual void GetIngoingEdgesList(Segment const & segment, std::vector<SegmentEdge> & edges) = 0;

  virtual RouteWeight HeuristicCostEstimate(Segment const & from, Segment const & to) = 0;
  virtual RouteWeight HeuristicCostEstimate(m2::PointD const & from, m2::PointD const & to) = 0;
  virtual RouteWeight HeuristicCostEstimate(Segment const & from, m2::PointD const & to) = 0;

  virtual RouteWeight CalcSegmentWeight(Segment const & segment,
                                        EdgeEstimator::Purpose purpose) = 0;

  virtual RouteWeight CalcLeapWeight(m2::PointD const & from, m2::PointD const & to) const = 0;

  virtual RouteWeight CalcOffroadWeight(m2::PointD const & from, m2::PointD const & to,
                                        EdgeEstimator::Purpose purpose) const = 0;

  virtual double CalculateETA(Segment const & from, Segment const & to) = 0;
  virtual double CalculateETAWithoutPenalty(Segment const & segment) = 0;

  /// \returns transitions for mwm with id |numMwmId|.
  virtual std::vector<Segment> const & GetTransitions(NumMwmId numMwmId, bool isEnter);

  virtual bool IsRoutingOptionsGood(Segment const & /* segment */);
  virtual RoutingOptions GetRoutingOptions(Segment const & /* segment */);
  virtual void SetRoutingOptions(RoutingOptions /* routingOptions */);

  using ParentSegments = std::map<Segment, Segment>;
  using ParentJoints = std::map<JointSegment, JointSegment>;

  virtual void SetAStarParents(bool forward, ParentSegments & parents);
  virtual void SetAStarParents(bool forward, ParentJoints & parents);
  virtual void DropAStarParents();

  virtual bool AreWavesConnectible(ParentSegments & forwardParents, Segment const & commonVertex,
                                  ParentSegments & backwardParents,
                                  std::function<uint32_t(Segment const &)> && fakeFeatureConverter);
  virtual bool AreWavesConnectible(ParentJoints & forwardParents, JointSegment const & commonVertex,
                                  ParentJoints & backwardParents,
                                  std::function<uint32_t(JointSegment const &)> && fakeFeatureConverter);

  /// \returns transit-specific information for segment. For nontransit segments returns nullptr.
  virtual std::unique_ptr<TransitInfo> GetTransitInfo(Segment const & segment) = 0;

  virtual std::vector<RouteSegment::SpeedCamera> GetSpeedCamInfo(Segment const & segment);

  virtual IndexGraph & GetIndexGraph(NumMwmId numMwmId) = 0;

protected:
  void GetTwins(Segment const & segment, bool isOutgoing, bool useRoutingOptions,
                std::vector<SegmentEdge> & edges);
  virtual void GetTwinsInner(Segment const & segment, bool isOutgoing,
                             std::vector<Segment> & twins) = 0;
};

std::string DebugPrint(WorldGraphMode mode);
}  // namespace routing
