#include "routing/routing_quality/api/mapbox/mapbox_api.hpp"

#include "routing/vehicle_mask.hpp"

#include "coding/file_writer.hpp"
#include "coding/serdes_json.hpp"
#include "coding/url_encode.hpp"
#include "coding/writer.hpp"

#include "platform/http_client.hpp"

#include "geometry/latlon.hpp"

#include "base/logging.hpp"

#include <sstream>

using namespace std;
using namespace string_literals;

namespace
{
string const kBaseURL = "https://api.mapbox.com/";
string const kDirectionsApiVersion = "v5";

string VehicleTypeToMapboxType(routing_quality::api::VehicleType type)
{
  switch (type)
  {
  case routing_quality::api::VehicleType::Car: return "mapbox/driving"s;
  }

  UNREACHABLE();
}

string LatLonsToString(vector<ms::LatLon> const & coords)
{
  ostringstream oss;
  auto const size = coords.size();
  for (size_t i = 0; i < size; ++i)
  {
    auto const & ll = coords[i];
    oss << ll.m_lon << "," << ll.m_lat;
    if (i + 1 != size)
      oss << ";";
  }

  oss << ".json";
  return UrlEncode(oss.str());
}
}  // namespace

namespace routing_quality
{
namespace api
{
namespace mapbox
{
Response MapboxApi::CalculateRoute(VehicleType type,
                                   ms::LatLon const & start, ms::LatLon const & finish)
{
  Response response;
  MapboxResponse mapboxResponse = MakeRequest(type, start, finish);

  response.m_code = mapboxResponse.m_code;
  for (auto const & route : mapboxResponse.m_routes)
  {
    api::Route apiRoute;
    apiRoute.m_eta = route.m_duration;
    apiRoute.m_distance = route.m_distance;
    for (auto const & lonlat : route.m_geometry.m_coordinates)
    {
      CHECK_EQUAL(lonlat.size(), 2, ());

      auto const lon = lonlat.front();
      auto const lat = lonlat.back();
      apiRoute.m_waypoints.emplace_back(lat, lon);
    }

    response.m_routes.emplace_back(move(apiRoute));
  }

  return response;
}

MapboxResponse MapboxApi::MakeRequest(VehicleType type,
                                      ms::LatLon const & start, ms::LatLon const & finish) const
{
  MapboxResponse mapboxResponse;
  platform::HttpClient request(GetDirectionsURL(type, start, finish));

  if (request.RunHttpRequest() && !request.WasRedirected() && request.ErrorCode() == 200)
  {
    auto const & response = request.ServerResponse();
    CHECK(!response.empty(), ());
    {
      coding::DeserializerJson des(response);
      des(mapboxResponse);
    }
  }
  else
  {
    LOG(LWARNING, (request.ErrorCode(), request.ServerResponse()));
    mapboxResponse.m_code = ResultCode::Error;
  }

  return mapboxResponse;
}

string MapboxApi::GetDirectionsURL(VehicleType type,
                                   ms::LatLon const & start, ms::LatLon const & finish) const
{
  CHECK(!GetAccessToken().empty(), ());

  ostringstream oss;
  oss << kBaseURL << "directions/" << kDirectionsApiVersion << "/"
      << VehicleTypeToMapboxType(type) << "/";
  oss << LatLonsToString({start, finish}) << "?";
  oss << "access_token=" << GetAccessToken() << "&";
  oss << "overview=simplified&"
      << "geometries=geojson";
  oss << "&alternatives=true";

  return oss.str();
}
}  // namespace mapbox
}  // namespace api
}  // namespace routing_quality
