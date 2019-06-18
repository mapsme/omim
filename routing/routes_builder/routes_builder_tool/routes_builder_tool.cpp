#include "routing/routes_builder/routes_builder_tool/utils.hpp"

#include "routing/routing_quality/api/mapbox/mapbox_api.hpp"

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

DEFINE_string(api_name, "", "Api name, current options: mapbox");
DEFINE_string(api_token, "", "Token for api_name.");

using namespace routing;
using namespace routes_builder;
using namespace routing_quality;

bool IsLocalBuild()
{
  return !FLAGS_routes_file.empty() && FLAGS_api_name.empty() && FLAGS_api_token.empty();
}

bool IsApiBuild()
{
  return !FLAGS_routes_file.empty() && !FLAGS_api_name.empty() && !FLAGS_api_token.empty();
}

int main(int argc, char ** argv)
{
  google::ParseCommandLineFlags(&argc, &argv, true);

  CHECK(!FLAGS_routes_file.empty(),
        ("\n\n\t--routes_file is required.",
         "\n\nType --help for usage."));

  if (!FLAGS_data_path.empty())
  {
    auto & pl = GetPlatform();
    pl.SetWritableDirForTests(FLAGS_data_path);
  }

  CHECK(IsLocalBuild() || IsApiBuild(),
        ("\n\n\t--routes_file empty is:", FLAGS_routes_file.empty(),
         "\n\t--api_name empty is:", FLAGS_api_name.empty(),
         "\n\t--api_token empty is:", FLAGS_api_token.empty(),
         "\n\nType --help for usage."));

  CHECK(!FLAGS_dump_path.empty(),
        ("\n\n\t--dump_path is empty. It makes no sense to run this tool. No result will be saved.",
         "\n\nType --help for usage."));

  std::vector<RoutesBuilder::Result> results;
  if (IsLocalBuild())
    results = BuildRoutes(FLAGS_routes_file, FLAGS_dump_path);

  if (IsApiBuild())
  {
    auto api = CreateRoutingApi(FLAGS_api_name);
    api->SetAccessToken(FLAGS_api_token);

    BuildRoutesWithApi(std::move(api), FLAGS_routes_file, FLAGS_dump_path);
  }

  return 0;
}
