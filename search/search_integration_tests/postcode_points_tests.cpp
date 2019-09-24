#include "testing/testing.hpp"

#include "search/postcode_points.hpp"
#include "search/search_tests_support/helpers.hpp"

#include "generator/generator_tests_support/test_mwm_builder.hpp"

#include "storage/country_info_getter.hpp"

#include "indexer/search_string_utils.hpp"

#include "platform/platform.hpp"
#include "platform/platform_tests_support/scoped_file.hpp"

#include "coding/point_coding.hpp"

#include "base/file_name_utils.hpp"
#include "base/math.hpp"
#include "base/stl_helpers.hpp"
#include "base/string_utils.hpp"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

using namespace generator::tests_support;
using namespace platform::tests_support;
using namespace search::tests_support;
using namespace search;
using namespace std;

namespace
{
class PostcodePointsTest : public SearchTest
{
};

UNIT_CLASS_TEST(PostcodePointsTest, Smoke)
{
  string const countryName = "Wonderland";

  Platform & platform = GetPlatform();
  auto const & writableDir = platform.WritableDir();
  string const testFile = "postcodes.csv";
  auto const postcodesRelativePath = base::JoinPath(writableDir, testFile);

  // <outward>,<inward>,<easting>,<northing>,<WGS84 lat>,<WGS84 long>,<2+6 NGR>,<grid>,<sources>
  ScopedFile const osmScopedFile(
      testFile,
      "aa11 0, dummy, 1000, 1000, dummy, dummy, dummy, dummy, dummy, dummy\n"
      "aa11 1, dummy, 2000, 2000, dummy, dummy, dummy, dummy, dummy, dummy\n"
      "aa11 2, dummy, 3000, 3000, dummy, dummy, dummy, dummy, dummy, dummy\n");

  auto infoGetter = std::make_shared<storage::CountryInfoGetterForTesting>();
  infoGetter->AddCountry(
      storage::CountryDef(countryName, m2::RectD(MercatorBounds::UKCoordsToXY(999, 999),
                                                 MercatorBounds::UKCoordsToXY(30001, 30001))));

  auto const id = BuildCountry(countryName, [&](TestMwmBuilder & builder) {
    builder.SetPostcodesData(postcodesRelativePath, infoGetter);
  });

  auto handle = m_dataSource.GetMwmHandleById(id);
  auto value = handle.GetValue<MwmValue>();
  CHECK(value, ());
  TEST(value->m_cont.IsExist(POSTCODE_POINTS_FILE_TAG), ());

  PostcodePoints p(*value);
  {
    vector<m2::PointD> points;
    p.Get(NormalizeAndSimplifyString("aa11 0"), points);
    TEST_EQUAL(points.size(), 1, ());
    TEST(base::AlmostEqualAbs(points[0], MercatorBounds::UKCoordsToXY(1000, 1000),
                              kMwmPointAccuracy),
         ());
  }
  {
    vector<m2::PointD> points;
    p.Get(NormalizeAndSimplifyString("aa11 1"), points);
    TEST_EQUAL(points.size(), 1, ());
    TEST(base::AlmostEqualAbs(points[0], MercatorBounds::UKCoordsToXY(2000, 2000),
                              kMwmPointAccuracy),
         ());
  }
  {
    vector<m2::PointD> points;
    p.Get(NormalizeAndSimplifyString("aa11 2"), points);
    TEST_EQUAL(points.size(), 1, ());
    TEST(base::AlmostEqualAbs(points[0], MercatorBounds::UKCoordsToXY(3000, 3000),
                              kMwmPointAccuracy),
         ());
  }
  {
    vector<m2::PointD> points;
    p.Get(NormalizeAndSimplifyString("aa11"), points);
    TEST_EQUAL(points.size(), 3, ());
    sort(points.begin(), points.end());
    TEST(base::AlmostEqualAbs(points[0], MercatorBounds::UKCoordsToXY(1000, 1000),
                              kMwmPointAccuracy),
         ());
    TEST(base::AlmostEqualAbs(points[1], MercatorBounds::UKCoordsToXY(2000, 2000),
                              kMwmPointAccuracy),
         ());
    TEST(base::AlmostEqualAbs(points[2], MercatorBounds::UKCoordsToXY(3000, 3000),
                              kMwmPointAccuracy),
         ());
  }
}

UNIT_CLASS_TEST(PostcodePointsTest, SearchPostcode)
{
  string const countryName = "Wonderland";

  Platform & platform = GetPlatform();
  auto const & writableDir = platform.WritableDir();
  string const testFile = "postcodes.csv";
  auto const postcodesRelativePath = base::JoinPath(writableDir, testFile);

  // <outward>,<inward>,<easting>,<northing>,<WGS84 lat>,<WGS84 long>,<2+6 NGR>,<grid>,<sources>
  ScopedFile const osmScopedFile(
      testFile,
      "BA6 7JP, dummy, 4000, 4000, dummy, dummy, dummy, dummy, dummy, dummy\n"
      "BA6 8JP, dummy, 6000, 6000, dummy, dummy, dummy, dummy, dummy, dummy\n");

  auto infoGetter = std::make_shared<storage::CountryInfoGetterForTesting>();
  infoGetter->AddCountry(
      storage::CountryDef(countryName, m2::RectD(MercatorBounds::UKCoordsToXY(3000, 3000),
                                                 MercatorBounds::UKCoordsToXY(7000, 7000))));

  auto const id = BuildCountry(countryName, [&](TestMwmBuilder & builder) {
    builder.SetPostcodesData(postcodesRelativePath, infoGetter);
  });

  auto test = [&](string const & query, m2::PointD const & expected) {
    auto request = MakeRequest(query);
    auto const & results = request->Results();
    TEST_EQUAL(results.size(), 1, ());

    auto const & result = results[0];
    TEST_EQUAL(result.GetResultType(), Result::Type::Postcode, ());
    TEST(result.HasPoint(), ());

    auto const actual = result.GetFeatureCenter();
    TEST(base::AlmostEqualAbs(expected, actual, kMwmPointAccuracy), ());
  };

  test("BA6 7JP", MercatorBounds::UKCoordsToXY(4000, 4000));
  test("BA6 7JP ", MercatorBounds::UKCoordsToXY(4000, 4000));
  test("BA6 8JP", MercatorBounds::UKCoordsToXY(6000, 6000));
  test("BA6 8JP ", MercatorBounds::UKCoordsToXY(6000, 6000));
  // Search should return center of all inward codes for outward query.
  test("BA6", MercatorBounds::UKCoordsToXY(5000, 5000));
  test("BA6 ", MercatorBounds::UKCoordsToXY(5000, 5000));
}
}  // namespace
