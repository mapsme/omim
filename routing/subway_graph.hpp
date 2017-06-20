#pragma once

#include "routing/num_mwm_id.hpp"
#include "routing/subway_cache.hpp"
#include "routing/subway_edge.hpp"

#include "routing/subway_vertex.hpp"

#include "routing_common/vehicle_model.hpp"

#include "indexer/index.hpp"
#include "indexer/feature.hpp"

#include <memory>
#include <utility>
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

  SubwayGraph(std::shared_ptr<VehicleModelFactory> modelFactory,
              std::shared_ptr<NumMwmIds> numMwmIds, std::shared_ptr<SubwayCache> cache,
              Index & index);

  // Interface for AStarAlgorithm:
  void GetOutgoingEdgesList(TVertexType const & vertex, vector<TEdgeType> & edges);
  void GetIngoingEdgesList(TVertexType const & vertex, vector<TEdgeType> & edges);
  double HeuristicCostEstimate(TVertexType const & from, TVertexType const & to);

  bool GetNearestStation(m2::PointD const & point, SubwayVertex & vertex) const;

private:
  bool IsValidRoad(FeatureType const & ft) const;

  std::shared_ptr<VehicleModelFactory> m_modelFactory;
  std::shared_ptr<NumMwmIds> m_numMwmIds;
  std::shared_ptr<SubwayCache> m_cache;
  Index & m_index;
};
}  // namespace routing
