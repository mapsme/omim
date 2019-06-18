#include "routing/routing_quality/waypoints.hpp"

#include "routing/base/followed_polyline.hpp"

#include "geometry/mercator.hpp"
#include "geometry/point2d.hpp"

#include "base/assert.hpp"
#include "base/logging.hpp"

#include <algorithm>
#include <utility>

using namespace std;

namespace routing_quality
{
namespace metrics
{
Similarity CompareByNumberOfMatchedWaypoints(routing::FollowedPolyline && polyline,
                                             ReferenceRoutes && candidates)
{
  auto constexpr kMaxDistanceFromRouteM = 15;
  Similarity bestResult = 0.0;
  for (size_t j = 0; j < candidates.size(); ++j)
  {
    routing::FollowedPolyline current = polyline;
    auto const & waypoints = candidates[j];
    auto const size = waypoints.size();
    CHECK_GREATER(size, 0, ());
    size_t numberOfErrors = 0;
    for (size_t i = 0; i < size; ++i)
    {
      auto const & ll = waypoints[i];
      m2::RectD const rect = MercatorBounds::MetersToXY(ll.m_lon, ll.m_lat, kMaxDistanceFromRouteM /* metresR */);
      auto const iter = current.UpdateProjection(rect);
      if (iter.IsValid())
      {
        auto const distFromRouteM = MercatorBounds::DistanceOnEarth(iter.m_pt, MercatorBounds::FromLatLon(ll));
        if (distFromRouteM <= kMaxDistanceFromRouteM)
          continue;
      }

      LOG(LDEBUG, ("Can't find point", ll, "with index", i));
      ++numberOfErrors;
    }

    CHECK_LESS_OR_EQUAL(numberOfErrors, size, ());
    auto const result = ((size - numberOfErrors) / static_cast<double>(size));
    LOG(LDEBUG, ("Matching result", result, "for route with index", j));
    bestResult = max(bestResult, result);
  }

  LOG(LDEBUG, ("Best result", bestResult));
  return bestResult;
}
}  // namespace metrics

Similarity CheckWaypoints(Params const & params, ReferenceRoutes && referenceRoutes)
{
  auto & builder = routing::routes_builder::RoutesBuilder::GetSimpleRoutesBuilder();
  auto result = builder.ProcessTask(params);

  return metrics::CompareByNumberOfMatchedWaypoints(move(result.m_followedPolyline),
                                                    move(referenceRoutes));
}

bool CheckRoute(Params const & params, ReferenceRoutes && referenceRoutes)
{
  return CheckWaypoints(params, move(referenceRoutes)) == 1.0;
}

bool CheckCarRoute(ms::LatLon const & start, ms::LatLon const & finish,
                   ReferenceRoutes && referenceTracks)
{
  Params params(routing::VehicleType::Car, start, finish);
  return CheckRoute(params, move(referenceTracks));
}
}  // namespace routing_quality
