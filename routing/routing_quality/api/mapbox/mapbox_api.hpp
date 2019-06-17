#pragma once

#include "routing/routing_quality/api/api.hpp"

#include "routing/routing_quality/api/mapbox/types.hpp"

#include "base/assert.hpp"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace routing_quality
{
namespace api
{
namespace mapbox
{
class MapboxApi : public RoutingApiBase
{
public:
  Response CalculateRoute(VehicleType type,
                          ms::LatLon const & start, ms::LatLon const & finish) override;

private:
  MapboxResponse MakeRequest(VehicleType type,
                             ms::LatLon const & start, ms::LatLon const & finish) const;

  std::string GetDirectionsURL(VehicleType type,
                               ms::LatLon const & start, ms::LatLon const & finish) const;
};
}  // namespace mapbox
}  // namespace api
}  // namespace routing_quality
