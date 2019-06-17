#pragma once

#include "routing/routes_builder/routes_builder.hpp"

#include "routing/vehicle_mask.hpp"

#include "geometry/latlon.hpp"

#include <vector>

namespace routing_quality
{
using Params = routing::routes_builder::RoutesBuilder::Params;

using Waypoints = std::vector<ms::LatLon>;

/// \brief There can be more than one reference route.
using ReferenceRoutes = std::vector<Waypoints>;

using Similarity = double;

/// \brief Checks how many reference waypoints the route contains.
/// \returns normalized value in range [0.0; 1.0].
Similarity CheckWaypoints(Params const & params, ReferenceRoutes && referenceRoutes);

/// \returns true if route from |start| to |finish| fully conforms one of |candidates|
/// and false otherwise.
bool CheckRoute(Params const & params, ReferenceRoutes && referenceRoutes);
bool CheckCarRoute(ms::LatLon const & start, ms::LatLon const & finish,
                   ReferenceRoutes && referenceRoutes);
}  // namespace routing_quality
