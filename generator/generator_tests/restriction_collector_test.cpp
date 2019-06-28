#include "testing/testing.hpp"

#include "generator/generator_tests_support/routing_helpers.hpp"
#include "generator/restriction_collector.hpp"

#include "routing/restrictions_serialization.hpp"

#include "indexer/classificator_loader.hpp"

#include "platform/platform_tests_support/scoped_dir.hpp"
#include "platform/platform_tests_support/scoped_file.hpp"

#include "platform/platform.hpp"

#include "base/file_name_utils.hpp"
#include "base/geo_object_id.hpp"
#include "base/stl_helpers.hpp"

#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace generator;
using namespace platform;
using namespace platform::tests_support;

namespace routing
{
std::string const kTestDir = "test-restrictions";
std::string const kOsmIdsToFeatureIdsName = "osm_ids_to_feature_ids" OSM2FEATURE_FILE_EXTENSION;

// 2                 *
//                ↗     ↘
//              F5        F4
//            ↗              ↘                             Finish
// 1         *                 *<- F3 ->*-> F8 -> *-> F10 -> *
//            ↖                         ↑       ↗
//              F6                      F2   F9
//         Start   ↖                    ↑  ↗
// 0         *-> F7 ->*-> F0 ->*-> F1 ->*
//          -1        0        1        2         3          4
//
std::unique_ptr<IndexGraph> BuildTwoCubeGraph()
{
  classificator::Load();
  std::unique_ptr<TestGeometryLoader> loader = std::make_unique<TestGeometryLoader>();
  loader->AddRoad(0 /* feature id */, true /* one way */, 1.0 /* speed */,
                  RoadGeometry::Points({{0.0, 0.0}, {1.0, 0.0}}));
  loader->AddRoad(1 /* feature id */, true /* one way */, 1.0 /* speed */,
                  RoadGeometry::Points({{1.0, 0.0}, {2.0, 0.0}}));
  loader->AddRoad(2 /* feature id */, true /* one way */, 1.0 /* speed */,
                  RoadGeometry::Points({{2.0, 0.0}, {2.0, 1.0}}));
  loader->AddRoad(3 /* feature id */, false /* one way */, 1.0 /* speed */,
                  RoadGeometry::Points({{1.0, 1.0}, {2.0, 1.0}}));
  loader->AddRoad(4 /* feature id */, true /* one way */, 1.0 /* speed */,
                  RoadGeometry::Points({{0.0, 2.0}, {1.0, 1.0}}));
  loader->AddRoad(5 /* feature id */, true /* one way */, 1.0 /* speed */,
                  RoadGeometry::Points({{-1.0, 1.0}, {0.0, 2.0}}));
  loader->AddRoad(6 /* feature id */, true /* one way */, 1.0 /* speed */,
                  RoadGeometry::Points({{0.0, 0.0}, {-1.0, 1.0}}));
  loader->AddRoad(7 /* feature id */, true /* one way */, 1.0 /* speed */,
                  RoadGeometry::Points({{-1.0, 0.0}, {0.0, 0.0}}));
  loader->AddRoad(8 /* feature id */, true /* one way */, 1.0 /* speed */,
                  RoadGeometry::Points({{2.0, 1.0}, {3.0, 1.0}}));
  loader->AddRoad(9 /* feature id */, true /* one way */, 1.0 /* speed */,
                  RoadGeometry::Points({{2.0, 0.0}, {3.0, 1.0}}));
  loader->AddRoad(10 /* feature id */, true /* one way */, 1.0 /* speed */,
                  RoadGeometry::Points({{3.0, 1.0}, {4.0, 1.0}}));

  std::vector<Joint> const joints = {
      // {{/* feature id */, /* point id */}, ... }
      MakeJoint({{7, 0}}),                  /* joint at point (-1, 0) */
      MakeJoint({{0, 0}, {6, 0}, {7, 1}}),  /* joint at point (0, 0) */
      MakeJoint({{0, 1}, {1, 0}}),          /* joint at point (1, 0) */
      MakeJoint({{1, 1}, {2, 0}, {9, 0}}),  /* joint at point (2, 0) */
      MakeJoint({{2, 1}, {3, 1}, {8, 0}}),  /* joint at point (2, 1) */
      MakeJoint({{3, 0}, {4, 1}}),          /* joint at point (1, 1) */
      MakeJoint({{5, 1}, {4, 0}}),          /* joint at point (0, 2) */
      MakeJoint({{6, 1}, {5, 0}}),          /* joint at point (-1, 1) */
      MakeJoint({{8, 1}, {9, 1}, {10, 0}}), /* joint at point (3, 1) */
      MakeJoint({{10, 1}})                  /* joint at point (4, 1) */
  };

  traffic::TrafficCache const trafficCache;
  std::shared_ptr<EdgeEstimator> estimator = CreateEstimatorForCar(trafficCache);

  return BuildIndexGraph(std::move(loader), estimator, joints);
}

std::string const kosmIdsToFeatureIdsContentForTwoCubeGraph =
    R"(0, 0
       1, 1
       2, 2
       3, 3
       4, 4
       5, 5
       6, 6
       7, 7
       8, 8
       9, 9
       10, 10)";

