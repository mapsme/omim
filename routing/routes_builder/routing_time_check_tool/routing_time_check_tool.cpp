#include "routing/routes_builder/routes_builder.hpp"

#include "platform/platform.hpp"

#include "geometry/latlon.hpp"

#include "base/assert.hpp"
#include "base/logging.hpp"

#include <cstdint>
#include <exception>
#include <sstream>
#include <string>
#include <vector>

#include "3party/gflags/src/gflags/gflags.h"
#include "3party/jansson/myjansson.hpp"

DEFINE_uint64(threads, 2, "The number of threads. one is used by default.");

DEFINE_string(data_path, "", "Data path.");
DEFINE_string(resources_path, "", "Resources path.");

DEFINE_string(config_file, "routing_time_check.json", "Routing time check config.");

DEFINE_string(output_path, "routing_time_check_result.json", "Result file path.");

DEFINE_string(mode, "check",
              "Tool mode (check -- test routing time, init -- make initial measure).");

namespace
{
constexpr double kDefaultDiffRate = 0.1;

struct MeasureResult
{
  double m_totalTimeSeconds;
  uint64_t m_measurementsCount;
  uint64_t m_errorsCount;

  MeasureResult() : m_totalTimeSeconds(0.0), m_measurementsCount(0), m_errorsCount(0) {}
};

struct CheckResult
{
  std::string m_groupName;
  std::string m_status;
  std::string m_message;
  MeasureResult m_measurement;
};

struct RouteCheckpoints
{
  ms::LatLon m_start;
  ms::LatLon m_finish;
};

struct ExpectedResult
{
  bool m_hasData;
  uint64_t m_routesCount;
  double m_totalTimeSeconds;
  double m_diffRate;

  void Load(json_t const * config);
};

struct RoutesGroupInfo
{
  std::string m_name;
  std::string m_vehicleType;
  uint32_t m_timeoutPerRouteSeconds;
  uint32_t m_launchesNumber;
  std::vector<RouteCheckpoints> m_routes;
  ExpectedResult m_expectedResult;

  void Load(json_t * routesConfig);
};

struct TimeCheckConfig
{
  std::vector<RoutesGroupInfo> m_groups;

