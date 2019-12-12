#include "routing/base/followed_polyline.hpp"

#include "map/bookmark_manager.hpp"
#include "map/routing_manager.hpp"

#include "kml/types.hpp"

#include "geometry/point_with_altitude.hpp"
#include "geometry/polyline2d.hpp"

#include <QtGui/QImage>

#include <cstdint>
#include <utility>
#include <vector>

namespace qt
{
size_t constexpr kMaxChartsCount = 1;

struct RouteData
{
  uint64_t m_trackId = 0;
  geometry::Altitudes m_altitudes;
  routing::FollowedPolyline m_polyLine;
};

std::vector<RouteData> ExtractTracksData(BookmarkManager::KMLDataCollection const & collection,
                                         size_t maxRoutesCount = kMaxChartsCount);

std::vector<std::pair<QImage, std::string>> GenerateBitmapsForRoutes(
    std::vector<RouteData> const & routes, RoutingManager & routingManager);
}  // namespace qt