class TestRestrictionCollector
{
public:
  TestRestrictionCollector() : m_scopedDir(kTestDir)
  {
    // Creating osm ids to feature ids mapping.
    std::string const mappingRelativePath = base::JoinPath(kTestDir, kOsmIdsToFeatureIdsName);

    m_scopedFile = std::make_shared<ScopedFile>(mappingRelativePath, ScopedFile::Mode::Create);
    m_osmIdsToFeatureIdFullPath = m_scopedFile->GetFullPath();

    ReEncodeOsmIdsToFeatureIdsMapping(kosmIdsToFeatureIdsContentForTwoCubeGraph,
                                      m_osmIdsToFeatureIdFullPath);
  }

  void ValidCase()
  {
    RestrictionCollector restrictionCollector(m_osmIdsToFeatureIdFullPath, BuildTwoCubeGraph());

    // Adding restrictions.
    TEST(restrictionCollector.AddRestriction(
             {2.0, 0.0} /* coords of intersection feature with id = 1 and feature with id = 2 */,
             Restriction::Type::No,                     /* restriction type */
             {base::MakeOsmWay(1), base::MakeOsmWay(2)} /* features in format {from, (via*)?, to} */
             ),
         ());

    TEST(restrictionCollector.AddRestriction({2.0, 1.0}, Restriction::Type::Only,
                                             {base::MakeOsmWay(2), base::MakeOsmWay(3)}),
         ());

    TEST(restrictionCollector.AddRestriction(
             RestrictionCollector::kNoCoords, /* no coords in case of way as via */
             Restriction::Type::No,
             /*      from                via                    to         */
             {base::MakeOsmWay(0), base::MakeOsmWay(1), base::MakeOsmWay(2)}),
         ());

    base::SortUnique(restrictionCollector.m_restrictions);

    std::vector<Restriction> expectedRestrictions = {
        {Restriction::Type::No, {1, 2}},
        {Restriction::Type::Only, {2, 3}},
        {Restriction::Type::No, {0, 1, 2}},
    };

    std::sort(expectedRestrictions.begin(), expectedRestrictions.end());

    TEST_EQUAL(restrictionCollector.m_restrictions, expectedRestrictions, ());
  }

  void InvalidCase_NoSuchFeature()
  {
    RestrictionCollector restrictionCollector(m_osmIdsToFeatureIdFullPath, BuildTwoCubeGraph());

    // No such feature - 2809
    TEST(!restrictionCollector.AddRestriction({2.0, 1.0}, Restriction::Type::No,
                                              {base::MakeOsmWay(2809), base::MakeOsmWay(1)}),
         ());

    TEST(!restrictionCollector.HasRestrictions(), ());
  }

  void InvalidCase_FeaturesNotIntersecting()
  {
    RestrictionCollector restrictionCollector(m_osmIdsToFeatureIdFullPath, BuildTwoCubeGraph());

    // Fetures with id 1 and 2 do not intersect in {2.0, 1.0}
    TEST(!restrictionCollector.AddRestriction({2.0, 1.0}, Restriction::Type::No,
                                              {base::MakeOsmWay(1), base::MakeOsmWay(2)}),
         ());

    // No such chain of features (1 => 2 => 4),
    // because feature with id 2 and 4 do not have common joint.
    TEST(!restrictionCollector.AddRestriction(
             RestrictionCollector::kNoCoords, Restriction::Type::No,
             {base::MakeOsmWay(1), base::MakeOsmWay(2), base::MakeOsmWay(4)}),
         ());

    TEST(!restrictionCollector.HasRestrictions(), ());
  }

private:
  ScopedDir m_scopedDir;
  std::shared_ptr<ScopedFile> m_scopedFile;
  std::string m_osmIdsToFeatureIdFullPath;
};

UNIT_CLASS_TEST(TestRestrictionCollector, ValidCase) { TestRestrictionCollector::ValidCase(); }

UNIT_CLASS_TEST(TestRestrictionCollector, InvalidCase_NoSuchFeature)
{
  TestRestrictionCollector::InvalidCase_NoSuchFeature();
}

UNIT_CLASS_TEST(TestRestrictionCollector, InvalidCase_FeaturesNotIntersecting)
{
  TestRestrictionCollector::InvalidCase_FeaturesNotIntersecting();
}
}  // namespace routing
