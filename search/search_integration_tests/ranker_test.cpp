#include "testing/testing.hpp"

#include "search/search_tests_support/helpers.hpp"
#include "search/search_tests_support/test_results_matching.hpp"

#include "generator/generator_tests_support/test_feature.hpp"
#include "generator/generator_tests_support/test_mwm_builder.hpp"

#include <utility>
#include <vector>

using namespace generator::tests_support;
using namespace search::tests_support;
using namespace search;
using namespace std;

namespace
{
class RankerTest : public SearchTest
{
};

UNIT_CLASS_TEST(RankerTest, ErrorsInStreets)
{
  TestStreet mazurova(
      vector<m2::PointD>{m2::PointD(-0.001, -0.001), m2::PointD(0, 0), m2::PointD(0.001, 0.001)},
      "Мазурова", "ru");
  TestBuilding mazurova14(m2::PointD(-0.001, -0.001), "", "14", mazurova.GetName("ru"), "ru");

  TestStreet masherova(
      vector<m2::PointD>{m2::PointD(-0.001, 0.001), m2::PointD(0, 0), m2::PointD(0.001, -0.001)},
      "Машерова", "ru");
  TestBuilding masherova14(m2::PointD(0.001, 0.001), "", "14", masherova.GetName("ru"), "ru");

  auto id = BuildCountry("Belarus", [&](TestMwmBuilder & builder) {
    builder.Add(mazurova);
    builder.Add(mazurova14);

    builder.Add(masherova);
    builder.Add(masherova14);
  });

  SetViewport(m2::RectD(m2::PointD(0, 0), m2::PointD(0.001, 0.001)));
  {
    auto request = MakeRequest("Мазурова 14");
    auto const & results = request->Results();

    Rules rules = {ExactMatch(id, mazurova14), ExactMatch(id, masherova14)};
    TEST(ResultsMatch(results, rules), ());

    TEST_EQUAL(results.size(), 2, ());
    TEST(ResultsMatch({results[0]}, {rules[0]}), ());
    TEST(ResultsMatch({results[1]}, {rules[1]}), ());
  }
}

UNIT_CLASS_TEST(RankerTest, UniteSameResults)
{
  size_t constexpr kSameCount = 10;

  vector<TestCafe> bars;
  for (size_t i = 0; i < kSameCount; ++i)
    bars.emplace_back(m2::PointD(0.0, 1.0), "bar", "en");

  vector<TestCafe> cafes;
  for (size_t i = 0; i < kSameCount; ++i)
    cafes.emplace_back(m2::PointD(1.0, 1.0), "cafe", "en");

  vector<TestCafe> fastfoods;
  for (size_t i = 0; i < kSameCount; ++i)
    fastfoods.emplace_back(m2::PointD(1.0, 1.0), "fastfood", "en");

  auto id = BuildCountry("FoodLand", [&](TestMwmBuilder & builder) {
    for (auto const & b : bars)
      builder.Add(b);

    for (auto const & c : cafes)
      builder.Add(c);

    for (auto const & f : fastfoods)
      builder.Add(f);
  });

  SetViewport(m2::RectD(m2::PointD(0.0, 1.0), m2::PointD(2.0, 3.0)));
  {
    auto request = MakeRequest("eat ");
    auto const & results = request->Results();

    Rules barRules;
    for (auto const & b : bars)
      barRules.push_back(ExactMatch(id, b));

    Rules cafeRules;
    for (auto const & c : cafes)
      cafeRules.push_back(ExactMatch(id, c));

    Rules fastfoodRules;
    for (auto const & f : fastfoods)
      fastfoodRules.push_back(ExactMatch(id, f));

    TEST(ResultsMatch(results,
                      {AlternativesMatch(move(barRules)), AlternativesMatch(move(cafeRules)),
                       AlternativesMatch(move(fastfoodRules))}),
         ());
  }
}

UNIT_CLASS_TEST(RankerTest, PreferCountry)
{
  TestCountry wonderland(m2::PointD(10.0, 10.0), "Wonderland", "en");
  TestPOI cafe(m2::PointD(0.0, 0.0), "Wonderland", "en");
  auto worldId = BuildWorld([&](TestMwmBuilder & builder) {
    builder.Add(wonderland);
    builder.Add(cafe);
  });

  SetViewport(m2::RectD(m2::PointD(0.0, 0.0), m2::PointD(1.0, 1.0)));
  {
    // Country which exactly matches the query should be preferred even if cafe is much closer to
    // viewport center.
    auto request = MakeRequest("Wonderland");
    auto const & results = request->Results();

    Rules rules = {ExactMatch(worldId, wonderland), ExactMatch(worldId, cafe)};
    TEST(ResultsMatch(results, rules), ());

    TEST_EQUAL(results.size(), 2, ());
    TEST(ResultsMatch({results[0]}, {rules[0]}), ());
    TEST(ResultsMatch({results[1]}, {rules[1]}), ());
  }
  {
    // Country name does not exactly match, we should prefer cafe.
    auto request = MakeRequest("Wanderland");
    auto const & results = request->Results();

    Rules rules = {ExactMatch(worldId, wonderland), ExactMatch(worldId, cafe)};
    TEST(ResultsMatch(results, rules), ());

    TEST_EQUAL(results.size(), 2, ());
    TEST(ResultsMatch({results[0]}, {rules[1]}), ());
    TEST(ResultsMatch({results[1]}, {rules[0]}), ());
  }
}
}  // namespace
