#include "testing/testing.hpp"

#include "generator/generator_tests_support/test_feature.hpp"
#include "generator/generator_tests_support/test_mwm_builder.hpp"

#include "routing/subway_graph.hpp"
#include "routing/subway_model.hpp"

#include "indexer/classificator_loader.hpp"
#include "indexer/data_header.hpp"
#include "indexer/index.hpp"

#include "geometry/point2d.hpp"

#include "coding/file_name_utils.hpp"

#include "platform/local_country_file.hpp"
#include "platform/platform.hpp"
#include "platform/platform_tests_support/scoped_dir.hpp"
#include "platform/platform_tests_support/scoped_file.hpp"
#include "platform/platform_tests_support/writable_dir_changer.hpp"

#include "base/logging.hpp"
#include "base/scope_guard.hpp"

#include "defines.hpp"

#include <vector>

namespace
{
using namespace generator::tests_support;
using namespace platform;
using namespace routing;
using namespace std;

// Directory name for creating test mwm and temporary files.
std::string const kTestDir = "subway_test";
// Temporary mwm name for testing.
std::string const kTestMwm = "test";

void BuildMwm(vector<TestSubway> const & ways, LocalCountryFile & country)
{
  generator::tests_support::TestMwmBuilder builder(country, feature::DataHeader::country);

  for (TestSubway const & line : ways)
    builder.Add(line);
}

UNIT_TEST(SubwayModelSmokeTest)
{
  SubwayModelFactory factory;
  shared_ptr<IVehicleModel> model = factory.GetVehicleModel();

  TEST_EQUAL(model->GetMaxSpeed(), kSpeedSubwayLineKMpH, ());
}

UNIT_TEST(SubwayGraphTest)
{
  classificator::Load();
  Platform & platform = GetPlatform();
  std::string const testDirFullPath = my::JoinFoldersToPath(platform.WritableDir(), kTestDir);

  LocalCountryFile country(testDirFullPath, CountryFile(kTestMwm), 1);
  platform::tests_support::ScopedDir testScopedDir(kTestDir);
  platform::tests_support::ScopedFile testScopedMwm(
      my::JoinFoldersToPath(kTestDir, kTestMwm) + DATA_FILE_EXTENSION);

  //  *-----*-----* Line (1)
  //  |
  // Change (2)
  //  |
  //  *-----*-----* Line (3)
  vector<TestSubway> const ways = {
      {std::vector<m2::PointD>({{0.0, 0.0}, {0.001, 0.0}, {0.002, 0.0}}), SubwayType::Line, "555555"},
      {std::vector<m2::PointD>({{0.0, 0.0}, {0.0, 0.001}}), SubwayType::Change, ""},
      {std::vector<m2::PointD>({{0.0, 0.001}, {0.001, 0.001}, {0.002, 0.001}}), SubwayType::Line, "555557"},
  };
  BuildMwm(ways, country);

  Index index;
  index.Register(country);

  shared_ptr<SubwayModelFactory> factory = make_shared<SubwayModelFactory>();
  shared_ptr<NumMwmIds> numMwmIds = make_shared<NumMwmIds>();
  numMwmIds->RegisterFile(country.GetCountryFile());

  SubwayGraph graph(factory, numMwmIds, index);
  graph.GetNearestStation({0.0, 0.0});
  // @TODO It's necessary to test using SubwayGraph methods hears.
}
}  // namespace
