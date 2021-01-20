#include "testing/testing.hpp"

#include "generator/affiliation.hpp"
#include "generator/feature_builder.hpp"

#include "indexer/classificator.hpp"
#include "indexer/classificator_loader.hpp"

#include "platform/platform.hpp"

#include "geometry/point2d.hpp"

#include "base/assert.hpp"
#include "base/file_name_utils.hpp"
#include "base/stl_helpers.hpp"

#include <fstream>
#include <string>
#include <vector>

namespace
{
class AffiliationTests
{
public:
  static const std::string kOne;
  static const std::string kTwo;

  static constexpr m2::PointD kPointInsideOne1{3.0, 3.0};
  static constexpr m2::PointD kPointInsideOne2{4.0, 4.0};
  static constexpr m2::PointD kPointInsideTwo1{10.0, 3.0};
  static constexpr m2::PointD kPointInsideTwo2{11.0, 4.0};
  static constexpr m2::PointD kPointInsideOneAndTwo1{7.0, 4.0};
  static constexpr m2::PointD kPointInsideOneAndTwo2{9.0, 5.0};
  static constexpr m2::PointD kPointOnBorderOne{1.0, 6.0};
  static constexpr m2::PointD kPointOnBorderTwo{14.0, 6.0};
  static constexpr m2::PointD kPointInsideOneBoundingBox{1.0, 9.0};
  static constexpr m2::PointD kPointInsideTwoBoundingBox{14.0, 9.0};

  AffiliationTests()
  {
    classificator::Load();

    auto & platform = GetPlatform();
    m_testPath = base::JoinPath(platform.WritableDir(), "AffiliationTests");
    m_borderPath = base::JoinPath(m_testPath, "borders");
    CHECK(Platform::MkDirRecursively(m_borderPath), (m_borderPath));

    std::ofstream(base::JoinPath(m_borderPath, "One.poly")) <<
        R"(One
1
  5.0 0.0
  0.0 5.0
  5.0 10.0
  10.0 5.0
  5.0 0.0
END
END)";

    std::ofstream(base::JoinPath(m_borderPath, "Two.poly")) <<
        R"(Two
1
    10.0 0.0
    5.0 5.0
    10.0 10.0
    15.0 5.0
    10.0 0.0
END
END)";
  }

  ~AffiliationTests() { CHECK(Platform::RmDirRecursively(m_testPath), (m_testPath)); }

  std::string const & GetBorderPath() const { return m_testPath; }

  static feature::FeatureBuilder MakeLineFb(std::vector<m2::PointD> const & geom)
  {
    feature::FeatureBuilder fb;
    for (auto const & p : geom)
      fb.AddPoint(p);

    fb.AddType(classif().GetTypeByPath({"highway", "secondary"}));
    fb.SetLinear();
    return fb;
  }

private:
  std::string m_testPath;
  std::string m_borderPath;
};

// static
const std::string AffiliationTests::kOne = "One";
const std::string AffiliationTests::kTwo = "Two";

bool Test(std::vector<feature::CountryPolygonsPtr> const & res, std::set<std::string> const & answ)
{
  if (res.size() != answ.size())
    return false;

  std::set<std::string> r;
  base::Transform(res, std::inserter(r, std::begin(r)),
                  [](auto const * c) { return c->GetName(); });
  return r == answ;
}

