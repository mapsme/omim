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
class MapboxApi : public RoutingApi
{
public:
  MapboxApi();
  ~MapboxApi() override = default;

  // https://docs.mapbox.com/api/navigation/#directions-api-restrictions-and-limits
  static uint32_t constexpr kMaxRPS = 5;

  Response CalculateRoute(Params const & params) const override;

  uint32_t GetMaxRPS() const override;

private:
  MapboxResponse MakeRequest(Params const & params) const;

  std::string GetDirectionsURL(Params const & params) const;
};
}  // namespace mapbox
}  // namespace api
}  // namespace routing_quality
