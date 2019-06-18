#include "routing/routes_builder/routes_builder_tool/utils.hpp"

#include "routing/routing_quality/api/mapbox/mapbox_api.hpp"

#include "routing/vehicle_mask.hpp"

#include "platform/platform.hpp"

#include "geometry/latlon.hpp"

#include "base/assert.hpp"
#include "base/file_name_utils.hpp"
#include "base/logging.hpp"
#include "base/timer.hpp"

#include <chrono>
#include <fstream>
#include <future>
#include <iostream>
#include <thread>

using namespace routing_quality;

namespace routing
{
namespace routes_builder
{
std::vector<RoutesBuilder::Result> BuildRoutes(std::string const & routesPath,
                                               std::string const & dumpPath)
{
  CHECK(Platform::IsFileExistsByFullPath(routesPath), ("Can not find file:", routesPath));

  std::ifstream input(routesPath);
  CHECK(input.good(), ("Error during opening:", routesPath));

  std::vector<RoutesBuilder::Result> result;
  RoutesBuilder routesBuilder(std::thread::hardware_concurrency());
  ms::LatLon start;
  ms::LatLon finish;

  std::vector<std::future<RoutesBuilder::Result>> tasks;

  {
    RoutesBuilder::Params params;
    params.m_type = VehicleType::Car;

    base::ScopedLogLevelChanger changer(base::LogLevel::LERROR);
    while (input >> params.m_start.m_lat >> params.m_start.m_lon
                 >> params.m_finish.m_lat >> params.m_finish.m_lon)
    {
      tasks.emplace_back(routesBuilder.ProcessTaskAsync(params));
    }

    std::cout << "Created: " << tasks.size() << " tasks." << std::endl;
    for (size_t i = 0; i < tasks.size(); ++i)
    {
      auto & task = tasks[i];
      task.wait();

      result.emplace_back(task.get());
      if (!dumpPath.empty())
      {
        std::string const fullPath = base::JoinPath(dumpPath, std::to_string(i) + ".dump");
        RoutesBuilder::Result::Dump(result.back(), fullPath);
      }

      if (i % 5 == 0 || i + 1 == tasks.size())
        std::cout << i + 1 << " / " << tasks.size() << std::endl;
    }
  }

  return result;
}

std::vector<api::Response> BuildRoutesWithApi(std::unique_ptr<api::RoutingApi> routingApi,
                                              std::string const & routesPath,
                                              std::string const & dumpPath)
{
  std::ifstream input(routesPath);
  CHECK(input.good(), ("Error during opening:", routesPath));

  std::vector<api::Response> result;

  api::Params params;
  params.m_type = api::VehicleType::Car;

  ms::LatLon start;
  ms::LatLon finish;

  size_t rps = 0;
  base::HighResTimer timer;
  auto const getMSeconds = [&timer]() {
    double ms = timer.ElapsedNano() / 1e6;
    LOG(LDEBUG, ("Elapsed:", ms, "ms"));
    return ms;
  };

  auto const drop = [&]() {
    rps = 0;
    timer.Reset();
  };

  auto const sleepIfNeed = [&]() {
    double constexpr kMsInSecond = 1000;
    if (getMSeconds() > kMsInSecond)
    {
      drop();
      return;
    }

    ++rps;
    if (rps >= routingApi->GetMaxRPS())
    {
      LOG(LDEBUG, ("Sleep for 2 seconds."));
      std::this_thread::sleep_for(std::chrono::seconds(2));
      drop();
    }
  };

  size_t count = 0;
  while (input >> start.m_lat >> start.m_lon >> finish.m_lat >> finish.m_lon)
  {
    if (count++ % 1000 == 0)
      LOG(LINFO, ("Start build:", count, "route."));

    params.m_start = start;
    params.m_finish = finish;

    result.emplace_back(routingApi->CalculateRoute(params));

    sleepIfNeed();
  }

  for (size_t i = 0; i < result.size(); ++i)
  {
    std::string filepath =
        base::JoinPath(dumpPath, std::to_string(i) + api::Response::kDumpExtension);

    api::Response::Dump(filepath, result[i]);
  }

  return result;
}

std::unique_ptr<routing_quality::api::RoutingApi> CreateRoutingApi(std::string const & name)
{
  if (name == "mapbox")
    return std::make_unique<routing_quality::api::mapbox::MapboxApi>();

  LOG(LERROR, ("Bad api name:", name));
  UNREACHABLE();
}
}  // namespace routes_builder
}  // routing
