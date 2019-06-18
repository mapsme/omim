#pragma once

#include "routing/routing_callbacks.hpp"

#include "geometry/latlon.hpp"

#include <fstream>
#include <string>
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

struct Params
{
  static void Dump(std::ofstream & output, Params const & route);
  static Params Load(std::ifstream & input);

  VehicleType m_type;
  ms::LatLon m_start;
  ms::LatLon m_finish;
};

struct Route
{
  static void Dump(std::ofstream & output, Route const & route);
  static Route Load(std::ifstream & input);

  double m_eta = 0.0;
  double m_distance = 0.0;
  std::vector<ms::LatLon> m_waypoints;
};

struct Response
{
  static std::string const kDumpExtension;

  static void Dump(std::string const & filepath, Response const & response);
  static Response Load(std::string const & filepath);

  ResultCode m_code = ResultCode::Error;
  Params m_params;
  std::vector<Route> m_routes;
};

class RoutingApiInterface
{
public:
  virtual Response CalculateRoute(Params const & params) const = 0;
  virtual uint32_t GetMaxRPS() const = 0;
  virtual ~RoutingApiInterface() = default;
};

class RoutingApi : public RoutingApiInterface
{
public:
  ~RoutingApi() override = default;

  Response CalculateRoute(Params const & params) const override;

  uint32_t GetMaxRPS() const override;

  void SetApiName(std::string const & apiName);
  void SetAccessToken(std::string const & token);

  std::string const & GetApiName() const;
  std::string const & GetAccessToken() const;

private:
  std::string m_apiName;
  std::string m_accessToken;
};

std::string DebugPrint(ResultCode const & resultCode);
ResultCode CodeFromString(std::string const & str);

std::string DebugPrint(VehicleType type);
VehicleType TypeFromString(std::string const & str);
}  // namespace api
}  // namespace routing_quality