  void LoadFromFile(std::string const & configPath);
  void SaveToFile(std::string const & configPath) const;
  static std::string ReadConfigData(std::string const & configPath);
};

routing::VehicleType ConvertVehicleTypeFromString(std::string const & str);
MeasureResult MeasureRoutingTime(RoutesGroupInfo const & config, uint64_t threadsNumber);
void DoCheck(std::string const & configPath, std::string const & outputPath,
             uint64_t threadsNumber);
void DoInitialMeasurement(std::string const & configPath, uint64_t threadsNumber);
void SaveCheckResult(std::string const & outputPath, std::vector<CheckResult> const & result);

routing::VehicleType ConvertVehicleTypeFromString(std::string const & str)
{
  if (str == "car")
    return routing::VehicleType::Car;
  if (str == "pedestrian")
    return routing::VehicleType::Pedestrian;
  if (str == "bicycle")
    return routing::VehicleType::Bicycle;
  if (str == "transit")
    return routing::VehicleType::Transit;

  CHECK(false, ("Unknown vehicle type:", str));
  UNREACHABLE();
}

void ExpectedResult::Load(json_t const * config)
{
  m_hasData = false;
  m_routesCount = 0;
  m_totalTimeSeconds = 0.0;
  m_diffRate = kDefaultDiffRate;

  if (config)
  {
    m_hasData = true;
    m_routesCount = FromJSONObject<uint64_t>(config, "routes_count");
    m_totalTimeSeconds = FromJSONObject<double>(config, "total_time");
    m_diffRate = FromJSONObject<double>(config, "diff_rate");
  }
}

void RoutesGroupInfo::Load(json_t * routesConfig)
{
  FromJSONObject(routesConfig, "name", m_name);
  FromJSONObject(routesConfig, "vehicle_type", m_vehicleType);
  FromJSONObject(routesConfig, "timeout", m_timeoutPerRouteSeconds);
  FromJSONObject(routesConfig, "launches_number", m_launchesNumber);

  std::vector<json_t *> routesData;
  FromJSONObject(routesConfig, "routes", routesData);

  m_routes.reserve(routesData.size());

  ms::LatLon start;
  ms::LatLon finish;

  for (const auto routeData : routesData)
  {
    RouteCheckpoints checkpoints;

    FromJSONObject(routeData, "start_lat", checkpoints.m_start.m_lat);
    FromJSONObject(routeData, "start_lon", checkpoints.m_start.m_lon);
    FromJSONObject(routeData, "finish_lat", checkpoints.m_finish.m_lat);
    FromJSONObject(routeData, "finish_lon", checkpoints.m_finish.m_lon);

    m_routes.push_back(checkpoints);
  }

  json_t const * expectedResultConfig = nullptr;
  FromJSONObjectOptionalField(routesConfig, "expected_result", expectedResultConfig);
  m_expectedResult.Load(expectedResultConfig);
}

std::string TimeCheckConfig::ReadConfigData(std::string const & configPath)
{
  auto configReader = GetPlatform().GetReader(configPath);
  std::string configData;
  configReader->ReadAsString(configData);
  return configData;
}

void TimeCheckConfig::LoadFromFile(std::string const & configPath)
{
  std::string configContent = ReadConfigData(configPath);
  base::Json configData(configContent.data());

  std::vector<json_t *> groups;
  FromJSONObject(configData.get(), "groups", groups);

  m_groups.reserve(groups.size());
  for (const auto groupData : groups)
  {
    RoutesGroupInfo groupInfo;
    groupInfo.Load(groupData);
    m_groups.push_back(std::move(groupInfo));
  }
}

void TimeCheckConfig::SaveToFile(std::string const & configPath) const
{
  std::ostringstream jsonBuilder;
  jsonBuilder << "{\n"
                 "  \"groups\": [\n";
  bool isFirstElement = true;
  for (auto const & group : m_groups)
  {
    if (isFirstElement)
    {
      jsonBuilder << "    {\n";
      isFirstElement = false;
    }
    else
    {
      jsonBuilder << ",\n    {\n";
    }

    jsonBuilder << "      \"name\": \"" << group.m_name
                << "\",\n"
                   "      \"vehicle_type\": \""
                << group.m_vehicleType
                << "\",\n"
                   "      \"timeout\": "
                << group.m_timeoutPerRouteSeconds
                << ",\n"
                   "      \"launches_number\": "
                << group.m_launchesNumber
                << ",\n"
                   "      \"routes\": [\n";
    bool isFirstRoute = true;
    for (auto const & route : group.m_routes)
    {
      if (isFirstRoute)
      {
        jsonBuilder << "        {\n";
        isFirstRoute = false;
      }
      else
      {
        jsonBuilder << ",\n        {\n";
      }
      jsonBuilder << "          \"start_lat\": " << route.m_start.m_lat
                  << ",\n"
                     "          \"start_lon\": "
                  << route.m_start.m_lon
                  << ",\n"
                     "          \"finish_lat\": "
                  << route.m_finish.m_lat
                  << ",\n"
                     "          \"finish_lon\": "
                  << route.m_finish.m_lon
                  << "\n"
                     "        }";
    }
    jsonBuilder << "\n      ]";

    if (group.m_expectedResult.m_hasData)
    {
      jsonBuilder << ",\n"
                     "      \"expected_result\": {\n"
                     "        \"routes_count\": "
                  << group.m_expectedResult.m_routesCount
                  << ",\n"
                     "        \"total_time\": "
                  << group.m_expectedResult.m_totalTimeSeconds
                  << ",\n"
                     "        \"diff_rate\": "
                  << group.m_expectedResult.m_diffRate
                  << "\n"
                     "      }\n"
                     "    }";
    }
    else
    {
      jsonBuilder << "\n"
                     "    }";
    }
  };

  jsonBuilder << "\n  ]\n"
                 "}\n";

  std::string jsonData(jsonBuilder.str());
  FileWriter resultWriter(configPath);
  resultWriter.Write(jsonData.data(), jsonData.size());
  resultWriter.Flush();
}

MeasureResult MeasureRoutingTime(RoutesGroupInfo const & config, uint64_t threadsNumber)
{
  routing::routes_builder::RoutesBuilder routesBuilder(threadsNumber);

  std::vector<std::future<routing::routes_builder::RoutesBuilder::Result>> tasks;

  routing::routes_builder::RoutesBuilder::Params params;
  params.m_type = ConvertVehicleTypeFromString(config.m_vehicleType);

  params.m_timeoutSeconds = config.m_timeoutPerRouteSeconds;
  params.m_launchesNumber = config.m_launchesNumber;

  for (const auto route : config.m_routes)
  {
    auto const startPoint = mercator::FromLatLon(route.m_start);
    auto const finishPoint = mercator::FromLatLon(route.m_finish);

    params.m_checkpoints = routing::Checkpoints(std::vector<m2::PointD>({startPoint, finishPoint}));
    tasks.emplace_back(routesBuilder.ProcessTaskAsync(params));
  }

  LOG_FORCE(LINFO, ("Created:", tasks.size(), "tasks, vehicle type:", params.m_type));

  MeasureResult measurement;

  base::Timer timer;
  for (size_t i = 0; i < tasks.size(); ++i)
  {
    auto & task = tasks[i];
    task.wait();

    auto const result = task.get();
    if (result.m_code == routing::RouterResultCode::NoError)
    {
      measurement.m_totalTimeSeconds += result.m_buildTimeSeconds;
      measurement.m_measurementsCount += 1;
    }
    else
    {
      LOG_FORCE(LINFO, ("Unable to build route #", i, " result code ", result.m_code));
      measurement.m_errorsCount += 1;
    }
  }
  LOG_FORCE(LINFO, ("BuildRoutes() took:", timer.ElapsedSeconds(), "seconds."));

  return measurement;
}

void SaveCheckResult(std::string const & outputPath, std::vector<CheckResult> const & result)
{
  std::ostringstream jsonBuilder;
  jsonBuilder << "{\n"
                 "  \"results\": [\n";
  bool isFirstElement = true;
  for (auto const & group : result)
  {
    if (isFirstElement)
    {
      jsonBuilder << "    {\n";
      isFirstElement = false;
    }
    else
    {
      jsonBuilder << ",\n    {\n";
    }

    jsonBuilder << "      \"name\": \"" << group.m_groupName
                << "\",\n"
                   "      \"status\": \""
                << group.m_status
                << "\",\n"
                   "      \"message\": \""
                << group.m_message
                << "\",\n"
                   "      \"errors_count\": "
                << group.m_measurement.m_errorsCount
                << ",\n"
                   "      \"measurements_count\": "
                << group.m_measurement.m_measurementsCount
                << ",\n"
                   "      \"total_time\": "
                << group.m_measurement.m_totalTimeSeconds
                << "\n"
                   "    }";
  }
  jsonBuilder << "\n  ]\n"
                 "}\n";

  std::string jsonData(jsonBuilder.str());
  FileWriter resultWriter(outputPath);
  resultWriter.Write(jsonData.data(), jsonData.size());
  resultWriter.Flush();
}

void DoCheck(std::string const & configPath, std::string const & outputPath, uint64_t threadsNumber)
{
  std::vector<CheckResult> result;

  TimeCheckConfig config;
  config.LoadFromFile(configPath);

  for (const auto group : config.m_groups)
  {
    LOG(LINFO, ("Process routes group:", group.m_name));

    MeasureResult measurement = MeasureRoutingTime(group, threadsNumber);
    LOG(LINFO, ("Routing result total time:", measurement.m_totalTimeSeconds, " routes build:",
                measurement.m_measurementsCount, " errors: ", measurement.m_errorsCount));

    if (!group.m_expectedResult.m_hasData)
    {
      LOG(LWARNING, ("Missing previous measurement for group:", group.m_name));
      result.push_back(
          CheckResult({group.m_name, "error", "No previous measurement to check", measurement}));
      continue;
    }

    double timeDiff = (measurement.m_totalTimeSeconds - group.m_expectedResult.m_totalTimeSeconds) /
                      group.m_expectedResult.m_totalTimeSeconds;

    if (measurement.m_errorsCount > 0)
      result.push_back(
          CheckResult({group.m_name, "error", "Some routes can't be built", measurement}));
    else if (measurement.m_measurementsCount != group.m_expectedResult.m_routesCount)
      result.push_back(CheckResult({group.m_name, "error", "Configuration error", measurement}));
    else if (timeDiff > group.m_expectedResult.m_diffRate)
      result.push_back(CheckResult({group.m_name, "error", "Routing  became slower", measurement}));
    else if (timeDiff < -group.m_expectedResult.m_diffRate)
      result.push_back(
          CheckResult({group.m_name, "warning", "Routing  became faster", measurement}));
    else
      result.push_back(CheckResult({group.m_name, "ok", "Check passed", measurement}));
  }

  SaveCheckResult(outputPath, result);
}

void DoInitialMeasurement(std::string const & configPath, uint64_t threadsNumber)
{
  std::vector<CheckResult> result;

  TimeCheckConfig config;
  config.LoadFromFile(configPath);

  for (auto & group : config.m_groups)
  {
    LOG(LINFO, ("Process routes group:", group.m_name));

    MeasureResult measurement = MeasureRoutingTime(group, threadsNumber);
    LOG(LINFO, ("Routing result total time:", measurement.m_totalTimeSeconds, " routes build:",
                measurement.m_measurementsCount, " errors: ", measurement.m_errorsCount));

    group.m_expectedResult.m_hasData = true;
    group.m_expectedResult.m_routesCount = measurement.m_measurementsCount;
    group.m_expectedResult.m_totalTimeSeconds = measurement.m_totalTimeSeconds;
    group.m_expectedResult.m_diffRate = kDefaultDiffRate;
  }

  config.SaveToFile(configPath);
}
}  // namespace