void TestCountriesAffiliationInsideBorders(feature::AffiliationInterface const & affiliation)
{
  TEST(Test(affiliation.GetAffiliations(AffiliationTests::kPointInsideOne1),
            {AffiliationTests::kOne}),
       ());
  TEST(Test(affiliation.GetAffiliations(AffiliationTests::kPointInsideOne2),
            {AffiliationTests::kOne}),
       ());
  TEST(Test(affiliation.GetAffiliations(AffiliationTests::kPointOnBorderOne),
            {AffiliationTests::kOne}),
       ());
  TEST(Test(affiliation.GetAffiliations(AffiliationTests::kPointInsideTwo1),
            {AffiliationTests::kTwo}),
       ());
  TEST(Test(affiliation.GetAffiliations(AffiliationTests::kPointInsideTwo2),
            {AffiliationTests::kTwo}),
       ());
  TEST(Test(affiliation.GetAffiliations(AffiliationTests::kPointInsideOneAndTwo1),
            {AffiliationTests::kOne, AffiliationTests::kTwo}),
       ());
  TEST(Test(affiliation.GetAffiliations(AffiliationTests::kPointInsideOneAndTwo2),
            {AffiliationTests::kOne, AffiliationTests::kTwo}),
       ());
  TEST(Test(affiliation.GetAffiliations(AffiliationTests::kPointOnBorderTwo),
            {AffiliationTests::kTwo}),
       ());

  TEST(Test(affiliation.GetAffiliations(AffiliationTests::MakeLineFb(
                {AffiliationTests::kPointInsideOne1, AffiliationTests::kPointInsideOne2})),
            {AffiliationTests::kOne}),
       ());
  TEST(Test(affiliation.GetAffiliations(AffiliationTests::MakeLineFb(
                {AffiliationTests::kPointInsideTwo1, AffiliationTests::kPointInsideTwo2})),
            {AffiliationTests::kTwo}),
       ());
  TEST(Test(affiliation.GetAffiliations(AffiliationTests::MakeLineFb(
                {AffiliationTests::kPointInsideOne1, AffiliationTests::kPointInsideTwo1})),
            {AffiliationTests::kOne, AffiliationTests::kTwo}),
       ());
}

template <typename T>
void TestCountriesFilesAffiliation(std::string const & borderPath)
{
  {
    T affiliation(borderPath, false /* haveBordersForWholeWorld */);

    TestCountriesAffiliationInsideBorders(affiliation);

    TEST(Test(affiliation.GetAffiliations(AffiliationTests::kPointInsideOneBoundingBox), {}), ());
    TEST(Test(affiliation.GetAffiliations(AffiliationTests::kPointInsideTwoBoundingBox), {}), ());
  }
  {
    T affiliation(borderPath, true /* haveBordersForWholeWorld */);

    TestCountriesAffiliationInsideBorders(affiliation);

    TEST(Test(affiliation.GetAffiliations(AffiliationTests::kPointInsideOneBoundingBox),
              {AffiliationTests::kOne}),
         ());
    TEST(Test(affiliation.GetAffiliations(AffiliationTests::kPointInsideTwoBoundingBox),
              {AffiliationTests::kTwo}),
         ());
  }
}

UNIT_CLASS_TEST(AffiliationTests, SingleAffiliationTests)
{
  std::string const kName = "Name";
  feature::SingleAffiliation affiliation(kName);

  TEST(Test(affiliation.GetAffiliations(AffiliationTests::kPointInsideOne1), {kName}), ());

  TEST(Test(affiliation.GetAffiliations(AffiliationTests::kPointInsideOneAndTwo1), {kName}), ());

  TEST(Test(affiliation.GetAffiliations(AffiliationTests::MakeLineFb(
                {AffiliationTests::kPointInsideOne1, AffiliationTests::kPointInsideTwo1})),
            {kName}),
       ());

  TEST(affiliation.HasCountryByName(kName), ());
  TEST(!affiliation.HasCountryByName("NoName"), ());
}

UNIT_CLASS_TEST(AffiliationTests, CountriesFilesAffiliationTests)
{
  TestCountriesFilesAffiliation<feature::CountriesFilesAffiliation>(
      AffiliationTests::GetBorderPath());
}

UNIT_CLASS_TEST(AffiliationTests, CountriesFilesIndexAffiliationTests)
{
  TestCountriesFilesAffiliation<feature::CountriesFilesIndexAffiliation>(
      AffiliationTests::GetBorderPath());
}
}  // namespace
