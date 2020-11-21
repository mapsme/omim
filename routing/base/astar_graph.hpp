#pragma once

#include "routing/base/astar_weight.hpp"
#include "routing/base/astar_vertex_data.hpp"

#include <map>
#include <vector>

#include "3party/skarupke/bytell_hash_map.hpp"

namespace routing
{
template <typename VertexType, typename EdgeType, typename WeightType>
class AStarGraph
{
public:
  using Vertex = VertexType;
  using Edge = EdgeType;
  using Weight = WeightType;

  using Parents = ska::bytell_hash_map<Vertex, Vertex>;

  constexpr AStarGraph() {}
  virtual ~AStarGraph() = default;

  virtual Weight HeuristicCostEstimate(Vertex const & from, Vertex const & to, bool isOutgoing) = 0;

  virtual void GetOutgoingEdgesList(astar::VertexData<Vertex, Weight> const & vertexData,
                                    std::vector<Edge> & edges) = 0;
  virtual void GetIngoingEdgesList(astar::VertexData<Vertex, Weight> const & vertexData,
                                   std::vector<Edge> & edges) = 0;

  virtual void SetAStarParents(bool forward, Parents & parents);
  virtual void DropAStarParents();
  virtual bool AreWavesConnectible(Parents & forwardParents, Vertex const & commonVertex,
                                   Parents & backwardParents);

  virtual Weight GetAStarWeightEpsilon();

  virtual bool IsTwoThreadsReady() const { return false; }
};

template <typename VertexType, typename EdgeType, typename WeightType>
void AStarGraph<VertexType, EdgeType, WeightType>::SetAStarParents(bool /* forward */,
                                                                   Parents & /* parents */) {}

template <typename VertexType, typename EdgeType, typename WeightType>
void AStarGraph<VertexType, EdgeType, WeightType>::DropAStarParents() {}

template <typename VertexType, typename EdgeType, typename WeightType>
bool AStarGraph<VertexType, EdgeType, WeightType>::AreWavesConnectible(AStarGraph::Parents & /* forwardParents */,
                                                                       Vertex const & /* commonVertex */,
                                                                       AStarGraph::Parents & /* backwardParents */)
{
  return true;
}

template <typename VertexType, typename EdgeType, typename WeightType>
WeightType AStarGraph<VertexType, EdgeType, WeightType>::GetAStarWeightEpsilon()
{
  return routing::GetAStarWeightEpsilon<WeightType>();
}
}  // namespace routing
