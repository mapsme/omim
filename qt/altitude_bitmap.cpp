#include "qt/altitude_bitmap.hpp"

#include "platform/measurement_utils.hpp"

#include <QtGui/QBitmap>

#include <string>

namespace qt
{
RouteData ExtractTrackData(kml::TrackData const & kmlData)
{
  RouteData data;
  m2::PolylineD polyLine;

  for (auto point : kmlData.m_pointsWithAltitudes)
  {
    data.m_altitudes.push_back(point.GetAltitude());
    polyLine.Add(point.GetPoint());
  }

  data.m_altitudes.pop_back();
  data.m_polyLine = routing::FollowedPolyline(polyLine.Begin(), polyLine.End());
  data.m_trackId = kmlData.m_id;

  return data;
}

std::vector<RouteData> ExtractTracksData(BookmarkManager::KMLDataCollection const & collection,
                                         size_t maxRoutesCount)
{
  std::vector<RouteData> routes;

  // Finds
  for (auto const & kmlData : collection)
  {
    for (auto const & track : kmlData.second->m_tracksData)
    {
      routes.push_back(ExtractTrackData(track));
      if (routes.size() >= maxRoutesCount)
        return routes;
    }
  }

  return routes;
}

std::string GetTitleForChart(uint64_t trackId, int32_t deltaAltitudes)
{
  return std::to_string(trackId) +
         " track id. Altitudes. Difference in height: " + std::to_string(deltaAltitudes);
}

std::vector<std::pair<QImage, std::string>> GenerateBitmapsForRoutes(
    std::vector<RouteData> const & routes, RoutingManager & routingManager)
{
  std::vector<std::pair<QImage, std::string>> res;

  size_t constexpr width = 600;
  size_t constexpr height = 300;

  for (auto const & route : routes)
  {
    std::vector<uint8_t> imageRGBAData;
    int32_t minRouteAltitude = 0;
    int32_t maxRouteAltitude = 0;
    measurement_utils::Units altitudeUnits;

    routingManager.GenerateRouteAltitudeChart(
        width, height, route.m_altitudes, route.m_polyLine.GetSegDistanceMeters(), imageRGBAData,
        minRouteAltitude, maxRouteAltitude, altitudeUnits);

    std::string chartTitle = GetTitleForChart(route.m_trackId, maxRouteAltitude - minRouteAltitude);

    res.emplace_back(QImage(imageRGBAData.data(), width, height, QImage::Format::Format_RGBA8888,
                            nullptr, nullptr),
                     chartTitle);
  }

  return res;
}
}  // namespace qt
