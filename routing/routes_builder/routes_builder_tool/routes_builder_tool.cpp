#include "routing/routes_builder/routes_builder.hpp"

#include "geometry/latlon.hpp"

#include "base/assert.hpp"
#include "base/file_name_utils.hpp"
#include "base/logging.hpp"

#include "3party/gflags/src/gflags/gflags.h"

#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

DEFINE_string(routes_file, "", "Path to file with routes in format: \n\t"
                               "first_start_lat first_start_lon first_finish_lat first_finish_lon\n\t"
                               "second_start_lat second_start_lon second_finish_lat second_finish_lon\n\t"
                               "...");

DEFINE_string(dump_path, "", "Path were routes will be dumped after building."
                             "Useful for intermediate results, because routes building "
                             "is long process.");

DEFINE_string(read_dumped_from, "", "Path were dumped routes are placed. "
                                    "Reads routes data and doesn't use --routes_file options.");

DEFINE_string(data_path, "", "Path to dir with mwms.");

using namespace routing;
using namespace routes_builder;

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
    while (input >> params.m_start.m_lat >> params.m_start.m_lon >> params.m_finish.m_lat >> params.m_finish.m_lon)
      tasks.emplace_back(routesBuilder.ProcessTaskAsync(params));

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

std::vector<RoutesBuilder::Result> ReadDumpedRoutes(std::string const & dumpDirPath)
{
  std::vector<RoutesBuilder::Result> result;

  size_t i = 0;
  for (;;)
  {
    std::string const path = base::JoinPath(dumpDirPath, std::to_string(i) + ".dump");
    if (!Platform::IsFileExistsByFullPath(path))
      break;

    result.emplace_back(RoutesBuilder::Result::Load(path));
    ++i;

    if (i % 5 == 0)
      std::cout << "Read: " << i << " files" << std::endl;
  }

  return result;
}

void PrintResults(std::vector<RoutesBuilder::Result> const & results)
{
  for (auto const & result : results)
  {
    LOG(LINFO, (result.m_code, result.m_eta));
  }
}

int main(int argc, char ** argv)
{
  google::ParseCommandLineFlags(&argc, &argv, true);

  if (FLAGS_routes_file.empty() && FLAGS_read_dumped_from.empty())
  {
    std::cerr << "--routes_file or --read_dumped_from are required, type --help for more info.\n";
    return 1;
  }

  if (!FLAGS_routes_file.empty() && !FLAGS_read_dumped_from.empty())
  {
    std::cerr << "Only one option can be used: --routes_file or --read_dumped_from.";
    return 1;
  }

  if (!FLAGS_data_path.empty())
  {
    auto & pl = GetPlatform();
    pl.SetWritableDirForTests(FLAGS_data_path);
  }

  std::vector<RoutesBuilder::Result> results;
  if (!FLAGS_routes_file.empty())
    results = BuildRoutes(FLAGS_routes_file, FLAGS_dump_path);

  if (!FLAGS_read_dumped_from.empty())
    results = ReadDumpedRoutes(FLAGS_read_dumped_from);

  return 0;
}
