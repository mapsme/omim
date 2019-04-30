#pragma once

#include <functional>
#include <map>
#include <queue>
#include <vector>

namespace routing
{
template <typename Graph>
class BFS
{
public:
  using Vertex = typename Graph::Vertex;
  using Edge = typename Graph::Edge;
  using Weight = typename Graph::Weight;

  struct State
  {
    State(Vertex const & v, Vertex const & p) : vertex(v), parent(p) {}
    Vertex vertex;
    Vertex parent;
  };

  explicit BFS(Graph & graph): m_graph(graph) {}

  void Run(Vertex const & start, bool isOutgoing,
           std::function<bool(State const &)> && onVisitCallback);

  std::vector<Vertex> ReconstructPath(Vertex from);

private:
  Graph & m_graph;
  std::map<Vertex, Vertex> m_parents;
};

template <typename Graph>
void BFS<Graph>::Run(Vertex const & start, bool isOutgoing,
                     std::function<bool(State const &)> && onVisitCallback)
{
  m_parents.clear();

  std::queue<Vertex> queue;
  queue.push(start);

  std::vector<Edge> edges;
  while (!queue.empty())
  {
    Vertex const current = queue.front();
    queue.pop();

    if (isOutgoing)
      m_graph.GetOutgoingEdgesList(current, edges);
    else
      m_graph.GetIngoingEdgesList(current, edges);

    for (auto const & edge : edges)
    {
      Vertex const & child = edge.GetTarget();
      State const state(child, current);

      if (!onVisitCallback(state))
        return;

      m_parents[child] = current;
      queue.push(child);
    }
  }
}

template <typename Graph>
auto BFS<Graph>::ReconstructPath(Vertex from) -> std::vector<Vertex>
{
  std::vector<Vertex> result;
  auto it = m_parents.find(from);
  while (it != m_parents.end())
  {
    result.emplace_back(from);
    from = it->second;
    it = m_parents.find(from);
  }

  result.emplace_back(from);
  return result;
}
}  // namespace routing
