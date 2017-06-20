#pragma once

#include "routing/subway_vertex.hpp"

#include <limits>

namespace routing
{
class SubwayEdge final
{
public:
  SubwayEdge(SubwayVertex const & target, double weight) : m_target(target), m_weight(weight) {}
  SubwayEdge() = default;

  SubwayVertex const & GetTarget() const { return m_target; }
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
  SubwayVertex m_target;
  double m_weight = std::numeric_limits<double>::max();
};
}  // namespace routing
