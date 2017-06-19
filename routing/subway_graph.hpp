#pragma once

#include "routing/subway_edge.hpp"
#include "routing/subway_vertex.hpp"

#include "indexer/feature.hpp"

#include <vector>

namespace routing
{
class SubwayGraph final
{
public:
  // AStarAlgorithm types aliases:
  using TVertexType = SubwayVertex;
  using TEdgeType = SubwayEdge;
  using TWeightType = double;

  // Interface for AStarAlgorithm:
  void GetOutgoingEdgesList(TVertexType const & vertex, vector<TEdgeType> & edges);
  void GetIngoingEdgesList(TVertexType const & vertex, vector<TEdgeType> & edges);
  double HeuristicCostEstimate(TVertexType const & from, TVertexType const & to);

  SubwayVertex GetNearestStation(m2::PointD const & point) const;
};
}  // namespace routing
