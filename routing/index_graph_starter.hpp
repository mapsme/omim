#pragma once

#include "routing/index_graph.hpp"
#include "routing/joint.hpp"
#include "routing/num_mwm_id.hpp"
#include "routing/route_point.hpp"
#include "routing/world_graph.hpp"

#include <limits>
#include <utility>
#include <vector>

namespace routing
{
// IndexGraphStarter adds fake start and finish vertexes for AStarAlgorithm.
class IndexGraphStarter final
{
public:
  // AStarAlgorithm types aliases:
  using TVertexType = Segment;
  using TEdgeType = SegmentEdge;

  class FakeVertex final
  {
  public:
    FakeVertex(NumMwmId mwmId, uint32_t featureId, uint32_t segmentIdx, m2::PointD const & point)
      : m_segment(mwmId, featureId, segmentIdx, true /* forward */), m_point(point)
    {
    }

    FakeVertex(Segment const & segment, m2::PointD const & point)
      : m_segment(segment), m_point(point)
    {
    }

    NumMwmId GetMwmId() const { return m_segment.GetMwmId(); }
    uint32_t GetFeatureId() const { return m_segment.GetFeatureId(); }
    uint32_t GetSegmentIdx() const { return m_segment.GetSegmentIdx(); }
    Segment const & GetSegment() const { return m_segment; }
    m2::PointD const & GetPoint() const { return m_point; }
    bool Fits(Segment const & segment) const
    {
      return segment.GetMwmId() == GetMwmId() && segment.GetFeatureId() == GetFeatureId() &&
             segment.GetSegmentIdx() == GetSegmentIdx();
    }

  private:
    Segment m_segment;
    m2::PointD const m_point;
  };

  static uint32_t constexpr kFakeFeatureId = numeric_limits<uint32_t>::max();
  static uint32_t constexpr kFakeSegmentIdx = numeric_limits<uint32_t>::max();
  static Segment constexpr kStartFakeSegment =
    Segment(kFakeNumMwmId, kFakeFeatureId, kFakeSegmentIdx, false);
  static Segment constexpr kFinishFakeSegment =
    Segment(kFakeNumMwmId, kFakeFeatureId, kFakeSegmentIdx, true);

  IndexGraphStarter(FakeVertex const & start, FakeVertex const & finish, WorldGraph & graph);

  WorldGraph & GetGraph() { return m_graph; }
  Segment const & GetStart() const { return kStartFakeSegment; }
  Segment const & GetFinish() const { return kFinishFakeSegment; }
  FakeVertex const & GetStartVertex() const { return m_start; }
  FakeVertex const & GetFinishVertex() const { return m_finish; }
  m2::PointD const & GetPoint(Segment const & segment, bool front);

  static size_t GetRouteNumPoints(std::vector<Segment> const & route);
  m2::PointD const & GetRoutePoint(std::vector<Segment> const & route, size_t pointIndex);

  void GetEdgesList(Segment const & segment, bool isOutgoing, std::vector<SegmentEdge> & edges);

  void GetOutgoingEdgesList(TVertexType const & segment, std::vector<TEdgeType> & edges)
  {
    GetEdgesList(segment, true /* isOutgoing */, edges);
  }

  void GetIngoingEdgesList(TVertexType const & segment, std::vector<TEdgeType> & edges)
  {
    GetEdgesList(segment, false /* isOutgoing */, edges);
  }

  double HeuristicCostEstimate(TVertexType const & from, TVertexType const & to)
  {
    return m_graph.GetEstimator().CalcHeuristic(GetPoint(from, true /* front */),
                                                GetPoint(to, true /* front */));
  }

  double CalcSegmentWeight(Segment const & segment) const
  {
    return m_graph.GetEstimator().CalcSegmentWeight(
        segment, m_graph.GetRoadGeometry(segment.GetMwmId(), segment.GetFeatureId()));
  }

  bool IsLeap(NumMwmId mwmId) const
  {
    return mwmId != kFakeNumMwmId && mwmId != m_start.GetMwmId() && mwmId != m_finish.GetMwmId() &&
           m_graph.GetEstimator().LeapIsAllowed(mwmId);
  }

  static bool IsFakeSegment(Segment const & segment)
  {
    return segment.GetFeatureId() == kFakeFeatureId;
  }

private:
  void GetFakeToNormalEdges(FakeVertex const & fakeVertex, bool isOutgoing,
                            std::vector<SegmentEdge> & edges);
  void GetFakeToNormalEdge(FakeVertex const & fakeVertex, bool forward,
                           std::vector<SegmentEdge> & edges);
  void GetNormalToFakeEdge(Segment const & segment, FakeVertex const & fakeVertex,
                           Segment const & fakeSegment, bool isOutgoing,
                           std::vector<SegmentEdge> & edges);
  /// \brief If |isOutgoing| == true fills |edges| with SegmentEdge(s) which connects
  /// |fakeVertex| with all exits of mwm.
  /// \brief If |isOutgoing| == false fills |edges| with SegmentEdge(s) which connects
  /// all enters to mwm with |fakeVertex|.
  void ConnectLeapToTransitions(FakeVertex const & fakeVertex, bool isOutgoing,
                                std::vector<SegmentEdge> & edges);

  WorldGraph & m_graph;
  FakeVertex const m_start;
  FakeVertex const m_finish;
};
}  // namespace routing
