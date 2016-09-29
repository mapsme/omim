#include "testing/testing.hpp"

#include "generator/edge_index_generator.hpp"
#include "generator/generator_tests_support/test_feature.hpp"
#include "generator/generator_tests_support/test_mwm_builder.hpp"

#include "routing/features_road_graph.hpp"
#include "routing/pedestrian_model.hpp"

#include "indexer/edge_index_loader.hpp"
#include "indexer/feature_altitude.hpp"
#include "indexer/feature_edge_index.hpp"
#include "indexer/feature_processor.hpp"
#include "indexer/index.hpp"

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

namespace
{
// These tests generate mwms with edge index sections and then check if edge index
// in the mwms are correct.

// Directory name for creating test mwm and temporary files.
string const kTestDir = "edge_index_generation_test";
// Temporary mwm name for testing.
string const kTestMwm = "test";
double constexpr kEpsilon = 1e-5;

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

void SortEdges(IRoadGraph::TEdgeVector & edges1, IRoadGraph::TEdgeVector & edges2)
{
  sort(edges1.begin(), edges1.end());
  sort(edges2.begin(), edges2.end());
}

void TestEqualEdges(IRoadGraph::TEdgeVector const & edgesFormIndex,
                    IRoadGraph::TEdgeVector const & edgesFormGeometry)
{
  TEST_EQUAL(edgesFormIndex.size(), edgesFormGeometry.size(), ());
  for (size_t j = 0; j < edgesFormIndex.size(); ++j)
  {
    Edge const & fromIndex = edgesFormIndex[j];
    Edge const & fromGeom = edgesFormGeometry[j];
    TEST(m2::AlmostEqualAbs(fromIndex.GetStartJunction().GetPoint(),
                            fromGeom.GetStartJunction().GetPoint(), kEpsilon),
         (fromIndex.GetStartJunction().GetPoint(), fromGeom.GetStartJunction().GetPoint()));
    TEST(m2::AlmostEqualAbs(fromIndex.GetEndJunction().GetPoint(),
                            fromGeom.GetEndJunction().GetPoint(), kEpsilon),
         (fromIndex.GetEndJunction().GetPoint(), fromGeom.GetEndJunction().GetPoint()));
    TEST_EQUAL(fromIndex.IsForward(), fromGeom.IsForward(), ());
    TEST_EQUAL(fromIndex.GetFeatureId().m_index, fromGeom.GetFeatureId().m_index, ());
    TEST_EQUAL(fromIndex.GetSegId(), fromGeom.GetSegId(), ());
  }
}

/// \note This method could be used to check correctness of edge index section
/// of any mwm which has one.
// @TODO It's worth writing an integration test on a real mwm based on this method.
void TestEdgeIndex(MwmValue const & mwmValue, MwmSet::MwmId const & mwmId,
                   string const & dir, string const & country)
{
  feature::EdgeIndexLoader loader(mwmValue, mwmId);
  TEST(loader.HasEdgeIndex(), ());

  string const mwmPath = my::JoinFoldersToPath(dir, country + DATA_FILE_EXTENSION);
  Index index;
  LocalCountryFile localCountryFile(dir, CountryFile(country), 1 /* version */);
  index.RegisterMap(localCountryFile);
  FeaturesRoadGraph featureRoadGraph(index, IRoadGraph::Mode::IgnoreOnewayTag,
                                     make_unique<PedestrianModelFactory>());

  auto processor = [&loader, &featureRoadGraph](FeatureType const & f, uint32_t const & id)
  {
    f.ParseGeometry(FeatureType::BEST_GEOMETRY);
    size_t const pointsCount = f.GetPointsCount();
    for (size_t i = 0; i < pointsCount; ++i)
    {
      // Outgoing edges.
      {
        // Edges from edge index section.
        Junction const junction(f.GetPoint(i), kDefaultAltitudeMeters);
        IRoadGraph::TEdgeVector edgesFormIndex;
        loader.GetOutgoingEdges(junction, edgesFormIndex);

        // Edges from geometry.
        IRoadGraph::TEdgeVector edgesFormGeometry;
        featureRoadGraph.GetOutgoingEdges(junction, edgesFormGeometry);

        // Comparing outgoing edges for edge index section and from geometry.
        SortEdges(edgesFormIndex, edgesFormGeometry);
        TestEqualEdges(edgesFormIndex, edgesFormGeometry);
      }

      // Ingoing edges.
      {
        // Edges from edge index section.
        Junction const junction(f.GetPoint(i), kDefaultAltitudeMeters);
        IRoadGraph::TEdgeVector edgesFormIndex;
        loader.GetIngoingEdges(junction, edgesFormIndex);

        // Edges from geometry.
        IRoadGraph::TEdgeVector edgesFormGeometry;
        featureRoadGraph.GetIngoingEdges(junction, edgesFormGeometry);

        // Comparing ingoing edges for edge index section and from geometry.
        SortEdges(edgesFormIndex, edgesFormGeometry);
        TestEqualEdges(edgesFormIndex, edgesFormGeometry);
      }
    }
  };
  feature::ForEachFromDat(mwmPath, processor);
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
  Index index;
  auto const regResult = index.RegisterMap(country);
  TEST_EQUAL(regResult.second, MwmSet::RegResult::Success, ());

  MwmSet::MwmHandle mwmHandle = index.GetMwmHandleById(regResult.first);
  TEST(mwmHandle.IsAlive(), ());

  TestEdgeIndex(*mwmHandle.GetValue<MwmValue>(), mwmHandle.GetId(), testDirFullPath,
                kTestMwm);
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