void Main(int argc, char ** argv)
{
  google::SetUsageMessage("This tool provides routing time check functionality.");
  google::ParseCommandLineFlags(&argc, &argv, true);

  if (!FLAGS_data_path.empty())
    GetPlatform().SetWritableDirForTests(FLAGS_data_path);

  if (!FLAGS_resources_path.empty())
    GetPlatform().SetResourceDir(FLAGS_resources_path);

  std::string configPath = GetPlatform().ReadPathForFile(FLAGS_config_file);

  uint64_t threadsNumber = FLAGS_threads;

  if (FLAGS_mode == "check")
    DoCheck(configPath, FLAGS_output_path, threadsNumber);
  else if (FLAGS_mode == "init")
    DoInitialMeasurement(configPath, threadsNumber);
  else
    LOG(LERROR, ("Unknown mode:", FLAGS_mode));
}

int main(int argc, char ** argv)
{
  try
  {
    Main(argc, argv);
  }
  catch (RootException const & e)
  {
    LOG(LERROR, ("Core exception:", e.Msg()));
  }
  catch (std::exception const & e)
  {
    LOG(LERROR, ("Std exception:", e.what()));
  }
  catch (...)
  {
    LOG(LERROR, ("Unknown exception."));
  }

  LOG(LINFO, ("Done."));
  return 0;
}
