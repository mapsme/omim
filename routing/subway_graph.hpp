#pragma once

#include "routing/subway_edge.hpp"
#include "routing/world_road_point.hpp"

#include "indexer/feature.hpp"

#include <vector>

namespace routing
{
class SubwayGraph final
{
public:
  // AStarAlgorithm types aliases:
  using TVertexType = WorldRoadPoint;
  using TEdgeType = SubwayEdge;

  // Interface for AStarAlgorithm:
  void GetOutgoingEdgesList(TVertexType const & vertex, vector<TEdgeType> & edges);
  void GetIngoingEdgesList(TVertexType const & vertex, vector<TEdgeType> & edges);
  double HeuristicCostEstimate(TVertexType const & from, TVertexType const & to);

  WorldRoadPoint GetNearestStation(m2::PointD const & point) const;
};
}  // namespace routing
