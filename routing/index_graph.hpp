#pragma once

#include "routing/edge_estimator.hpp"
#include "routing/geometry.hpp"
#include "routing/joint.hpp"
#include "routing/joint_index.hpp"
#include "routing/restrictions_serialization.hpp"
#include "routing/road_index.hpp"
#include "routing/road_point.hpp"
#include "routing/segment.hpp"

#include "geometry/point2d.hpp"

#include <cstdint>
#include <memory>
#include <memory>
#include <utility>
#include <vector>

namespace routing
{
class IndexGraph final
{
public:
  IndexGraph() = default;
  explicit IndexGraph(std::unique_ptr<GeometryLoader> loader, std::shared_ptr<EdgeEstimator> estimator);

  // Put outgoing (or ingoing) egdes for segment to the 'edges' vector.
  void GetEdgeList(Segment const & segment, bool isOutgoing, std::vector<SegmentEdge> & edges);

  Joint::Id GetJointId(RoadPoint const & rp) const { return m_roadIndex.GetJointId(rp); }

  Geometry & GetGeometry() { return m_geometry; }
  bool IsRoad(uint32_t featureId) const { return m_roadIndex.IsRoad(featureId); }
  RoadJointIds const & GetRoad(uint32_t featureId) const { return m_roadIndex.GetRoad(featureId); }

  uint32_t GetNumRoads() const { return m_roadIndex.GetSize(); }
  uint32_t GetNumJoints() const { return m_jointIndex.GetNumJoints(); }
  uint32_t GetNumPoints() const { return m_jointIndex.GetNumPoints(); }

  void Build(uint32_t numJoints);
  void Import(std::vector<Joint> const & joints);

  void SetRestrictions(RestrictionVec && restrictions);

  void PushFromSerializer(Joint::Id jointId, RoadPoint const & rp)
  {
    m_roadIndex.PushFromSerializer(jointId, rp);
  }

  template <typename F>
  void ForEachRoad(F && f) const
  {
    m_roadIndex.ForEachRoad(std::forward<F>(f));
  }

  template <typename F>
  void ForEachPoint(Joint::Id jointId, F && f) const
  {
    m_jointIndex.ForEachPoint(jointId, std::forward<F>(f));
  }

private:
  double CalcSegmentWeight(Segment const & segment);
  void GetNeighboringEdges(Segment const & from, RoadPoint const & rp, bool isOutgoing,
                           std::vector<SegmentEdge> & edges);
  void GetNeighboringEdge(Segment const & from, Segment const & to, bool isOutgoing,
                          std::vector<SegmentEdge> & edges);
  double GetPenalties(Segment const & u, Segment const & v) const;

  Geometry m_geometry;
  std::shared_ptr<EdgeEstimator> m_estimator;
  RoadIndex m_roadIndex;
  JointIndex m_jointIndex;
  RestrictionVec m_restrictions;
};
}  // namespace routing
