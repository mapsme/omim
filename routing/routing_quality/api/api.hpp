#pragma once

#include "routing/routing_callbacks.hpp"

#include "geometry/latlon.hpp"

#include <vector>

namespace routing_quality
{
namespace api
{
enum class ResultCode
{
  OK,
  Error
};

enum class VehicleType
{
  Car
};

struct Route
{
  double m_eta = 0.0;
  double m_distance = 0.0;
  std::vector<ms::LatLon> m_waypoints;
};

struct Response
{
  ResultCode m_code = ResultCode::Error;
  std::vector<Route> m_routes;
};

class RoutingApiInterface
{
public:
  virtual Response CalculateRoute(VehicleType type,
                                  ms::LatLon const & start, ms::LatLon const & finish) = 0;
};

class RoutingApiBase : public RoutingApiInterface
{
public:
  Response CalculateRoute(VehicleType type,
                          ms::LatLon const & start, ms::LatLon const & finish) override;

  void SetApiName(std::string const & apiName);
  void SetAccessToken(std::string const & token);

  std::string const & GetApiName() const;
  std::string const & GetAccessToken() const;

private:
  std::string m_apiName;
  std::string m_accessToken;
};
}  // namespace api
}  // namespace routing_quality
