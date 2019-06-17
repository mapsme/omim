#include "routing/routing_quality/api/api.hpp"

namespace routing_quality
{
namespace api
{
Response RoutingApiBase::CalculateRoute(VehicleType type,
                                        ms::LatLon const & start,
                                        ms::LatLon const & finish)
{
  return {};
}

void RoutingApiBase::SetApiName(std::string const & apiName)
{
  m_apiName = apiName;
}

std::string const & RoutingApiBase::GetApiName() const
{
  return m_apiName;
}

void RoutingApiBase::SetAccessToken(std::string const & token)
{
  m_accessToken = token;
}

std::string const & RoutingApiBase::GetAccessToken() const
{
  return m_accessToken;
}

}  // namespace api
}  // namespace routing_quality
