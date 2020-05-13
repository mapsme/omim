#include "track_generator/track_generator.hpp"

#include "routing/vehicle_mask.hpp"

#include "platform/platform.hpp"

#include "base/assert.hpp"
#include "base/logging.hpp"

#include <string>

#include "3party/gflags/src/gflags/gflags.h"

DEFINE_string(inputDir, "", "Path to kmls.");

DEFINE_string(outputDir, "", "Path to converted kmls.");

DEFINE_string(dataDir, "", "Path to MAPS.ME data directory.");

DEFINE_int32(vehicleType, 0, "Numeric value of routing::VehicleType enum. "
                             "Pedestrian by default.");

DEFINE_bool(showHelp, false, "Show help on all flags.");

int main(int argc, char ** argv)
{
  google::SetUsageMessage("The tool is used to generate more smooth track based on "
                          "waypoints from the kml. The kml has to contain points "
                          "within LineString tag. If the router can't build the route "
                          "than the specific path will be untouched. Multiple kmls into "
                          " the directory are supported.\n\n"
                          "Usage example: "
                          "./track_generator_tool -inputDir=path/to/input/ -outputDir=/path/to/output/");

  google::ParseCommandLineFlags(&argc, &argv, false /* remove_flags */);

  if (argc == 1 || FLAGS_showHelp)
  {
    google::ShowUsageWithFlags(argv[0]);
    return 0;
  }

  if (FLAGS_inputDir.empty() || FLAGS_outputDir.empty() || FLAGS_dataDir.empty())
  {
    LOG(LINFO, ("Missing required option."));
    google::ShowUsageWithFlags(argv[0]);
    return 1;
  }

  GetPlatform().SetResourceDir(FLAGS_dataDir);
  GetPlatform().SetWritableDirForTests(FLAGS_dataDir);

  track_generator_tool::GenerateTracks(FLAGS_inputDir, FLAGS_outputDir,
                                       static_cast<routing::VehicleType>(FLAGS_vehicleType));
  return 0;
}
