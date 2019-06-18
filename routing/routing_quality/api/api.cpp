#include "routing/routing_quality/api/api.hpp"

#include "base/assert.hpp"
#include "base/logging.hpp"

#include <iomanip>
#include <string>

namespace
{
char constexpr kResultCodeOK[] = "OK";
char constexpr kResultCodeError[] = "Error";

char constexpr kVehicleTypeCar[] = "Car";
}  // namespace

namespace routing_quality
{
namespace api
{

// static
void Params::Dump(std::ofstream & output, Params const & route)
{
  output << DebugPrint(route.m_type) << std::endl
         << route.m_start.m_lat << " " << route.m_start.m_lon << std::endl
         << route.m_finish.m_lat << " " << route.m_finish.m_lon << std::endl;
}

// static
Params Params::Load(std::ifstream & input)
{
  Params params;

  std::string typeString;
  std::getline(input, typeString);
  params.m_type = TypeFromString(typeString);

  input >> params.m_start.m_lat >> params.m_start.m_lon;
  input >> params.m_finish.m_lat >> params.m_finish.m_lon;

  return params;
}

// static
void Route::Dump(std::ofstream & output, Route const & route)
{
  output << route.m_eta << " " << route.m_distance << std::endl;

  output << route.m_waypoints.size() << std::endl;
  for (auto const & latlon : route.m_waypoints)
    output << latlon.m_lat << " " << latlon.m_lon << std::endl;
}

// static
Route Route::Load(std::ifstream & input)
{
  Route route;

  input >> route.m_eta >> route.m_distance;

  size_t n;
  input >> n;
  route.m_waypoints.resize(n);
  ms::LatLon latlon;
  for (size_t i = 0; i < n; ++i)
  {
    input >> latlon.m_lat >> latlon.m_lon;
    route.m_waypoints[i] = latlon;
  }

  return route;
}

// static
std::string const Response::kDumpExtension = ".api.dump";

// static
void Response::Dump(std::string const & filepath, Response const & response)
{
  std::ofstream output(filepath);
  CHECK(output.good(), ("Error during opening:", filepath));

  output << std::setprecision(20);

  output << DebugPrint(response.m_code) << std::endl;

  Params::Dump(output, response.m_params);

  output << response.m_routes.size() << std::endl;
  for (auto const & route : response.m_routes)
    Route::Dump(output, route);
}

// static
Response Response::Load(std::string const & filepath)
{
  std::ifstream input(filepath);
  CHECK(input.good(), ("Error during opening:", filepath));

  Response response;

  std::string codeString;
  std::getline(input, codeString);
  response.m_code = CodeFromString(codeString);

  response.m_params = Params::Load(input);

  size_t n;
  input >> n;
  response.m_routes.resize(n);
  for (size_t i = 0; i < n; ++i)
    response.m_routes[i] = Route::Load(input);

  return response;
}

Response RoutingApi::CalculateRoute(Params const & params) const
{
  return {};
}

uint32_t RoutingApi::GetMaxRPS() const
{
  return 0;
}

void RoutingApi::SetApiName(std::string const & apiName)
{
  m_apiName = apiName;
}

std::string const & RoutingApi::GetApiName() const
{
  return m_apiName;
}

void RoutingApi::SetAccessToken(std::string const & token)
{
  m_accessToken = token;
}

std::string const & RoutingApi::GetAccessToken() const
{
  return m_accessToken;
}

std::string DebugPrint(ResultCode const & resultCode)
{
  switch (resultCode)
  {
  case ResultCode::OK: return kResultCodeOK;
  case ResultCode::Error: return kResultCodeError;
  }

  UNREACHABLE();
}

ResultCode CodeFromString(std::string const & str)
{
  if (str == kResultCodeOK)
    return ResultCode::OK;

  if (str == kResultCodeError)
    return ResultCode::Error;

  LOG(LERROR, ("Bad result code:", str));
  UNREACHABLE();
}

std::string DebugPrint(VehicleType type)
{
  switch (type)
  {
  case VehicleType::Car: return kVehicleTypeCar;
  }

  UNREACHABLE();
}

VehicleType TypeFromString(std::string const & str)
{
  if (str == kVehicleTypeCar)
    return VehicleType::Car;

  LOG(LERROR, ("Bad type:", str));
  UNREACHABLE();
}
}  // namespace api
}  // namespace routing_quality
