#include "testing/testing.hpp"

#include "generator/generator_tests_support/test_mwm_consistency.hpp"

#include "generator/edge_index_generator.hpp"
#include "generator/generator_tests_support/test_feature.hpp"
#include "generator/generator_tests_support/test_mwm_builder.hpp"

#include "routing/features_road_graph.hpp"
#include "routing/pedestrian_model.hpp"

#include "indexer/edge_index_loader.hpp"
#include "indexer/feature_altitude.hpp"
#include "indexer/feature_edge_index.hpp"
#include "indexer/feature_processor.hpp"

#include "geometry/point2d.hpp"

#include "coding/file_name_utils.hpp"

#include "platform/local_country_file.hpp"
#include "platform/platform.hpp"
#include "platform/platform_tests_support/scoped_dir.hpp"
#include "platform/platform_tests_support/scoped_file.hpp"
#include "platform/platform_tests_support/writable_dir_changer.hpp"

#include "std/string.hpp"
#include "std/vector.hpp"

using namespace feature;
using namespace generator;
using namespace platform;
using namespace routing;
using namespace generator::tests_support;

namespace
{
// These tests generate mwms with edge index sections and then check if edge index sections
// in the mwms are correct.

// Directory name for creating test mwm and temporary files.
string const kTestDir = "edge_index_generation_test";
// Temporary mwm name for testing.
string const kTestMwm = "test";

using TPoint2DList = vector<m2::PointD>;

TPoint2DList const kRoad1 = {{0.0, -1.0}, {0.0, 0.0}, {0.0, 1.0}};
TPoint2DList const kRoad2 = {{0.0, 1.0}, {5.0, 1.0}, {10.0, 1.0}};
TPoint2DList const kRoad3 = {{10.0, 1.0}, {15.0, 6.0}, {20.0, 11.0}};
TPoint2DList const kRoad4 = {{-10.0, 1.0}, {-20.0, 6.0}, {-20.0, -11.0}};

void BuildMwmWithoutEdgeIndex(vector<TPoint2DList> const & roads, LocalCountryFile & country)
{
  generator::tests_support::TestMwmBuilder builder(country, feature::DataHeader::country);

  for (TPoint2DList const & geom : roads)
    builder.Add(generator::tests_support::TestStreet(geom, string(), string()));
}

void TestEdgeIndexBuilding(vector<TPoint2DList> const & roads)
{
  Platform & platform = GetPlatform();
  string const testDirFullPath = my::JoinFoldersToPath(platform.WritableDir(), kTestDir);

  // Building mwm without altitude section.
  LocalCountryFile country(testDirFullPath, CountryFile(kTestMwm), 1);
  platform::tests_support::ScopedDir testScopedDir(kTestDir);
  platform::tests_support::ScopedFile testScopedMwm(country.GetPath(MapOptions::Map));

  BuildMwmWithoutEdgeIndex(roads, country);

  // Adding edge index section to mwm.
  BuildOutgoingEdgeIndex(testDirFullPath, kTestMwm);

  // Reading from mwm and testing index section information.
  TestEdgeIndex(testDirFullPath, kTestMwm);
}

UNIT_TEST(EdgeIndexGenerationTest_ThreeRoads)
{
  vector<TPoint2DList> const roads = {kRoad1, kRoad2, kRoad3};
  TestEdgeIndexBuilding(roads);
}

UNIT_TEST(EdgeIndexGenerationTest_ThreeUnorderedRoads)
{
  vector<TPoint2DList> const roads = {kRoad2, kRoad1, kRoad3};
  TestEdgeIndexBuilding(roads);
}

UNIT_TEST(EdgeIndexGenerationTest_TwoRoads)
{
  vector<TPoint2DList> const roads = {kRoad1, kRoad3};
  TestEdgeIndexBuilding(roads);
}

UNIT_TEST(EdgeIndexGenerationTest_FourRoads)
{
  vector<TPoint2DList> const roads = {kRoad1, kRoad2, kRoad3, kRoad4};
  TestEdgeIndexBuilding(roads);
}

UNIT_TEST(EdgeIndexGenerationTest_FiveTheSameRoads)
{
  vector<TPoint2DList> const roads = {kRoad1, kRoad1, kRoad1, kRoad1, kRoad1};
  TestEdgeIndexBuilding(roads);
}
}  // namespace
