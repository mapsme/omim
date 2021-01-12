#include "routing/routes_builder/routes_builder.hpp"

#include "platform/platform.hpp"

#include "base/assert.hpp"
#include "base/logging.hpp"

#include <vector>
#include <string>
#include <exception>

#include "3party/jansson/myjansson.hpp"
#include "3party/gflags/src/gflags/gflags.h"

DEFINE_uint64(threads, 1, "The number of threads. one is used by default.");

DEFINE_string(data_path, "", "Data path.");
DEFINE_string(resources_path, "", "Resources path.");

DEFINE_string(config_file, "routing_time_check.json", "Routing time check config.");

DEFINE_string(output_path, "routing_time_check_result.json", "Result file path.");

DEFINE_string(mode, "check", "Tool mode (check -- test routing time, init -- make initial measure).");

namespace
{

constexpr double kDefaultDiffRate = 0.1;

struct MeasureResult
{
  double totalTimeSeconds;
  uint64_t measurementsCount;
  uint64_t errorsCount;
};

struct CheckResult
{
  std::string groupName;
  std::string status;
  std::string message;
  MeasureResult measurement;
};

struct RouteCheckpoints {
  ms::LatLon start;
  ms::LatLon finish;
};

struct ExpectedResult {
  bool hasData;
  uint64_t routesCount;
  double totalTimeSeconds;
  double diffRate;

  void Load(json_t const * config);
};

struct RoutesGroupInfo {
  std::string name;
  std::string vehicleType;
  uint32_t timeoutPerRouteSeconds;
  uint32_t launchesNumber;
  std::vector<RouteCheckpoints> routes;
  ExpectedResult expectedResult;

  void Load(json_t * routesConfig);
};

struct TimeCheckConfig {
  std::vector<RoutesGroupInfo> groups;

  void LoadFromFile(std::string const & configPath);
  void SaveToFile(std::string const & configPath) const;
  static std::string ReadConfigData(std::string const & configPath);
};

routing::VehicleType ConvertVehicleTypeFromString(std::string const & str);
MeasureResult MeasureRoutingTime(RoutesGroupInfo const & config, uint64_t threadsNumber);
void DoCheck(std::string const & configPath, std::string const & outputPath, uint64_t threadsNumber);
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
  hasData = false;
  routesCount = 0;
  totalTimeSeconds = 0.0;
  diffRate = kDefaultDiffRate;

  if (config)
  {
    hasData = true;
    routesCount = FromJSONObject<uint64_t>(config, "routes_count");
    totalTimeSeconds = FromJSONObject<double>(config, "total_time");
    diffRate = FromJSONObject<double>(config, "diff_rate");
  }
}

void RoutesGroupInfo::Load(json_t * routesConfig)
{
  FromJSONObject(routesConfig, "name", name);
  FromJSONObject(routesConfig, "vehicle_type", vehicleType);
  FromJSONObject(routesConfig, "timeout", timeoutPerRouteSeconds);
  FromJSONObject(routesConfig, "launches_number", launchesNumber);

  std::vector<json_t *> routesData;
  FromJSONObject(routesConfig, "routes", routesData);

  routes.reserve(routesData.size());

  ms::LatLon start;
  ms::LatLon finish;

  for (const auto routeData: routesData)
  {
    RouteCheckpoints checkpoints;

    FromJSONObject(routeData, "start_lat", checkpoints.start.m_lat);
    FromJSONObject(routeData, "start_lon", checkpoints.start.m_lon);
    FromJSONObject(routeData, "finish_lat", checkpoints.finish.m_lat);
    FromJSONObject(routeData, "finish_lon", checkpoints.finish.m_lon);

    routes.push_back(checkpoints);
  }

  json_t const * expectedResultConfig = nullptr;
  FromJSONObjectOptionalField(routesConfig, "expected_result", expectedResultConfig);
  expectedResult.Load(expectedResultConfig);
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

  this->groups.reserve(groups.size());
  for (const auto groupData: groups)
  {
    RoutesGroupInfo groupInfo;
    groupInfo.Load(groupData);
    this->groups.push_back(groupInfo);
  }
}

