#include "testing/testing.hpp"

#include "generator/cities_ids_builder.hpp"
#include "generator/feature_builder.hpp"
#include "generator/generator_tests_support/test_feature.hpp"
#include "generator/generator_tests_support/test_with_custom_mwms.hpp"
#include "generator/utils.hpp"

#include "indexer/classificator.hpp"
#include "indexer/data_source.hpp"
#include "indexer/feature.hpp"
#include "indexer/feature_processor.hpp"
#include "indexer/feature_to_osm.hpp"
#include "indexer/ftypes_matcher.hpp"

#include "platform/country_defines.hpp"
#include "platform/local_country_file.hpp"

#include "geometry/point2d.hpp"

#include "base/assert.hpp"
#include "base/geo_object_id.hpp"

#include <cstddef>
#include <sstream>
#include <string>

using namespace generator;
using namespace generator::tests_support;

namespace
{
class CitiesIdsTest : public TestWithCustomMwms
{
public:
  DataSource const & GetDataSource() const { return m_dataSource; }
};

// A feature that is big enough for World.mwm but is not a locality.
class TestSea : public TestFeature
{
public:
  TestSea(m2::PointD const & center, std::string const & name, std::string const & lang)
    : TestFeature(center, name, lang), m_name(name)
  {
  }

  // TestFeature overrides:
  void Serialize(feature::FeatureBuilder & fb) const
  {
    TestFeature::Serialize(fb);
    fb.AddType(classif().GetTypeByPath({"place", "sea"}));
  }

  std::string ToDebugString() const
  {
    std::ostringstream oss;
    oss << "TestSea [" << m_name << "]";
    return oss.str();
  }

private:
  std::string m_name;
};
}  // namespace

UNIT_CLASS_TEST(CitiesIdsTest, BuildCitiesIds)
{
  TestCity paris(m2::PointD(0, 0), "Paris", "fr", 100 /* rank */);
  TestCity wien(m2::PointD(1, 1), "Wien", "de", 100 /* rank */);
  TestSea marCaribe(m2::PointD(2, 2), "Mar Caribe", "es");

  auto testWorldId = BuildWorld([&](TestMwmBuilder & builder) {
    builder.Add(paris);
    builder.Add(wien);
    builder.Add(marCaribe);
  });

  auto const worldMwmPath = testWorldId.GetInfo()->GetLocalFile().GetPath(MapOptions::Map);

  indexer::FeatureIdToGeoObjectIdBimap bimap(GetDataSource());
  TEST(bimap.Load(), ());

  std::unordered_map<uint32_t, uint64_t> originalMapping;
  CHECK(ParseFeatureIdToTestIdMapping(worldMwmPath, originalMapping), ());

  {
    size_t numFeatures = 0;
    size_t numLocalities = 0;
    auto const test = [&](FeatureType & ft, uint32_t index) {
      ++numFeatures;
      FeatureID const fid(testWorldId, index);
      base::GeoObjectId gid;
      FeatureID receivedFid;

      bool const mustExist = ftypes::IsLocalityChecker::Instance()(ft);

      TEST_EQUAL(bimap.GetGeoObjectId(fid, gid), mustExist, (index, ft.GetNames()));
      if (!mustExist)
        return;

      ++numLocalities;
      TEST_EQUAL(bimap.GetFeatureID(gid, receivedFid), mustExist, (index));
      TEST_EQUAL(receivedFid, fid, ());

      CHECK(originalMapping.find(index) != originalMapping.end(), ());
      TEST_EQUAL(originalMapping[index], gid.GetSerialId(), ());
    };

    feature::ForEachFromDat(worldMwmPath, test);

    TEST_EQUAL(numFeatures, 3, ());
    TEST_EQUAL(numLocalities, 2, ());
  }
}
