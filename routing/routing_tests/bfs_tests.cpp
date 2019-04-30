#include "testing/testing.hpp"

#include "routing/base/bfs.hpp"

#include "routing/routing_tests/routing_algorithm.hpp"

namespace
{
using namespace routing_test;

UndirectedGraph BuildUndirectedGraph()
{
  UndirectedGraph graph;

  // Inserts edges in a format: <source, target, weight>.
  graph.AddEdge(0, 4, 1);
  graph.AddEdge(5, 4, 1);
  graph.AddEdge(4, 1, 1);
  graph.AddEdge(4, 3, 1);
  graph.AddEdge(3, 2, 1);
  graph.AddEdge(7, 4, 1);
  graph.AddEdge(7, 6, 1);
  graph.AddEdge(7, 8, 1);
  graph.AddEdge(8, 9, 1);
  graph.AddEdge(8, 10, 1);

  return graph;
}

DirectedGraph BuildDirectedGraph()
{
  DirectedGraph graph;

  // Inserts edges in a format: <source, target, weight>.
  graph.AddEdge(0, 4, 1);
  graph.AddEdge(5, 4, 1);
  graph.AddEdge(4, 1, 1);
  graph.AddEdge(4, 3, 1);
  graph.AddEdge(3, 2, 1);
  graph.AddEdge(7, 4, 1);
  graph.AddEdge(7, 6, 1);
  graph.AddEdge(7, 8, 1);
  graph.AddEdge(8, 9, 1);
  graph.AddEdge(8, 10, 1);

  return graph;
}
}  // namespace

namespace routing_test
{
using namespace routing;

UNIT_TEST(BFS_AllVisit_Undirected)
{
  UndirectedGraph graph = BuildUndirectedGraph();

  std::set<uint32_t> visited;

  BFS<UndirectedGraph> bfs(graph);
  bfs.Run(0 /* start */, true /* isOutgoing */,
      [&](BFS<UndirectedGraph>::State const & state)
      {
        visited.emplace(state.vertex);
        // Because of graph is undirected, the bfs will execute infinitely.
        return visited.size() != graph.GetNodesNumber();
      });

  TEST_EQUAL(visited.size(), graph.GetNodesNumber(), ("Some nodes were not visited."));
}

UNIT_TEST(BFS_AllVisit_Directed_Forward)
{
  DirectedGraph graph = BuildDirectedGraph();

  std::set<uint32_t> visited;

  BFS<DirectedGraph> bfs(graph);
  bfs.Run(0 /* start */, true /* isOutgoing */,
          [&](BFS<DirectedGraph>::State const & state)
          {
            visited.emplace(state.vertex);
            return true;
          });

  std::vector<uint32_t> const expectedInVisited = {4, 1, 3, 2};
  for (auto const v : expectedInVisited)
    TEST_NOT_EQUAL(visited.count(v), 0, ("vertex =", v, "was not visited."));
}

UNIT_TEST(BFS_AllVisit_Directed_Backward)
{
  DirectedGraph graph = BuildDirectedGraph();

  std::set<uint32_t> visited;

  BFS<DirectedGraph> bfs(graph);
  bfs.Run(2 /* start */, false /* isOutgoing */,
          [&](BFS<DirectedGraph>::State const & state)
          {
            visited.emplace(state.vertex);
            // Because of graph is undirected, the bfs will execute infinitely.
            // So we should stop it when then number of visited
            // vertexes is equal the vertexes number if graph.
            return visited.size() != graph.GetNodesNumber();
          });

  std::vector<uint32_t> expectedInVisited = {3, 4, 0, 5, 7};
  for (auto const v : expectedInVisited)
    TEST_NOT_EQUAL(visited.count(v), 0, ("vertex =", v, "was not visited."));
}
} //  namespace routing_test
