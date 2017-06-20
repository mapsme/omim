#pragma once

#include "routing/num_mwm_id.hpp"
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
              std::shared_ptr<NumMwmIds> numMwmIds, Index const & index)
    : m_modelFactory(std::move(modelFactory)), m_numMwmIds(std::move(numMwmIds)), m_index(index)
  {
    CHECK(m_modelFactory, ());
    CHECK(m_numMwmIds, ());
  }

  // Interface for AStarAlgorithm:
  void GetOutgoingEdgesList(TVertexType const & vertex, vector<TEdgeType> & edges);
  void GetIngoingEdgesList(TVertexType const & vertex, vector<TEdgeType> & edges);
  double HeuristicCostEstimate(TVertexType const & from, TVertexType const & to);

  SubwayVertex GetNearestStation(m2::PointD const & point) const;

private:
  std::shared_ptr<VehicleModelFactory> m_modelFactory;
  std::shared_ptr<NumMwmIds> m_numMwmIds;
  Index const & m_index;
};
}  // namespace routing
