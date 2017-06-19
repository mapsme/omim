#pragma once

namespace routing
{
template <typename Vertex>
class Edge final
{
public:
  Edge(Vertex const & target, double weight) : m_target(target), m_weight(weight) {}

  Vertex const & GetTarget() const { return m_target; }
  double GetWeight() const { return m_weight; }

  bool operator==(Edge const & edge) const
  {
    return m_target == edge.m_target && m_weight == edge.m_weight;
  }

  bool operator<(Edge const & edge) const
  {
    if (m_target != edge.m_target)
      return m_target < edge.m_target;
    return m_weight < edge.m_weight;
  }

private:
  // Target is vertex going to for outgoing edges, vertex going from for ingoing edges.
  Vertex m_target;
  double m_weight;
};
}  // namespace routing
