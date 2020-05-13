#include "map/benchmark_tool/api.hpp"

#include "indexer/classificator_loader.hpp"
#include "indexer/data_header.hpp"
#include "indexer/features_tag.hpp"

#include <iostream>

#include "3party/gflags/src/gflags/gflags.h"

using namespace std;

DEFINE_string(input, "", "MWM file name in the data directory");
DEFINE_int32(lowS, 10, "Low processing scale");
DEFINE_int32(highS, 17, "High processing scale");
DEFINE_bool(print_scales, false, "Print geometry scales for MWM and exit");
DEFINE_string(mode, "all", "Features enumeration mode (common or all)");

int main(int argc, char ** argv)
{
  classificator::Load();

  google::SetUsageMessage("MWM benchmarking tool");
  if (argc < 2)
  {
    google::ShowUsageWithFlagsRestrict(argv[0], "main");
    return 1;
  }

  google::ParseCommandLineFlags(&argc, &argv, false);

  if (FLAGS_print_scales)
  {
    feature::DataHeader h(FLAGS_input);
    cout << "Scales with geometry: ";
    for (size_t i = 0; i < h.GetScalesCount(); ++i)
      cout << h.GetScale(i) << " ";
    cout << endl;
    return 0;
  }

  if (!FLAGS_input.empty())
  {
    if (FLAGS_mode != "all" && FLAGS_mode != "common")
    {
      google::ShowUsageWithFlagsRestrict(argv[0], "main");
      return 1;
    }

    auto const mode =
        FLAGS_mode == "all" ? FeaturesEnumerationMode::All : FeaturesEnumerationMode::Common;

    using namespace bench;

    AllResult res;
    RunFeaturesLoadingBenchmark(FLAGS_input, make_pair(FLAGS_lowS, FLAGS_highS), mode, res);

    res.Print();
  }

  return 0;
}
