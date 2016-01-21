#include "testing/testing.hpp"

#include "storage/country_info_getter.hpp"
#include "storage/country.hpp"
#include "storage/storage.hpp"

#include "geometry/mercator.hpp"

#include "platform/mwm_version.hpp"
#include "platform/platform.hpp"

#include "base/logging.hpp"

#include "std/unique_ptr.hpp"


using namespace storage;

namespace
{
double constexpr kEpsilon = 1e-3;

unique_ptr<CountryInfoGetter> CreateCountryInfoGetter()
{
  Platform & platform = GetPlatform();
  return make_unique<CountryInfoGetter>(platform.GetReader(PACKED_POLYGONS_FILE),
                                        platform.GetReader(COUNTRIES_FILE));
}

bool IsEmptyName(map<string, CountryInfo> const & id2info, string const & id)
{
  auto const it = id2info.find(id);
  TEST(it != id2info.end(), ());
  CountryInfo const ci = it->second;
  return ci.m_name.empty();
}
}  // namespace

UNIT_TEST(CountryInfoGetter_GetByPoint_Smoke)
{
  auto const getter = CreateCountryInfoGetter();

  CountryInfo info;
  Storage storage;
  bool const isSingleMwm = version::IsSingleMwm(storage.GetCurrentDataVersion());

  // Minsk
  getter->GetRegionInfo(MercatorBounds::FromLatLon(53.9022651, 27.5618818), info);
  string const belarusName = isSingleMwm ? "Belarus_Minsk Region" : "Belarus";
  TEST_EQUAL(info.m_name, belarusName, ());

  getter->GetRegionInfo(MercatorBounds::FromLatLon(-6.4146288, -38.0098101), info);
  string const brazilName = isSingleMwm ? "Brazil_Rio Grande do Norte" : "Brazil_Northeast";
  TEST_EQUAL(info.m_name, brazilName, ());

  getter->GetRegionInfo(MercatorBounds::FromLatLon(34.6509, 135.5018), info);
  string const japanName = isSingleMwm ? "Japan_Kinki Region_Osaka" : "Japan_Kinki";
  TEST_EQUAL(info.m_name, japanName, ());
}

UNIT_TEST(CountryInfoGetter_ValidName_Smoke)
{
  string buffer;
  ReaderPtr<Reader>(GetPlatform().GetReader(COUNTRIES_FILE)).ReadAsString(buffer);

  map<string, CountryInfo> id2info;
  bool isSingleMwm;
  storage::LoadCountryFile2CountryInfo(buffer, id2info, isSingleMwm);

  Storage storage;

  if (version::IsSingleMwm(storage.GetCurrentDataVersion()))
  {
    TEST(!IsEmptyName(id2info, "Belgium_West Flanders"), ());
    TEST(!IsEmptyName(id2info, "France_Ile-de-France_Paris"), ());
  }
  else
  {
    TEST(!IsEmptyName(id2info, "Germany_Baden-Wurttemberg"), ());
    TEST(!IsEmptyName(id2info, "France_Paris & Ile-de-France"), ());
  }
}

UNIT_TEST(CountryInfoGetter_SomeRects)
{
  auto const getter = CreateCountryInfoGetter();

  m2::RectD rects[3];
  getter->CalcUSALimitRect(rects);

  LOG(LINFO, ("USA Continental: ", rects[0]));
  LOG(LINFO, ("Alaska: ", rects[1]));
  LOG(LINFO, ("Hawaii: ", rects[2]));

  LOG(LINFO, ("Canada: ", getter->CalcLimitRect("Canada_")));
}

UNIT_TEST(CountryInfoGetter_GetRegionsCountryId)
{
  auto const getter = CreateCountryInfoGetter();

  TCountriesVec countryIds;
  m2::PointD const montevideoUruguay = MercatorBounds::FromLatLon(-34.8094, -56.1558);
  getter->GetRegionsCountryId(montevideoUruguay, countryIds);
  TEST_EQUAL(countryIds.size(), 1, ());
  TEST_EQUAL(countryIds[0], string("Uruguay"), ());
}

UNIT_TEST(CountryInfoGetter_CalcLimitRectForLeafSingleMwm)
{
  auto const getter = CreateCountryInfoGetter();
  Storage storage(COUNTRIES_FILE);
  if (!version::IsSingleMwm(storage.GetCurrentDataVersion()))
    return;

  m2::RectD leafBoundBox = getter->CalcLimitRectForLeaf("Angola");
  TEST(my::AlmostEqualAbs(leafBoundBox.maxX(), 24.08212, kEpsilon), ());
  TEST(my::AlmostEqualAbs(leafBoundBox.maxY(), -4.393187, kEpsilon), ());
  TEST(my::AlmostEqualAbs(leafBoundBox.minX(), 9.205259, kEpsilon), ());
  TEST(my::AlmostEqualAbs(leafBoundBox.minY(), -18.34456, kEpsilon), ());
}

UNIT_TEST(CountryInfoGetter_CalcLimitRectForLeafTwoComponentMwm)
{
  auto const getter = CreateCountryInfoGetter();
  Storage storage;
  if (version::IsSingleMwm(storage.GetCurrentDataVersion()))
    return;

  m2::RectD leafBoundBox = getter->CalcLimitRectForLeaf("Angola");
  TEST(my::AlmostEqualAbs(leafBoundBox.maxX(), 24.08212, kEpsilon), ());
  TEST(my::AlmostEqualAbs(leafBoundBox.maxY(), -4.393187, kEpsilon), ());
  TEST(my::AlmostEqualAbs(leafBoundBox.minX(), 11.50151, kEpsilon), ());
  TEST(my::AlmostEqualAbs(leafBoundBox.minY(), -18.344569, kEpsilon), ());
}
