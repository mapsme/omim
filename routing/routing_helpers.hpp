#pragma once

#include "routing/directions_engine.hpp"
#include "routing/road_graph.hpp"
#include "routing/route.hpp"
#include "routing/traffic_stash.hpp"

#include "traffic/traffic_info.hpp"

#include "routing_common/bicycle_model.hpp"
#include "routing_common/car_model.hpp"
#include "routing_common/pedestrian_model.hpp"

#include "base/cancellable.hpp"

#include <memory>
#include <vector>

namespace routing
{
/// \returns true when there exists a routing mode where the feature with |types| can be used.
template <class TTypes>
bool IsRoad(TTypes const & types)
{
  return CarModel::AllLimitsInstance().HasRoadType(types) ||
         PedestrianModel::AllLimitsInstance().HasRoadType(types) ||
         BicycleModel::AllLimitsInstance().HasRoadType(types);
}

void ReconstructRoute(IDirectionsEngine & engine, RoadGraphBase const & graph,
                      std::shared_ptr<TrafficStash> const & trafficStash,
                      my::Cancellable const & cancellable, std::vector<Junction> & path, Route & route);
}  // namespace rouing
