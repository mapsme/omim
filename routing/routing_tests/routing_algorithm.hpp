#pragma once

#include "routing/base/astar_algorithm.hpp"
#include "routing/base/routing_result.hpp"

#include "routing/road_graph.hpp"
#include "routing/router.hpp"

#include "geometry/point_with_altitude.hpp"

#include <cstdint>
#include <map>
#include <string>

namespace routing_test
{
using namespace routing;

struct SimpleEdge
{
  SimpleEdge(uint32_t to, double weight) : m_to(to), m_weight(weight) {}

  uint32_t GetTarget() const { return m_to; }
  double GetWeight() const { return m_weight; }

  uint32_t m_to;
  double m_weight;
};

class UndirectedGraph : public AStarGraph<uint32_t, SimpleEdge, double>
{
public:
  UndirectedGraph();

  void AddEdge(Vertex u, Vertex v, Weight w);
  size_t GetNodesNumber() const;

  // AStarGraph overrides
  // @{
  void GetIngoingEdgesList(astar::VertexData<Vertex, Weight> const & vertexData,
                           std::vector<Edge> & adj) override;
  void GetOutgoingEdgesList(astar::VertexData<Vertex, Weight> const & vertexData,
                            std::vector<Edge> & adj) override;
  double HeuristicCostEstimate(Vertex const & v, Vertex const & w, bool isOutgoing) override;
  virtual bool IsTwoThreadsReady() const override { return m_twoThreadsReady; }
  // @}

  void GetEdgesList(Vertex const & vertex, bool /* isOutgoing */, std::vector<Edge> & adj);
  /// \note If |twoThreadsRead| is equal to true it means class should be ready that methods
  /// GetIngoingEdgesList(), GetOutgoingEdgesList() and HeuristicCostEstimate() are called
  /// from two threads depending on |isOutgoing| parameter.
  void SetTwoThreadsReady(bool twoThreadsRead) { m_twoThreadsReady = twoThreadsRead; }

private:
  void GetAdjacencyList(Vertex v, std::vector<Edge> & adj) const;

  std::map<uint32_t, std::vector<Edge>> m_adjs;
  bool m_twoThreadsReady = false;
};

class DirectedGraph
{
public:
  using Vertex = uint32_t;
  using Edge = SimpleEdge;
  using Weight = double;

  void AddEdge(Vertex from, Vertex to, Weight w);

  void GetEdgesList(Vertex const & v, bool isOutgoing, std::vector<Edge> & adj);

private:
  std::map<uint32_t, std::vector<Edge>> m_outgoing;
  std::map<uint32_t, std::vector<Edge>> m_ingoing;
};
}  // namespace routing_tests

namespace routing
{
class TestAStarBidirectionalAlgo
{
public:
  enum class Result
  {
    OK,
    NoPath,
    Cancelled
  };

  Result CalculateRoute(IRoadGraph const & graph, geometry::PointWithAltitude const & startPos,
                        geometry::PointWithAltitude const & finalPos,
                        RoutingResult<IRoadGraph::Vertex, IRoadGraph::Weight> & path);
};

std::string DebugPrint(TestAStarBidirectionalAlgo::Result const & result);
}  // namespace routing
