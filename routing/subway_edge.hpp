#pragma once

#include "routing/world_road_point.hpp"

namespace routing
{
class SubwayEdge final
{
public:
  SubwayEdge(WorldRoadPoint const & target, double weight) : m_target(target), m_weight(weight) {}

  WorldRoadPoint const & GetTarget() const { return m_target; }
  double GetWeight() const { return m_weight; }

  bool operator==(SubwayEdge const & edge) const
  {
    return m_target == edge.m_target && m_weight == edge.m_weight;
  }

  bool operator<(SubwayEdge const & edge) const
  {
    if (m_target != edge.m_target)
      return m_target < edge.m_target;
    return m_weight < edge.m_weight;
  }

private:
  // Target is vertex going to for outgoing edges, vertex going from for ingoing edges.
  WorldRoadPoint m_target;
  double m_weight;
};
}  // namespace routing
