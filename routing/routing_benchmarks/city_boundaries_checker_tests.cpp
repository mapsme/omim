#include "testing/testing.hpp"

#include "routing/city_boundaries_checker.hpp"

#include "indexer/data_source.hpp"

#include "geometry/latlon.hpp"
#include "geometry/mercator.hpp"

#include "platform/local_country_file.hpp"
#include "platform/local_country_file_utils.hpp"

#include "base/logging.hpp"
#include "base/timer.hpp"

#include <limits>
#include <random>
#include <vector>

namespace
{
using namespace platform;
using namespace std;

FrozenDataSource & RegisterWorldMwm(FrozenDataSource & dataSource)
{
  vector<LocalCountryFile> localFiles;
  FindAllLocalMapsAndCleanup(numeric_limits<int64_t>::max(), localFiles);

  LocalCountryFile world;
  for (auto const & lcf : localFiles)
  {
    if (lcf.GetCountryName() == "World")
    {
      world = lcf;
      break;
    }
  }
  CHECK_NOT_EQUAL(world.GetCountryName(), string(), ());

  auto const result = dataSource.RegisterMap(world);
  CHECK_EQUAL(result.second, MwmSet::RegResult::Success, ());
  return dataSource;
}

class CityBoundariesCheckerTest
{
public:
  CityBoundariesCheckerTest() : m_checker(RegisterWorldMwm(m_dataSource)) {}

protected:
  FrozenDataSource m_dataSource;
  routing::CityBoundariesChecker m_checker;
};

UNIT_CLASS_TEST(CityBoundariesCheckerTest, Smoke)
{
  LOG(LINFO, ("World.mwm contains", m_checker.GetSizeForTesting(), "city boundaries."));
  TEST_GREATER(m_checker.GetSizeForTesting(), 0, ());
}

// Test on CityBoundariesChecker::InCity() method.
UNIT_CLASS_TEST(CityBoundariesCheckerTest, InCity)
{
  // Point in Kolodischi (Belarus)
  TEST(m_checker.InCity(MercatorBounds::FromLatLon(53.94420, 27.77752)), ());

  // Point in two intersected city boundaries Borovliani and Lesnoy.
  TEST(m_checker.InCity(MercatorBounds::FromLatLon(54.00500, 27.68456)), ());

  // Point in Zvenigorod (Russia).
  TEST(m_checker.InCity(MercatorBounds::FromLatLon(55.72899, 36.85690)), ());

  // Point in Yauzskoi reservoir.
  TEST(!m_checker.InCity(MercatorBounds::FromLatLon(55.8838, 35.0399)), ());
}

UNIT_TEST(CityBoundariesCheckerTest_Benchmark)
{
  FrozenDataSource dataSource;
  RegisterWorldMwm(dataSource);

  {
    my::Timer timer;
    size_t constexpr ctorCalls = 10;
    for (size_t i = 0; i < ctorCalls; ++i)
    {
      routing::CityBoundariesChecker checker(dataSource);
      CHECK_GREATER(checker.GetSizeForTesting(), 0, ());
    }
    LOG(LINFO, (ctorCalls, "constructions of CityBoundariesChecker took", timer
        .ElapsedSeconds(), "seconds."));
  }

  routing::CityBoundariesChecker checker(dataSource);
  // A rect on the land with significant amount of city boundaries.
  ms::LatLon const northWest(56.7970, 26.2602);
  ms::LatLon const southEast(50.3718, 43.5256);
  double const deltaLat = northWest.lat - southEast.lat;
  double const deltaLon = southEast.lon - northWest.lon;

  CHECK_GREATER(deltaLat, 0.0, ());
  CHECK_GREATER(deltaLon, 0.0, ());

  mt19937 gen;
  uniform_real_distribution<double> dist(0.0, 1.0);

  size_t inCityCounter = 0;
  size_t outCityCounter = 0;
  my::Timer timer;
  size_t constexpr inCityCalls = 1000000;
  for(size_t i = 0; i < inCityCalls; ++i)
  {
    ms::LatLon const randomLatLon(southEast.lat + deltaLat * dist(gen),
                                  northWest.lon + deltaLon * dist(gen));
    if (checker.InCity(MercatorBounds::FromLatLon(randomLatLon)))
      ++inCityCounter;
    else
      ++outCityCounter;
  }
  LOG(LINFO,
      (inCityCalls, "calls of CityBoundariesChecker::InCity() method takes", timer.ElapsedSeconds(),
       "seconds.", inCityCounter, "points were in city and", outCityCounter, "were out of city."));
}
}  // namespace
