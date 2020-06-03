#include "testing/testing.hpp"

#include "routing/routing_integration_tests/routing_test_tools.hpp"

#include "indexer/altitude_loader.hpp"
#include "indexer/classificator.hpp"
#include "indexer/classificator_loader.hpp"
#include "indexer/data_source.hpp"
#include "indexer/feature_altitude.hpp"
#include "indexer/feature_data.hpp"
#include "indexer/feature_processor.hpp"

#include "routing/routing_helpers.hpp"

#include "geometry/mercator.hpp"
#include "geometry/point_with_altitude.hpp"

#include "platform/local_country_file.hpp"

#include "base/math.hpp"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace
{
using namespace feature;
using namespace platform;
using namespace std;

LocalCountryFile GetLocalCountryFileByCountryId(CountryFile const & country)
{
  vector<LocalCountryFile> localFiles;
  integration::GetAllLocalFiles(localFiles);

  for (auto const & lf : localFiles)
  {
    if (lf.GetCountryFile() == country)
      return lf;
  }
  return {};
}

void TestAltitudeOfAllMwmFeatures(string const & countryId,
                                  geometry::Altitude const altitudeLowerBoundMeters,
                                  geometry::Altitude const altitudeUpperBoundMeters)
{
  FrozenDataSource dataSource;

  LocalCountryFile const country = GetLocalCountryFileByCountryId(CountryFile(countryId));
  TEST_NOT_EQUAL(country, LocalCountryFile(), ());
  TEST(country.HasFiles(), (country));

  pair<MwmSet::MwmId, MwmSet::RegResult> const regResult = dataSource.RegisterMap(country);
  TEST_EQUAL(regResult.second, MwmSet::RegResult::Success, ());
  TEST(regResult.first.IsAlive(), ());

  unique_ptr<AltitudeLoader> altitudeLoader =
      make_unique<AltitudeLoader>(dataSource, regResult.first /* mwmId */);

  ForEachFeature(country.GetPath(MapFileType::Map), [&](FeatureType & f, uint32_t const & id) {
    if (!routing::IsRoad(TypesHolder(f)))
      return;

    f.ParseGeometry(FeatureType::BEST_GEOMETRY);
    size_t const pointsCount = f.GetPointsCount();
    if (pointsCount == 0)
      return;

    geometry::Altitudes altitudes = altitudeLoader->GetAltitudes(id, pointsCount);
    TEST(!altitudes.empty(),
         ("Empty altitude vector. MWM:", countryId, ", feature id:", id, ", altitudes:", altitudes));

    for (auto const altitude : altitudes)
    {
      TEST_EQUAL(base::Clamp(altitude, altitudeLowerBoundMeters, altitudeUpperBoundMeters), altitude,
                 ("Unexpected altitude. MWM:", countryId, ", feature id:", id, ", altitudes:", altitudes));
    }
  });
}

UNIT_TEST(AllMwmFeaturesGetAltitudeTest)
{
  classificator::Load();

  TestAltitudeOfAllMwmFeatures("Russia_Moscow", 50 /* altitudeLowerBoundMeters */,
                               300 /* altitudeUpperBoundMeters */);
  TestAltitudeOfAllMwmFeatures("Nepal_Kathmandu", 250 /* altitudeLowerBoundMeters */,
                               6000 /* altitudeUpperBoundMeters */);
  TestAltitudeOfAllMwmFeatures("Netherlands_North Holland_Amsterdam", -25 /* altitudeLowerBoundMeters */,
                               50 /* altitudeUpperBoundMeters */);
}
}  // namespace