void TimeCheckConfig::SaveToFile(std::string const & configPath) const
{
  std::ostringstream jsonBuilder;
  jsonBuilder << "{\n"
      "  \"groups\": [\n";
  bool isFirstElement = true;
  for (auto const & group: groups)
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

    jsonBuilder << "      \"name\": \"" << group.name << "\",\n"
        "      \"vehicle_type\": \"" << group.vehicleType << "\",\n"
        "      \"timeout\": " << group.timeoutPerRouteSeconds << ",\n"
        "      \"launches_number\": " << group.launchesNumber << ",\n"
        "      \"routes\": [\n";
    bool isFirstRoute = true;
    for (auto const & route: group.routes)
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
      jsonBuilder << "          \"start_lat\": " << route.start.m_lat << ",\n"
          "          \"start_lon\": " << route.start.m_lon << ",\n"
          "          \"finish_lat\": " << route.finish.m_lat << ",\n"
          "          \"finish_lon\": " << route.finish.m_lon << "\n"
          "        }";
    }
    jsonBuilder << "\n      ]";

    if (group.expectedResult.hasData)
    {
      jsonBuilder << ",\n"
          "      \"expected_result\": {\n"
          "        \"routes_count\": " << group.expectedResult.routesCount << ",\n"
          "        \"total_time\": " << group.expectedResult.totalTimeSeconds << ",\n"
          "        \"diff_rate\": " << group.expectedResult.diffRate << "\n"
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
  params.m_type = ConvertVehicleTypeFromString(config.vehicleType);

  params.m_timeoutSeconds = config.timeoutPerRouteSeconds;
  params.m_launchesNumber = config.launchesNumber;

  for (const auto route: config.routes)
  {
    auto const startPoint = mercator::FromLatLon(route.start);
    auto const finishPoint = mercator::FromLatLon(route.finish);

    params.m_checkpoints = routing::Checkpoints(std::vector<m2::PointD>({startPoint, finishPoint}));
    tasks.emplace_back(routesBuilder.ProcessTaskAsync(params));
  }

  LOG_FORCE(LINFO, ("Created:", tasks.size(), "tasks, vehicle type:", params.m_type));

  MeasureResult measurement = { 0.0, 0, 0 };

  base::Timer timer;
  for (size_t i = 0; i < tasks.size(); ++i)
  {
    auto & task = tasks[i];
    task.wait();

    auto const result = task.get();
    if (result.m_code == routing::RouterResultCode::NoError)
    {
      measurement.totalTimeSeconds += result.m_buildTimeSeconds;
      measurement.measurementsCount += 1;
    }
    else
    {
      LOG_FORCE(LINFO, ("Unable to build route #", i, " result code ", result.m_code));
      measurement.errorsCount += 1;
    }
  }
  LOG_FORCE(LINFO, ("BuildRoutes() took:", timer.ElapsedSeconds(), "seconds."));

  return measurement;
}

void SaveCheckResult(std::string const & outputPath, std::vector<CheckResult> const & result) {
  std::ostringstream jsonBuilder;
  jsonBuilder << "{\n"
      "  \"results\": [\n";
  bool isFirstElement = true;
  for (auto const & group: result) {
    if (isFirstElement)
    {
      jsonBuilder << "    {\n";
      isFirstElement = false;
    }
    else
    {
      jsonBuilder << ",\n    {\n";
    }

    jsonBuilder << "      \"name\": \"" << group.groupName << "\",\n"
        "      \"status\": \"" << group.status << "\",\n"
        "      \"message\": \"" << group.message << "\",\n"
        "      \"errors_count\": " << group.measurement.errorsCount << ",\n"
        "      \"measurements_count\": " << group.measurement.measurementsCount << ",\n"
        "      \"total_time\": " << group.measurement.totalTimeSeconds << "\n"
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

  for (const auto group: config.groups)
  {
    LOG(LINFO, ("Process routes group:", group.name));

    MeasureResult measurement = MeasureRoutingTime(group, threadsNumber);
    LOG(LINFO, ("Routing result total time:", measurement.totalTimeSeconds,
        " routes build:", measurement.measurementsCount,
        " errors: ", measurement.errorsCount));

    if (!group.expectedResult.hasData)
    {
      LOG(LWARNING, ("Missing previous measurement for group:", group.name));
      result.push_back(CheckResult({ group.name, "error", "No previous measurement to check", measurement }));
      continue;
    }

    double timeDiff = (measurement.totalTimeSeconds - group.expectedResult.totalTimeSeconds)/group.expectedResult.totalTimeSeconds;

    if (measurement.errorsCount > 0)
      result.push_back(CheckResult({ group.name, "error", "Some routes can't be built", measurement }));
    else if (measurement.measurementsCount != group.expectedResult.routesCount)
      result.push_back(CheckResult({ group.name, "error", "Configuration error", measurement }));
    else if (timeDiff > group.expectedResult.diffRate)
      result.push_back(CheckResult({ group.name, "error", "Routing  become slower", measurement }));
    else if (timeDiff < -group.expectedResult.diffRate)
      result.push_back(CheckResult({ group.name, "warning", "Routing  become faster", measurement }));
    else
      result.push_back(CheckResult({ group.name, "ok", "Check passed", measurement }));
  }

  SaveCheckResult(outputPath, result);
}

void DoInitialMeasurement(std::string const & configPath, uint64_t threadsNumber)
{
  std::vector<CheckResult> result;

  TimeCheckConfig config;
  config.LoadFromFile(configPath);

  for (auto & group: config.groups)
  {
    LOG(LINFO, ("Process routes group:", group.name));

    MeasureResult measurement = MeasureRoutingTime(group, threadsNumber);
    LOG(LINFO, ("Routing result total time:", measurement.totalTimeSeconds,
        " routes build:", measurement.measurementsCount,
        " errors: ", measurement.errorsCount));

    group.expectedResult.hasData = true;
    group.expectedResult.routesCount = measurement.measurementsCount;
    group.expectedResult.totalTimeSeconds = measurement.totalTimeSeconds;
    group.expectedResult.diffRate = kDefaultDiffRate;
  }

  config.SaveToFile(configPath);
}

}  // namespace

void Main(int argc, char ** argv)
{
  google::SetUsageMessage("This tool provides routing time change check functionality.");
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
