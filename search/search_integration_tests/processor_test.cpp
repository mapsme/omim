#include "testing/testing.hpp"

#include "generator/generator_tests_support/test_feature.hpp"
#include "generator/generator_tests_support/test_mwm_builder.hpp"

#include "generator/feature_builder.hpp"

#include "search/search_tests_support/helpers.hpp"
#include "search/search_tests_support/test_results_matching.hpp"
#include "search/search_tests_support/test_search_request.hpp"

#include "search/cities_boundaries_table.hpp"
#include "search/features_layer_path_finder.hpp"
#include "search/retrieval.hpp"
#include "search/token_range.hpp"
#include "search/token_slice.hpp"

#include "editor/editable_data_source.hpp"

#include "indexer/feature.hpp"
#include "indexer/ftypes_matcher.hpp"

#include "geometry/mercator.hpp"
#include "geometry/point2d.hpp"
#include "geometry/rect2d.hpp"

#include "base/assert.hpp"
#include "base/checked_cast.hpp"
#include "base/macros.hpp"
#include "base/math.hpp"
#include "base/scope_guard.hpp"
#include "base/string_utils.hpp"

#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

using namespace feature;
using namespace generator::tests_support;
using namespace search::tests_support;
using namespace std;

namespace search
{
namespace
{
class TestHotel : public TestPOI
{
public:
  using Type = ftypes::IsHotelChecker::Type;

  TestHotel(m2::PointD const & center, string const & name, string const & lang, float rating,
            int priceRate, Type type)
    : TestPOI(center, name, lang), m_rating(rating), m_priceRate(priceRate)
  {
    CHECK_GREATER_OR_EQUAL(m_rating, 0.0, ());
    CHECK_LESS_OR_EQUAL(m_rating, 10.0, ());

    CHECK_GREATER_OR_EQUAL(m_priceRate, 0, ());
    CHECK_LESS_OR_EQUAL(m_priceRate, 5, ());

    SetTypes({{"tourism", ftypes::IsHotelChecker::GetHotelTypeTag(type)}});
  }

  // TestPOI overrides:
  void Serialize(FeatureBuilder & fb) const override
  {
    TestPOI::Serialize(fb);

    auto & metadata = fb.GetMetadata();
    metadata.Set(Metadata::FMD_RATING, strings::to_string(m_rating));
    metadata.Set(Metadata::FMD_PRICE_RATE, strings::to_string(m_priceRate));
  }

private:
  float const m_rating;
  int const m_priceRate;
};

class TestAirport : public TestPOI
{
public:
  TestAirport(m2::PointD const & center, string const & name, string const & lang, string const & iata)
    : TestPOI(center, name, lang), m_iata(iata)
  {
    SetTypes({{"aeroway", "aerodrome"}});
  }

  // TestPOI overrides:
  void Serialize(FeatureBuilder & fb) const override
  {
    TestPOI::Serialize(fb);

    auto & metadata = fb.GetMetadata();
    metadata.Set(Metadata::FMD_AIRPORT_IATA, m_iata);
  }

private:
  string m_iata;
};

class TestATM : public TestPOI
{
public:
  TestATM(m2::PointD const & center, string const & op, string const & lang)
    : TestPOI(center, {} /* name */, lang), m_operator(op)
  {
    SetTypes({{"amenity", "atm"}});
  }

  // TestPOI overrides:
  void Serialize(FeatureBuilder & fb) const override
  {
    TestPOI::Serialize(fb);

    auto & metadata = fb.GetMetadata();
    metadata.Set(Metadata::FMD_OPERATOR, m_operator);
  }

private:
  string m_operator;
};

class TestBrandFeature : public TestCafe
{
public:
  TestBrandFeature(m2::PointD const & center, string const & brand, string const & lang)
    : TestCafe(center, {} /* name */, lang), m_brand(brand)
  {
  }

  // TestPOI overrides:
  void Serialize(FeatureBuilder & fb) const override
  {
    TestCafe::Serialize(fb);

    auto & metadata = fb.GetMetadata();
    metadata.Set(Metadata::FMD_BRAND, m_brand);
  }

private:
  string m_brand;
};

class ProcessorTest : public SearchTest
{
};

UNIT_CLASS_TEST(ProcessorTest, Smoke)
{
  string const countryName = "Wonderland";
  TestCountry wonderlandCountry(m2::PointD(10, 10), countryName, "en");

  TestCity losAlamosCity(m2::PointD(10, 10), "Los Alamos", "en", 100 /* rank */);
  TestCity mskCity(m2::PointD(0, 0), "Moscow", "en", 100 /* rank */);
  TestCity torontoCity(m2::PointD(-10, -10), "Toronto", "en", 100 /* rank */);

  TestVillage longPondVillage(m2::PointD(15, 15), "Long Pond Village", "en", 10 /* rank */);
  TestStreet feynmanStreet(
      vector<m2::PointD>{m2::PointD(9.999, 9.999), m2::PointD(10, 10), m2::PointD(10.001, 10.001)},
      "Feynman street", "en");
  TestStreet bohrStreet1(
      vector<m2::PointD>{m2::PointD(9.999, 10.001), m2::PointD(10, 10), m2::PointD(10.001, 9.999)},
      "Bohr street", "en");
  TestStreet bohrStreet2(vector<m2::PointD>{m2::PointD(10.001, 9.999), m2::PointD(10.002, 9.998)},
                         "Bohr street", "en");
  TestStreet bohrStreet3(vector<m2::PointD>{m2::PointD(10.002, 9.998), m2::PointD(10.003, 9.997)},
                         "Bohr street", "en");
  TestStreet firstAprilStreet(vector<m2::PointD>{m2::PointD(14.998, 15), m2::PointD(15.002, 15)},
                              "1st April street", "en");

  TestBuilding feynmanHouse(m2::PointD(10, 10), "Feynman house", "1 unit 1", feynmanStreet.GetName("en"), "en");
  TestBuilding bohrHouse(m2::PointD(10, 10), "Bohr house", "1 unit 1", bohrStreet1.GetName("en"), "en");
  TestBuilding hilbertHouse(
      vector<m2::PointD>{
          {10.0005, 10.0005}, {10.0006, 10.0005}, {10.0006, 10.0006}, {10.0005, 10.0006}},
      "Hilbert house", "1 unit 2", bohrStreet1.GetName("en"), "en");
  TestBuilding descartesHouse(m2::PointD(10, 10), "Descartes house", "2", "en");
  TestBuilding bornHouse(m2::PointD(14.999, 15), "Born house", "8", firstAprilStreet.GetName("en"), "en");

  TestPOI busStop(m2::PointD(0, 0), "Central bus stop", "en");
  TestPOI tramStop(m2::PointD(0.0001, 0.0001), "Tram stop", "en");
  TestPOI quantumTeleport1(m2::PointD(0.0002, 0.0002), "Quantum teleport 1", "en");

  TestPOI quantumTeleport2(m2::PointD(10, 10), "Quantum teleport 2", "en");
  quantumTeleport2.SetHouseNumber("3");
  quantumTeleport2.SetStreetName(feynmanStreet.GetName("en"));

  TestPOI quantumCafe(m2::PointD(-0.0002, -0.0002), "Quantum cafe", "en");
  TestPOI lantern1(m2::PointD(10.0005, 10.0005), "lantern 1", "en");
  TestPOI lantern2(m2::PointD(10.0006, 10.0005), "lantern 2", "en");

  TestStreet stradaDrive(vector<m2::PointD>{m2::PointD(-10.001, -10.001), m2::PointD(-10, -10),
                                            m2::PointD(-9.999, -9.999)},
                         "Strada drive", "en");
  TestBuilding terranceHouse(m2::PointD(-10, -10), "", "155", stradaDrive.GetName("en"), "en");

  auto const worldId = BuildWorld([&](TestMwmBuilder & builder) {
    builder.Add(wonderlandCountry);
    builder.Add(losAlamosCity);
    builder.Add(mskCity);
    builder.Add(torontoCity);
  });
  auto const wonderlandId = BuildCountry(countryName, [&](TestMwmBuilder & builder) {
    builder.Add(losAlamosCity);
    builder.Add(mskCity);
    builder.Add(torontoCity);
    builder.Add(longPondVillage);

    builder.Add(feynmanStreet);
    builder.Add(bohrStreet1);
    builder.Add(bohrStreet2);
    builder.Add(bohrStreet3);
    builder.Add(firstAprilStreet);

    builder.Add(feynmanHouse);
    builder.Add(bohrHouse);
    builder.Add(hilbertHouse);
    builder.Add(descartesHouse);
    builder.Add(bornHouse);

    builder.Add(busStop);
    builder.Add(tramStop);
    builder.Add(quantumTeleport1);
    builder.Add(quantumTeleport2);
    builder.Add(quantumCafe);
    builder.Add(lantern1);
    builder.Add(lantern2);

    builder.Add(stradaDrive);
    builder.Add(terranceHouse);
  });

  SetViewport(m2::RectD(m2::PointD(-1.0, -1.0), m2::PointD(1.0, 1.0)));
  {
    Rules rules = {};
    TEST(ResultsMatch("", rules), ());
  }
  {
    Rules rules = {ExactMatch(wonderlandId, busStop)};
    TEST(ResultsMatch("Central bus stop", rules), ());
  }
  {
    Rules rules = {ExactMatch(wonderlandId, quantumCafe),
                   ExactMatch(wonderlandId, quantumTeleport1),
                   ExactMatch(wonderlandId, quantumTeleport2)};
    TEST(ResultsMatch("quantum", rules), ());
  }
  {
    Rules rules = {ExactMatch(wonderlandId, quantumCafe),
                   ExactMatch(wonderlandId, quantumTeleport1)};
    TEST(ResultsMatch("quantum Moscow ", rules), ());
  }
  {
    TEST(ResultsMatch("     ", Rules()), ());
  }
  {
    Rules rules = {ExactMatch(wonderlandId, quantumTeleport2), ExactMatch(wonderlandId, feynmanStreet)};
    TEST(ResultsMatch("teleport feynman street", rules), ());
  }
  {
    Rules rules = {ExactMatch(wonderlandId, quantumTeleport2)};
    TEST(ResultsMatch("feynman street 3", rules), ());
  }
  {
    // Here we expect to find feynmanHouse (building next to Feynman street with housenumber '1 unit 1')
    // but not lantern1 (building next to Feynman street with name 'lantern 1') because '1'
    // looks like housenumber.
    Rules rules = {ExactMatch(wonderlandId, feynmanHouse),
                   ExactMatch(wonderlandId, firstAprilStreet)};
    TEST(ResultsMatch("feynman street 1", rules), ());
  }
  {
    // Here we expect to find bohrHouse (building next to Bohr street with housenumber '1 unit 1')
    // but not lantern1 (building next to Bohr street with name 'lantern 1') because '1' looks like
    // housenumber.
    Rules rules = {ExactMatch(wonderlandId, bohrHouse), ExactMatch(wonderlandId, hilbertHouse),
                   ExactMatch(wonderlandId, firstAprilStreet)};
    TEST(ResultsMatch("bohr street 1", rules), ());
  }
  {
    TEST(ResultsMatch("bohr street 1 unit 3", {ExactMatch(wonderlandId, bohrStreet1)}), ());
  }
  {
    Rules rules = {ExactMatch(wonderlandId, lantern1), ExactMatch(wonderlandId, lantern2)};
    TEST(ResultsMatch("bohr street 1 lantern ", rules), ());
  }
  {
    Rules rules = {ExactMatch(wonderlandId, feynmanHouse), ExactMatch(wonderlandId, feynmanStreet)};
    TEST(ResultsMatch("wonderland los alamos feynman 1 unit 1 ", rules), ());
  }
  {
    // It's possible to find Descartes house by name.
    Rules rules = {ExactMatch(wonderlandId, descartesHouse)};
    TEST(ResultsMatch("Los Alamos Descartes", rules), ());
  }
  {
    // It's not possible to find Descartes house by house number,
    // because it doesn't belong to Los Alamos streets. But it still
    // exists.
    // Also it's not possible to find 'Quantum teleport 2' and 'lantern 2' by name because '2' looks
    // like house number and it is not considered as poi name.
    Rules rules = {ExactMatch(worldId, losAlamosCity)};
    TEST(ResultsMatch("Los Alamos 2", rules), ());
  }
  {
    Rules rules = {ExactMatch(wonderlandId, bornHouse), ExactMatch(wonderlandId, firstAprilStreet)};
    TEST(ResultsMatch("long pond 1st april street 8 ", rules), ());
  }

  {
    Rules rules = {ExactMatch(wonderlandId, terranceHouse), ExactMatch(wonderlandId, stradaDrive)};
    TEST(ResultsMatch("Toronto strada drive 155", rules), ());
  }
}

UNIT_CLASS_TEST(ProcessorTest, SearchInWorld)
{
  string const countryName = "Wonderland";
  TestCountry wonderland(m2::PointD(0, 0), countryName, "en");
  TestCity losAlamos(m2::PointD(0, 0), "Los Alamos", "en", 100 /* rank */);

  auto testWorldId = BuildWorld([&](TestMwmBuilder & builder)
                                {
                                  builder.Add(wonderland);
                                  builder.Add(losAlamos);
                                });
  RegisterCountry(countryName, m2::RectD(m2::PointD(-1.0, -1.0), m2::PointD(1.0, 1.0)));

  SetViewport(m2::RectD(m2::PointD(-1.0, -1.0), m2::PointD(-0.5, -0.5)));
  {
    Rules rules = {ExactMatch(testWorldId, losAlamos)};
    TEST(ResultsMatch("Los Alamos", rules), ());
  }
  {
    Rules rules = {ExactMatch(testWorldId, wonderland)};
    TEST(ResultsMatch("Wonderland", rules), ());
  }
  {
    Rules rules = {ExactMatch(testWorldId, losAlamos)};
    TEST(ResultsMatch("Wonderland Los Alamos", rules), ());
  }
}

UNIT_CLASS_TEST(ProcessorTest, SearchByName)
{
  string const countryName = "Wonderland";
  TestCity london(m2::PointD(1, 1), "London", "en", 100 /* rank */);
  TestPark hydePark(vector<m2::PointD>{m2::PointD(0.5, 0.5), m2::PointD(1.5, 0.5),
                                       m2::PointD(1.5, 1.5), m2::PointD(0.5, 1.5)},
                    "Hyde Park", "en");
  TestPOI cafe(m2::PointD(1.0, 1.0), "London Cafe", "en");

  auto worldId = BuildWorld([&](TestMwmBuilder & builder)
                            {
                              builder.Add(london);
                            });
  auto wonderlandId = BuildCountry(countryName, [&](TestMwmBuilder & builder)
                                   {
                                     builder.Add(hydePark);
                                     builder.Add(cafe);
                                   });

  SetViewport(m2::RectD(m2::PointD(-1.0, -1.0), m2::PointD(-0.9, -0.9)));
  {
    Rules rules = {ExactMatch(wonderlandId, hydePark)};
    TEST(ResultsMatch("hyde park", rules), ());
    TEST(ResultsMatch("london hyde park", rules), ());
  }
  {
    Rules rules = {ExactMatch(worldId, london)};
    TEST(ResultsMatch("hyde london park", rules), ());
  }

  SetViewport(m2::RectD(m2::PointD(0.5, 0.5), m2::PointD(1.5, 1.5)));
  {
    Rules rules = {ExactMatch(worldId, london)};
    TEST(ResultsMatch("london", Mode::Downloader, rules), ());
  }
  {
    Rules rules = {ExactMatch(worldId, london), ExactMatch(wonderlandId, cafe)};
    TEST(ResultsMatch("london", Mode::Everywhere, rules), ());
  }
}

UNIT_CLASS_TEST(ProcessorTest, DisableSuggests)
{
  TestCity london1(m2::PointD(1, 1), "London", "en", 100 /* rank */);
  TestCity london2(m2::PointD(-1, -1), "London", "en", 100 /* rank */);

  auto worldId = BuildWorld([&](TestMwmBuilder & builder)
                            {
                              builder.Add(london1);
                              builder.Add(london2);
                            });

  {
    SearchParams params;
    params.m_query = "londo";
    params.m_inputLocale = "en";
    params.m_viewport = m2::RectD(m2::PointD(0.5, 0.5), m2::PointD(1.5, 1.5));
    params.m_mode = Mode::Downloader;
    params.m_suggestsEnabled = false;

    TestSearchRequest request(m_engine, params);
    request.Run();
    Rules rules = {ExactMatch(worldId, london1), ExactMatch(worldId, london2)};

    TEST(ResultsMatch(request.Results(), rules), ());
  }
}

UNIT_CLASS_TEST(ProcessorTest, TestRankingInfo)
{
  string const countryName = "Wonderland";

  TestCity sanFrancisco(m2::PointD(1, 1), "San Francisco", "en", 100 /* rank */);
  // Golden Gate Bridge-bridge is located in this test on the Golden
  // Gate Bridge-street. Therefore, there are several valid parses of
  // the query "Golden Gate Bridge", and search engine must return
  // both features (street and bridge) as they were matched by full
  // name.
  TestStreet goldenGateStreet(
      vector<m2::PointD>{m2::PointD(-0.5, -0.5), m2::PointD(0, 0), m2::PointD(0.5, 0.5)},
      "Golden Gate Bridge", "en");

  TestPOI goldenGateBridge(m2::PointD(0, 0), "Golden Gate Bridge", "en");

  TestPOI waterfall(m2::PointD(0.5, 0.5), "", "en");
  waterfall.SetTypes({{"waterway", "waterfall"}});

  TestPOI lermontov(m2::PointD(1, 1), "Лермонтовъ", "en");
  lermontov.SetTypes({{"amenity", "cafe"}});

  // A low-rank city with two noname cafes.
  TestCity lermontovo(m2::PointD(-1, -1), "Лермонтово", "en", 0 /* rank */);
  TestPOI cafe1(m2::PointD(-1.01, -1.01), "", "en");
  cafe1.SetTypes({{"amenity", "cafe"}});
  TestPOI cafe2(m2::PointD(-0.99, -0.99), "", "en");
  cafe2.SetTypes({{"amenity", "cafe"}});

  // A low-rank village with a single noname cafe.
  TestVillage pushkino(m2::PointD(-10, -10), "Pushkino", "en", 0 /* rank */);
  TestPOI cafe3(m2::PointD(-10.01, -10.01), "", "en");
  cafe3.SetTypes({{"amenity", "cafe"}});

  auto worldId = BuildWorld([&](TestMwmBuilder & builder)
                            {
                              builder.Add(sanFrancisco);
                              builder.Add(lermontovo);
                            });
  auto wonderlandId = BuildCountry(countryName, [&](TestMwmBuilder & builder)
                                   {
                                     builder.Add(cafe1);
                                     builder.Add(cafe2);
                                     builder.Add(cafe3);
                                     builder.Add(goldenGateBridge);
                                     builder.Add(goldenGateStreet);
                                     builder.Add(lermontov);
                                     builder.Add(pushkino);
                                     builder.Add(waterfall);
                                   });

  SetViewport(m2::RectD(m2::PointD(-0.5, -0.5), m2::PointD(0.5, 0.5)));
  {
    auto request = MakeRequest("golden gate bridge ");

    Rules rules = {ExactMatch(wonderlandId, goldenGateBridge),
                   ExactMatch(wonderlandId, goldenGateStreet)};

    TEST(ResultsMatch(request->Results(), rules), ());
    for (auto const & result : request->Results())
    {
      auto const & info = result.GetRankingInfo();
      TEST_EQUAL(NAME_SCORE_FULL_MATCH, info.m_nameScore, (result));
      TEST(!info.m_pureCats, (result));
      TEST(!info.m_falseCats, (result));
    }
  }

  // This test is quite important and must always pass.
  {
    auto request = MakeRequest("cafe лермонтов");
    auto const & results = request->Results();

    Rules rules{ExactMatch(wonderlandId, cafe1), ExactMatch(wonderlandId, cafe2),
                ExactMatch(wonderlandId, lermontov)};
    TEST(ResultsMatch(results, rules), ());

    TEST_EQUAL(3, results.size(), ("Unexpected number of retrieved cafes."));
    auto const & top = results.front();
    TEST(ResultsMatch({top}, {ExactMatch(wonderlandId, lermontov)}), ());
  }

  {
    Rules rules{ExactMatch(wonderlandId, waterfall)};
    TEST(ResultsMatch("waterfall", rules), ());
  }

  SetViewport(m2::RectD(m2::PointD(-10.5, -10.5), m2::PointD(-9.5, -9.5)));
  {
    Rules rules{ExactMatch(wonderlandId, cafe3)};
    TEST(ResultsMatch("cafe pushkino ", rules), ());
  }
}

UNIT_CLASS_TEST(ProcessorTest, TestRankingInfo_ErrorsMade)
{
  string const countryName = "Wonderland";

  TestCity chekhov(m2::PointD(0, 0), "Чеховъ Антонъ Павловичъ", "ru", 100 /* rank */);

  TestStreet yesenina(
      vector<m2::PointD>{m2::PointD(0.5, -0.5), m2::PointD(0, 0), m2::PointD(-0.5, 0.5)},
      "Yesenina street", "en");

  TestStreet pushkinskaya(
      vector<m2::PointD>{m2::PointD(-0.5, -0.5), m2::PointD(0, 0), m2::PointD(0.5, 0.5)},
      "Улица Пушкинская", "ru");

  TestStreet ostrovskogo(
      vector<m2::PointD>{m2::PointD(-0.5, 0.0), m2::PointD(0, 0), m2::PointD(0.5, 0.0)},
      "улица Островского", "ru");

  TestPOI lermontov(m2::PointD(0, 0), "Трактиръ Лермонтовъ", "ru");
  lermontov.SetTypes({{"amenity", "cafe"}});

  auto worldId = BuildWorld([&](TestMwmBuilder & builder) { builder.Add(chekhov); });

  auto wonderlandId = BuildCountry(countryName, [&](TestMwmBuilder & builder) {
    builder.Add(yesenina);
    builder.Add(pushkinskaya);
    builder.Add(ostrovskogo);
    builder.Add(lermontov);
  });

  SetViewport(m2::RectD(-1, -1, 1, 1));

  auto checkErrors = [&](string const & query, ErrorsMade const & errorsMade) {
    auto request = MakeRequest(query, "ru");
    auto const & results = request->Results();

    Rules rules{ExactMatch(wonderlandId, lermontov)};
    TEST(ResultsMatch(results, rules), (query));
    TEST_EQUAL(results.size(), 1, (query));
    TEST_EQUAL(results[0].GetRankingInfo().m_errorsMade, errorsMade, (query));
  };

  // Prefix match "лермонтов" -> "Лермонтовъ" without errors.
  checkErrors("кафе лермонтов", ErrorsMade(0));
  checkErrors("кафе лермнтовъ", ErrorsMade(1));
  // Full match.
  checkErrors("трактир лермонтов", ErrorsMade(2));
  checkErrors("кафе", ErrorsMade());

  checkErrors("Cafe Yesenina", ErrorsMade(0));
  checkErrors("Cafe Jesenina", ErrorsMade(1));
  // We allow only Y->{E, J, I, U} misprints for the first letter.
  checkErrors("Cafe Esenina", ErrorsMade(2));

  checkErrors("Островского кафе", ErrorsMade(0));
  checkErrors("Астровского кафе", ErrorsMade(1));

  checkErrors("пушкенская трактир лермонтов", ErrorsMade(3));
  checkErrors("пушкенская кафе", ErrorsMade(1));
  checkErrors("пушкинская трактиръ лермонтовъ", ErrorsMade(0));

  // Prefix match "чехов" -> "Чеховъ" without errors.
  checkErrors("лермонтовъ чехов", ErrorsMade(0));
  checkErrors("лермонтовъ чехов ", ErrorsMade(1));
  checkErrors("лермонтовъ чеховъ", ErrorsMade(0));

  // Prefix match "чехов" -> "Чеховъ" without errors.
  checkErrors("лермонтов чехов", ErrorsMade(1));
  checkErrors("лермонтов чехов ", ErrorsMade(2));
  checkErrors("лермонтов чеховъ", ErrorsMade(1));

  checkErrors("лермонтов чеховъ антон павлович", ErrorsMade(3));
}

UNIT_CLASS_TEST(ProcessorTest, TestHouseNumbers)
{
  string const countryName = "HouseNumberLand";

  TestCity greenCity(m2::PointD(0, 0), "Зеленоград", "ru", 100 /* rank */);

  TestStreet street(
      vector<m2::PointD>{m2::PointD(-5.0, -5.0), m2::PointD(0, 0), m2::PointD(5.0, 5.0)},
      "Генерала Генералова", "ru");

  TestBuilding building100(m2::PointD(2.0, 2.0), "", "100", "en");
  TestBuilding building200(m2::PointD(3.0, 3.0), "", "к200", "ru");
  TestBuilding building300(m2::PointD(4.0, 4.0), "", "300 строение 400", "ru");
  TestBuilding building115(m2::PointD(1.0, 1.0), "", "115", "en");
  TestBuilding building115k1(m2::PointD(-1.0, -1.0), "", "115к1", "en");

  BuildWorld([&](TestMwmBuilder & builder) { builder.Add(greenCity); });
  auto countryId = BuildCountry(countryName, [&](TestMwmBuilder & builder) {
    builder.Add(street);
    builder.Add(building100);
    builder.Add(building200);
    builder.Add(building300);
    builder.Add(building115);
    builder.Add(building115k1);
  });

  SetViewport(m2::RectD(m2::PointD(-1.0, -1.0), m2::PointD(1.0, 1.0)));

  {
    Rules rules{ExactMatch(countryId, building100), ExactMatch(countryId, street)};
    TEST(ResultsMatch("Зеленоград генералова к100 ", "ru", rules), ());
  }
  {
    Rules rules{ExactMatch(countryId, building200), ExactMatch(countryId, street)};
    TEST(ResultsMatch("Зеленоград генералова к200 ", "ru", rules), ());
  }
  {
    Rules rules{ExactMatch(countryId, building200), ExactMatch(countryId, street)};
    TEST(ResultsMatch("Зеленоград к200 генералова ", "ru", rules), ());
  }
  {
    Rules rules{ExactMatch(countryId, building300), ExactMatch(countryId, street)};
    TEST(ResultsMatch("Зеленоград 300 строение 400 генералова ", "ru", rules), ());
  }
  {
    Rules rules{ExactMatch(countryId, street)};
    TEST(ResultsMatch("Зеленоград генералова строе 300", "ru", rules), ());
  }
  {
    Rules rules{ExactMatch(countryId, building300), ExactMatch(countryId, street)};
    TEST(ResultsMatch("Зеленоград генералова 300 строе", "ru", rules), ());
  }
  {
    auto request = MakeRequest("Зеленоград Генерала Генералова 115 ", "ru");

    // Test exact matching result ranked first.
    auto const & results = request->Results();
    TEST_GREATER(results.size(), 0, (results));
    TEST(ResultMatches(results[0], ExactMatch(countryId, building115)), (results));

    Rules rules{ExactMatch(countryId, building115), ExactMatch(countryId, building115k1),
                ExactMatch(countryId, street)};
    TEST(ResultsMatch(results, rules), ());
  }
  {
    auto request = MakeRequest("Зеленоград Генерала Генералова 115к1 ", "ru");

    // Test exact matching result ranked first.
    auto const & results = request->Results();
    TEST_GREATER(results.size(), 0, (results));
    TEST(ResultMatches(results[0], ExactMatch(countryId, building115k1)), (results));

    Rules rules{ExactMatch(countryId, building115k1), ExactMatch(countryId, building115),
                ExactMatch(countryId, street)};
    TEST(ResultsMatch(results, rules), ());
  }
  {
    Rules rules{ExactMatch(countryId, building115), ExactMatch(countryId, street)};
    TEST(ResultsMatch("Зеленоград Генерала Генералова 115к2 ", "ru", rules), ());
  }
}

UNIT_CLASS_TEST(ProcessorTest, TestPostcodes)
{
  string const countryName = "Russia";

  TestCity dolgoprudny(m2::PointD(0, 0), "Долгопрудный", "ru", 100 /* rank */);
  TestCity london(m2::PointD(10, 10), "London", "en", 100 /* rank */);

  TestStreet street(
      vector<m2::PointD>{m2::PointD(-0.5, 0.0), m2::PointD(0, 0), m2::PointD(0.5, 0.0)},
      "Первомайская", "ru");
  street.SetPostcode("141701");

  TestBuilding building28(m2::PointD(0.0, 0.00001), "", "28а", street.GetName("ru"), "ru");
  building28.SetPostcode("141701");

  TestBuilding building29(m2::PointD(0.0, -0.00001), "", "29", street.GetName("ru"), "ru");
  building29.SetPostcode("141701");

  TestPOI building30(m2::PointD(0.00002, 0.00002), "", "en");
  building30.SetHouseNumber("30");
  building30.SetPostcode("141701");
  building30.SetTypes({{"building", "address"}});

  TestBuilding building31(m2::PointD(0.00001, 0.00001), "", "31", street.GetName("ru"), "ru");
  building31.SetPostcode("141702");

  TestBuilding building1(m2::PointD(10, 10), "", "1", "en");
  building1.SetPostcode("WC2H 7BX");

  BuildWorld([&](TestMwmBuilder & builder)
             {
               builder.Add(dolgoprudny);
               builder.Add(london);
             });
  auto countryId = BuildCountry(countryName, [&](TestMwmBuilder & builder)
                                {
                                  builder.Add(street);
                                  builder.Add(building28);
                                  builder.Add(building29);
                                  builder.Add(building30);
                                  builder.Add(building31);

                                  builder.Add(building1);
                                });

  // Tests that postcode is added to the search index.
  {
    MwmContext context(m_dataSource.GetMwmHandleById(countryId));
    base::Cancellable cancellable;

    QueryParams params;
    {
      strings::UniString const tokens[] = {strings::MakeUniString("141702")};
      params.InitNoPrefix(tokens, tokens + ARRAY_SIZE(tokens));
    }

    Retrieval retrieval(context, cancellable);
    auto features = retrieval.RetrievePostcodeFeatures(
        TokenSlice(params, TokenRange(0, params.GetNumTokens())));
    TEST_EQUAL(1, features.PopCount(), ());

    uint64_t index = 0;
    while (!features.HasBit(index))
      ++index;

    FeaturesLoaderGuard loader(m_dataSource, countryId);
    auto ft = loader.GetFeatureByIndex(base::checked_cast<uint32_t>(index));
    TEST(ft, ());

    auto rule = ExactMatch(countryId, building31);
    TEST(rule->Matches(*ft), ());
  }

  {
    Rules rules{ExactMatch(countryId, building28), ExactMatch(countryId, street)};
    TEST(ResultsMatch("Долгопрудный первомайская 28а ", "ru", rules), ());
  }
  {
    Rules rules{ExactMatch(countryId, building28), ExactMatch(countryId, street)};
    TEST(ResultsMatch("Долгопрудный первомайская 28а, 141701", "ru", rules), ());
  }
  {
    Rules rules{ExactMatch(countryId, building28), ExactMatch(countryId, building29),
                ExactMatch(countryId, building30), ExactMatch(countryId, street)};
    TEST(ResultsMatch("Долгопрудный первомайская 141701", "ru", rules), ());
  }
  {
    Rules rules{ExactMatch(countryId, building31), ExactMatch(countryId, street)};
    TEST(ResultsMatch("Долгопрудный первомайская 141702", "ru", rules), ());
  }
  {
    Rules rules{ExactMatch(countryId, building28), ExactMatch(countryId, building29),
                ExactMatch(countryId, building30), ExactMatch(countryId, street)};
    TEST(ResultsMatch("Долгопрудный 141701", "ru", rules), ());
  }
  {
    Rules rules{ExactMatch(countryId, building31)};
    TEST(ResultsMatch("Долгопрудный 141702", "ru", rules), ());
  }

  {
    Rules rules{ExactMatch(countryId, building1)};
    TEST(ResultsMatch("london WC2H 7BX", rules), ());
    TEST(!ResultsMatch("london WC2H 7", rules), ());
    TEST(!ResultsMatch("london WC", rules), ());
  }
}

UNIT_CLASS_TEST(ProcessorTest, TestCategories)
{
  string const countryName = "Wonderland";

  TestCity sanFrancisco(m2::PointD(0, 0), "San Francisco", "en", 100 /* rank */);

  TestPOI nonameBeach(m2::PointD(0, 0), "", "ru");
  nonameBeach.SetTypes({{"natural", "beach"}});

  TestPOI namedBeach(m2::PointD(0.2, 0.2), "San Francisco beach", "en");
  namedBeach.SetTypes({{"natural", "beach"}});

  TestPOI nonameAtm(m2::PointD(0, 0), "", "en");
  nonameAtm.SetTypes({{"amenity", "atm"}});

  TestPOI namedAtm(m2::PointD(0.03, 0.03), "ATM", "en");
  namedAtm.SetTypes({{"amenity", "atm"}});

  TestPOI busStop(m2::PointD(0.00005, 0.0005), "ATM Bus Stop", "en");
  busStop.SetTypes({{"highway", "bus_stop"}});

  TestPOI cafe(m2::PointD(0.0001, 0.0001), "Cafe", "en");
  cafe.SetTypes({{"amenity", "cafe"}, {"internet_access", "wlan"}});

  TestPOI toi(m2::PointD(0.0001, 0.0001), "", "en");
  toi.SetTypes({{"amenity", "toilets"}});

  TestPOI namedResidential(m2::PointD(0.04, 0.04), "Residential", "en");
  namedResidential.SetTypes({{"landuse", "residential"}});

  BuildWorld([&](TestMwmBuilder & builder)
             {
               builder.Add(sanFrancisco);
             });
  auto wonderlandId = BuildCountry(countryName, [&](TestMwmBuilder & builder)
                                   {
                                     builder.Add(busStop);
                                     builder.Add(cafe);
                                     builder.Add(namedAtm);
                                     builder.Add(namedBeach);
                                     builder.Add(nonameAtm);
                                     builder.Add(nonameBeach);
                                     builder.Add(toi);
                                     builder.Add(namedResidential);
                                   });

  SetViewport(m2::RectD(m2::PointD(-0.5, -0.5), m2::PointD(0.5, 0.5)));

  {
    Rules const rules = {ExactMatch(wonderlandId, nonameAtm), ExactMatch(wonderlandId, namedAtm),
                         ExactMatch(wonderlandId, busStop)};

    auto request = MakeRequest("San Francisco atm");
    TEST(ResultsMatch(request->Results(), rules), ());
    for (auto const & result : request->Results())
    {
      FeaturesLoaderGuard loader(m_dataSource, wonderlandId);
      auto ft = loader.GetFeatureByIndex(result.GetFeatureID().m_index);
      TEST(ft, ());

      auto const & info = result.GetRankingInfo();

      if (busStop.Matches(*ft))
      {
        TEST(!info.m_pureCats, (result));
        TEST(info.m_falseCats, (result));
      }
      else
      {
        TEST(info.m_pureCats, (result));
        TEST(!info.m_falseCats, (result));
      }
    }
  }

  TEST(ResultsMatch("wifi", {ExactMatch(wonderlandId, cafe)}), ());
  TEST(ResultsMatch("wi-fi", {ExactMatch(wonderlandId, cafe)}), ());
  TEST(ResultsMatch("wai-fai", Rules{}), ());
  TEST(ResultsMatch("toilet", {ExactMatch(wonderlandId, toi)}), ());
  TEST(ResultsMatch("beach ",
                    {ExactMatch(wonderlandId, nonameBeach), ExactMatch(wonderlandId, namedBeach)}),
       ());
  TEST(ResultsMatch("district", {ExactMatch(wonderlandId, namedResidential)}), ());
  TEST(ResultsMatch("residential", {ExactMatch(wonderlandId, namedResidential)}), ());
}

// A separate test for the categorial search branch in the geocoder.
UNIT_CLASS_TEST(ProcessorTest, TestCategorialSearch)
{
  string const countryName = "Wonderland";

  TestCity sanDiego(m2::PointD(0, 0), "San Diego", "en", 100 /* rank */);
  TestCity homel(m2::PointD(10, 10), "Homel", "en", 100 /* rank */);

  // No need in TestHotel here, TestPOI is enough.
  TestPOI hotel1(m2::PointD(0, 0.01), "", "ru");
  hotel1.SetTypes({{"tourism", "hotel"}});

  TestPOI hotel2(m2::PointD(0, 0.02), "Hotel San Diego, California", "en");
  hotel2.SetTypes({{"tourism", "hotel"}});

  TestPOI hotelCafe(m2::PointD(0, 0.03), "Hotel", "en");
  hotelCafe.SetTypes({{"amenity", "cafe"}});

  TestPOI hotelDeVille(m2::PointD(0, 0.04), "Hôtel De Ville", "en");
  hotelDeVille.SetTypes({{"amenity", "townhall"}});

  TestPOI nightclub(m2::PointD(0, 0.05), "Moulin Rouge", "fr");
  nightclub.SetTypes({{"amenity", "nightclub"}});

  // A POI with that matches "entertainment" only by name.
  TestPOI laundry(m2::PointD(0, 0.06), "Entertainment 720", "en");
  laundry.SetTypes({{"shop", "laundry"}});

  auto const testWorldId = BuildWorld([&](TestMwmBuilder & builder) {
    builder.Add(sanDiego);
    builder.Add(homel);
  });
  auto wonderlandId = BuildCountry(countryName, [&](TestMwmBuilder & builder) {
    builder.Add(hotel1);
    builder.Add(hotel2);
    builder.Add(hotelCafe);
    builder.Add(hotelDeVille);
    builder.Add(nightclub);
    builder.Add(laundry);
  });

  SetViewport(m2::RectD(m2::PointD(-0.5, -0.5), m2::PointD(0.5, 0.5)));

  {
    Rules const rules = {ExactMatch(wonderlandId, hotel1), ExactMatch(wonderlandId, hotel2)};

    TEST(ResultsMatch("hotel ", rules), ());
    TEST(ResultsMatch("hôTeL ", rules), ());
  }

  {
    Rules const rules = {ExactMatch(wonderlandId, nightclub)};

    // A category with a rare name. The word "Entertainment"
    // occurs exactly once in the list of categories and starts
    // with a capital letter. This is a test for normalization.
    TEST(ResultsMatch("entertainment ", rules), ());
  }

  {
    Rules const rules = {ExactMatch(wonderlandId, hotel1), ExactMatch(wonderlandId, hotel2)};

    auto request = MakeRequest("гостиница ", "ru");
    TEST(ResultsMatch(request->Results(), rules), ());
  }

  {
    Rules const rules = {ExactMatch(wonderlandId, hotel1), ExactMatch(wonderlandId, hotel2)};

    // Hotel unicode character: both a synonym and and emoji.
    uint32_t const hotelEmojiCodepoint = 0x0001F3E8;
    strings::UniString const hotelUniString(1, hotelEmojiCodepoint);
    auto request = MakeRequest(ToUtf8(hotelUniString));
    TEST(ResultsMatch(request->Results(), rules), ());
  }

  {
    Rules const rules = {ExactMatch(wonderlandId, hotel1), ExactMatch(wonderlandId, hotel2),
                         ExactMatch(wonderlandId, hotelCafe), ExactMatch(testWorldId, homel),
                         ExactMatch(wonderlandId, hotelDeVille)};
    // A prefix token.
    auto request = MakeRequest("hote");
    TEST(ResultsMatch(request->Results(), rules), ());
  }

  {
    Rules const rules = {ExactMatch(wonderlandId, hotel1), ExactMatch(wonderlandId, hotel2),
                         ExactMatch(wonderlandId, hotelCafe),
                         ExactMatch(wonderlandId, hotelDeVille)};
    // It looks like a category search but we cannot tell it, so
    // even the features that match only by name are emitted.
    TEST(ResultsMatch("hotel san diego ", rules), ());
  }

  {
    Rules const rules = {ExactMatch(wonderlandId, hotel1), ExactMatch(wonderlandId, hotel2),
                         ExactMatch(wonderlandId, hotelCafe), ExactMatch(testWorldId, homel),
                         ExactMatch(wonderlandId, hotelDeVille)};
    // Homel matches exactly, other features are matched by fuzzy names.
    TEST(ResultsMatch("homel ", rules), ());
  }

  {
    Rules const rules = {ExactMatch(wonderlandId, hotel1), ExactMatch(wonderlandId, hotel2),
                         ExactMatch(wonderlandId, hotelCafe), ExactMatch(testWorldId, homel),
                         ExactMatch(wonderlandId, hotelDeVille)};
    // A typo in search: all features fit.
    TEST(ResultsMatch("hofel ", rules), ());
  }

  {
    Rules const rules = {ExactMatch(wonderlandId, hotelDeVille)};

    TEST(ResultsMatch("hotel de ville ", rules), ());
  }
}

UNIT_CLASS_TEST(ProcessorTest, TestCoords)
{
  vector<tuple<string, double, double>> tests = {
      {"51.681644 39.183481", 51.681644, 39.183481},

      {"https://maps.apple.com/maps?ll=30.3345,-81.6648&q=30.3345,-81.6648", 30.3345, -81.6648},
      {"https://maps.apple.com/maps?ll=30.3345,-81.6648&q=10.0,10.0", 30.3345, -81.6648},
      {"https://maps.apple.com/maps?q=10.0,10.0&ll=30.3345,-81.6648", 30.3345, -81.6648},
      {"https://maps.apple.com/maps?q=10.0,10.0&ll=10.0,10.0", 10.0, 10.0},

      // The first pair of coordinates in this URL belongs to the selected feature but is harder to
      // parse. The second pair (in "m=") is the viewport center.
      {"https://2gis.ru/moscow/geo/4504338361754075/"
       "37.326747%2C55.481637?m=37.371024%2C55.523592%2F9.69",
       55.523592, 37.371024},
      {"https://2gis.com.cy/cyprus?m=32.441559%2C34.767296%2F14.58", 34.767296, 32.441559},
      {"https://2gis.com.cy/cyprus/geo/70030076127247109/"
       "32.431259%2C34.771945?m=32.433265%2C34.770793%2F17.21",
       34.770793, 32.433265},

      {"https://yandex.ru/maps/?ll=158.828916%2C52.931098&z=9.1", 52.931098, 158.828916},
      {"https://yandex.ru/maps/78/petropavlovsk/?ll=158.657810%2C53.024529&z=12.99", 53.024529,
       158.657810},
      {"https://yandex.ru/maps/78/petropavlovsk/"
       "?ll=158.643359%2C53.018729&mode=whatshere&whatshere%5Bpoint%5D=158.643270%2C53.021174&"
       "whatshere%5Bzoom%5D=16.07&z=15.65",
       53.018729, 158.643359},
      {"https://yandex.com.tr/harita/115707/fatih/?ll=28.967470%2C41.008857&z=10", 41.008857,
       28.967470},

      {"http://ge0.me/kyuh76X_vf/Borgo_Maggiore", 43.941187, 12.447423},
      {"ge0://kyuh76X_vf/Borgo_Maggiore", 43.941187, 12.447423},
      {"Check out Ospedale di Stato My Places • Hospital "
       "http://ge0.me/syujRR7Xgi/Ospedale_di_Stato ge0://syujRR7Xgi/Ospedale_di_Stato",
       43.950255, 12.455579},

      {"https://en.mapy.cz/zakladni?x=37.5516243&y=55.7638088&z=12", 55.7638088, 37.5516243},
      {"https://en.mapy.cz/"
       "turisticka?moje-mapy&x=37.6575394&y=55.7253036&z=13&m3d=1&height=10605&yaw=0&pitch=-90&l=0&"
       "cat=mista-trasy",
       55.7253036, 37.6575394},
  };

  for (auto const & [query, lat, lon] : tests)
  {
    auto const request = MakeRequest(query);
    auto const & results = request->Results();
    TEST_EQUAL(results.size(), 1, ());

    auto const & result = results[0];
    TEST_EQUAL(result.GetResultType(), Result::Type::LatLon, ());
    TEST(result.HasPoint(), ());

    m2::PointD const expected = mercator::FromLatLon(lat, lon);
    auto const actual = result.GetFeatureCenter();
    auto const dist = mercator::DistanceOnEarth(actual, expected);
    TEST_LESS_OR_EQUAL(dist, 1.0, (actual, expected));
  }
}

UNIT_CLASS_TEST(ProcessorTest, HotelsFiltering)
{
  char const countryName[] = "Wonderland";

  TestHotel h1(m2::PointD(0, 0), "h1", "en", 8.0 /* rating */, 2 /* priceRate */,
               TestHotel::Type::Hotel);
  TestHotel h2(m2::PointD(0, 1), "h2", "en", 7.0 /* rating */, 5 /* priceRate */,
               TestHotel::Type::Hostel);
  TestHotel h3(m2::PointD(1, 0), "h3", "en", 9.0 /* rating */, 0 /* priceRate */,
               TestHotel::Type::GuestHouse);
  TestHotel h4(m2::PointD(1, 1), "h4", "en", 2.0 /* rating */, 4 /* priceRate */,
               TestHotel::Type::GuestHouse);

  auto id = BuildCountry(countryName, [&](TestMwmBuilder & builder) {
    builder.Add(h1);
    builder.Add(h2);
    builder.Add(h3);
    builder.Add(h4);
  });

  SearchParams params;
  params.m_query = "hotel";
  params.m_inputLocale = "en";
  params.m_viewport = m2::RectD(m2::PointD(-1, -1), m2::PointD(2, 2));
  params.m_mode = Mode::Everywhere;
  params.m_suggestsEnabled = false;

  {
    Rules rules = {ExactMatch(id, h1), ExactMatch(id, h2), ExactMatch(id, h3), ExactMatch(id, h4)};
    TEST(ResultsMatch(params, rules), ());
  }

  using namespace hotels_filter;

  params.m_hotelsFilter = And(Gt<Rating>(7.0), Le<PriceRate>(2));
  {
    Rules rules = {ExactMatch(id, h1), ExactMatch(id, h3)};
    TEST(ResultsMatch(params, rules), ());
  }

  params.m_hotelsFilter = Or(Eq<Rating>(9.0), Le<PriceRate>(4));
  {
    Rules rules = {ExactMatch(id, h1), ExactMatch(id, h3), ExactMatch(id, h4)};
    TEST(ResultsMatch(params, rules), ());
  }

  params.m_hotelsFilter = Or(And(Eq<Rating>(7.0), Eq<PriceRate>(5)), Eq<PriceRate>(4));
  {
    Rules rules = {ExactMatch(id, h2), ExactMatch(id, h4)};
    TEST(ResultsMatch(params, rules), ());
  }

  params.m_hotelsFilter = Or(Is(TestHotel::Type::GuestHouse), Is(TestHotel::Type::Hostel));
  {
    Rules rules = {ExactMatch(id, h2), ExactMatch(id, h3), ExactMatch(id, h4)};
    TEST(ResultsMatch(params, rules), ());
  }

  params.m_hotelsFilter = And(Gt<PriceRate>(3), Is(TestHotel::Type::GuestHouse));
  {
    Rules rules = {ExactMatch(id, h4)};
    TEST(ResultsMatch(params, rules), ());
  }

  params.m_hotelsFilter = OneOf((1U << static_cast<unsigned>(TestHotel::Type::Hotel)) |
                                (1U << static_cast<unsigned>(TestHotel::Type::Hostel)));
  {
    Rules rules = {ExactMatch(id, h1), ExactMatch(id, h2)};
    TEST(ResultsMatch(params, rules), ());
  }
}

UNIT_CLASS_TEST(ProcessorTest, FuzzyMatch)
{
  string const countryName = "Wonderland";
  TestCountry country(m2::PointD(10, 10), countryName, "en");

  TestCity city(m2::PointD(0, 0), "Москва", "ru", 100 /* rank */);
  TestStreet street(vector<m2::PointD>{m2::PointD(-0.001, -0.001), m2::PointD(0.001, 0.001)},
                    "Ленинградский", "ru");
  TestPOI bar(m2::PointD(0, 0), "Черчилль", "ru");
  bar.SetTypes({{"amenity", "pub"}});

  TestPOI metro(m2::PointD(5.0, 5.0), "Liceu", "es");
  metro.SetTypes({{"railway", "subway_entrance"}});

  BuildWorld([&](TestMwmBuilder & builder) {
    builder.Add(country);
    builder.Add(city);
  });

  auto id = BuildCountry(countryName, [&](TestMwmBuilder & builder) {
    builder.Add(street);
    builder.Add(bar);
    builder.Add(metro);
  });

  SetViewport(m2::RectD(m2::PointD(-1.0, -1.0), m2::PointD(1.0, 1.0)));
  {
    Rules rulesWithoutStreet = {ExactMatch(id, bar)};
    Rules rules = {ExactMatch(id, bar), ExactMatch(id, street)};
    TEST(ResultsMatch("москва черчилль", "ru", rulesWithoutStreet), ());
    TEST(ResultsMatch("москва ленинградский черчилль", "ru", rules), ());
    TEST(ResultsMatch("москва ленинградский паб черчилль", "ru", rules), ());

    TEST(ResultsMatch("масква лининградский черчиль", "ru", rules), ());
    TEST(ResultsMatch("масква ленинргадский черчиль", "ru", rules), ());

    // Too many errors, can't do anything.
    TEST(ResultsMatch("маcсква лениноргадсский чирчиль", "ru", Rules{}), ());

    TEST(ResultsMatch("моксва ленинргадский черчиль", "ru", rules), ());

    TEST(ResultsMatch("food", "ru", rulesWithoutStreet), ());
    TEST(ResultsMatch("foood", "ru", rulesWithoutStreet), ());
    TEST(ResultsMatch("fod", "ru", Rules{}), ());

    Rules rulesMetro = {ExactMatch(id, metro)};
    TEST(ResultsMatch("transporte", "es", rulesMetro), ());
    TEST(ResultsMatch("transport", "es", rulesMetro), ());
    TEST(ResultsMatch("transpurt", "en", rulesMetro), ());
    TEST(ResultsMatch("transpurrt", "es", rulesMetro), ());
    TEST(ResultsMatch("transportation", "en", Rules{}), ());
  }
}

UNIT_CLASS_TEST(ProcessorTest, SpacesInCategories)
{
  string const countryName = "Wonderland";
  TestCountry country(m2::PointD(10, 10), countryName, "en");

  TestCity city(m2::PointD(5.0, 5.0), "Москва", "ru", 100 /* rank */);
  TestPOI nightclub(m2::PointD(5.0, 5.0), "Crasy daizy", "ru");
  nightclub.SetTypes({{"amenity", "nightclub"}});

  BuildWorld([&](TestMwmBuilder & builder) {
    builder.Add(country);
    builder.Add(city);
  });

  auto id = BuildCountry(countryName, [&](TestMwmBuilder & builder) { builder.Add(nightclub); });

  {
    Rules rules = {ExactMatch(id, nightclub)};
    TEST(ResultsMatch("nightclub", "en", rules), ());
    TEST(ResultsMatch("night club", "en", rules), ());
    TEST(ResultsMatch("n i g h t c l u b", "en", Rules{}), ());
    TEST(ResultsMatch("Москва ночной клуб", "ru", rules), ());
  }
}

UNIT_CLASS_TEST(ProcessorTest, StopWords)
{
  TestCountry country(m2::PointD(0, 0), "France", "en");
  TestCity city(m2::PointD(0, 0), "Paris", "en", 100 /* rank */);
  TestStreet street(
      vector<m2::PointD>{m2::PointD(-0.001, -0.001), m2::PointD(0, 0), m2::PointD(0.001, 0.001)},
      "Rue de la Paix", "en");

  TestPOI bakery(m2::PointD(0.0, 0.0), "" /* name */, "en");
  bakery.SetTypes({{"shop", "bakery"}});

  BuildWorld([&](TestMwmBuilder & builder) {
    builder.Add(country);
    builder.Add(city);
  });

  auto id = BuildCountry(country.GetName("en"), [&](TestMwmBuilder & builder) {
    builder.Add(street);
    builder.Add(bakery);
  });

  {
    auto request = MakeRequest("la France à Paris Rue de la Paix");

    Rules rules = {ExactMatch(id, street)};

    auto const & results = request->Results();
    TEST(ResultsMatch(results, rules), ());

    auto const & info = results[0].GetRankingInfo();
    TEST_EQUAL(info.m_nameScore, NAME_SCORE_FULL_MATCH, ());
  }

  {
    Rules rules = {ExactMatch(id, bakery)};

    TEST(ResultsMatch("la boulangerie ", "fr", rules), ());
  }

  {
    TEST(ResultsMatch("la motviderie ", "fr", Rules{}), ());
    TEST(ResultsMatch("la la le la la la ", "fr", {ExactMatch(id, street)}), ());
    TEST(ResultsMatch("la la le la la la", "fr", Rules{}), ());
  }
}

UNIT_CLASS_TEST(ProcessorTest, Numerals)
{
  TestCountry country(m2::PointD(0, 0), "Беларусь", "ru");
  TestPOI school(m2::PointD(0, 0), "СШ №61", "ru");
  school.SetTypes({{"amenity", "school"}});

  auto id = BuildCountry(country.GetName("ru"), [&](TestMwmBuilder & builder) { builder.Add(school); });

  SetViewport(m2::RectD(m2::PointD(-1.0, -1.0), m2::PointD(1.0, 1.0)));
  {
    Rules rules{ExactMatch(id, school)};
    TEST(ResultsMatch("Школа 61", "ru", rules), ());
    TEST(ResultsMatch("Школа # 61", "ru", rules), ());
    TEST(ResultsMatch("school #61", "ru", rules), ());
    TEST(ResultsMatch("сш №61", "ru", rules), ());
    TEST(ResultsMatch("школа", "ru", rules), ());
  }
}

UNIT_CLASS_TEST(ProcessorTest, TestWeirdTypes)
{
  string const countryName = "Area51";

  TestCity tokyo(m2::PointD(0, 0), "東京", "ja", 100 /* rank */);

  TestStreet street(vector<m2::PointD>{m2::PointD(-8.0, 0.0), m2::PointD(8.0, 0.0)}, "竹下通り",
                    "ja");

  TestPOI defibrillator1(m2::PointD(0.0, 0.0), "" /* name */, "en");
  defibrillator1.SetTypes({{"emergency", "defibrillator"}});
  TestPOI defibrillator2(m2::PointD(-5.0, 0.0), "" /* name */, "ja");
  defibrillator2.SetTypes({{"emergency", "defibrillator"}});
  TestPOI fireHydrant(m2::PointD(2.0, 0.0), "" /* name */, "en");
  fireHydrant.SetTypes({{"emergency", "fire_hydrant"}});

  BuildWorld([&](TestMwmBuilder & builder) { builder.Add(tokyo); });

  auto countryId = BuildCountry(countryName, [&](TestMwmBuilder & builder) {
    builder.Add(street);
    builder.Add(defibrillator1);
    builder.Add(defibrillator2);
    builder.Add(fireHydrant);
  });

  {
    Rules rules{ExactMatch(countryId, defibrillator1), ExactMatch(countryId, defibrillator2)};
    TEST(ResultsMatch("defibrillator", "en", rules), ());
    TEST(ResultsMatch("除細動器", "ja", rules), ());

    Rules onlyFirst{ExactMatch(countryId, defibrillator1)};
    Rules firstWithStreet{ExactMatch(countryId, defibrillator1), ExactMatch(countryId, street)};

    // City + category. Only the first defibrillator is inside.
    TEST(ResultsMatch("東京 除細動器 ", "ja", onlyFirst), ());

    // City + street + category.
    TEST(ResultsMatch("東京 竹下通り 除細動器 ", "ja", firstWithStreet), ());
  }

  {
    Rules rules{ExactMatch(countryId, fireHydrant)};
    TEST(ResultsMatch("fire hydrant", "en", rules), ());
    TEST(ResultsMatch("гидрант", "ru", rules), ());
    TEST(ResultsMatch("пожарный гидрант", "ru", rules), ());

    TEST(ResultsMatch("fire station", "en", Rules{}), ());
  }
}

UNIT_CLASS_TEST(ProcessorTest, CityBoundaryLoad)
{
  TestCity city(vector<m2::PointD>({m2::PointD(0, 0), m2::PointD(0.5, 0), m2::PointD(0.5, 0.5),
                                    m2::PointD(0, 0.5)}),
                "moscow", "en", 100 /* rank */);
  auto const id = BuildWorld([&](TestMwmBuilder & builder) { builder.Add(city); });

  SetViewport(m2::RectD(m2::PointD(-1.0, -1.0), m2::PointD(1.0, 1.0)));
  {
    Rules const rules{ExactMatch(id, city)};
    TEST(ResultsMatch("moscow", "en", rules), ());
  }

  CitiesBoundariesTable table(m_dataSource);
  TEST(table.Load(), ());
  TEST(table.Has(0 /* fid */), ());
  TEST(!table.Has(10 /* fid */), ());

  CitiesBoundariesTable::Boundaries boundaries;
  TEST(table.Get(0 /* fid */, boundaries), ());
  TEST(boundaries.HasPoint(m2::PointD(0, 0)), ());
  TEST(boundaries.HasPoint(m2::PointD(0.5, 0)), ());
  TEST(boundaries.HasPoint(m2::PointD(0.5, 0.5)), ());
  TEST(boundaries.HasPoint(m2::PointD(0, 0.5)), ());
  TEST(boundaries.HasPoint(m2::PointD(0.25, 0.25)), ());

  TEST(!boundaries.HasPoint(m2::PointD(0.6, 0.6)), ());
  TEST(!boundaries.HasPoint(m2::PointD(-1, 0.5)), ());
}

UNIT_CLASS_TEST(ProcessorTest, CityBoundarySmoke)
{
  TestCity moscow(vector<m2::PointD>({m2::PointD(0, 0), m2::PointD(0.5, 0), m2::PointD(0.5, 0.5),
                                      m2::PointD(0, 0.5)}),
                  "Москва", "ru", 100 /* rank */);
  TestCity khimki(vector<m2::PointD>({m2::PointD(0.25, 0.5), m2::PointD(0.5, 0.5),
                                      m2::PointD(0.5, 0.75), m2::PointD(0.25, 0.75)}),
                  "Химки", "ru", 50 /* rank */);

  TestPOI cafeMoscow(m2::PointD(0.49, 0.49), "Москвичка", "ru");
  cafeMoscow.SetTypes({{"amenity", "cafe"}, {"internet_access", "wlan"}});

  TestPOI cafeKhimki(m2::PointD(0.49, 0.51), "Химичка", "ru");
  cafeKhimki.SetTypes({{"amenity", "cafe"}, {"internet_access", "wlan"}});

  BuildWorld([&](TestMwmBuilder & builder) {
    builder.Add(moscow);
    builder.Add(khimki);
  });

  auto countryId = BuildCountry("Россия" /* countryName */, [&](TestMwmBuilder & builder) {
    builder.Add(cafeMoscow);
    builder.Add(cafeKhimki);
  });

  SetViewport(m2::RectD(m2::PointD(-1.0, -1.0), m2::PointD(1.0, 1.0)));

  {
    auto request = MakeRequest("кафе", "ru");
    auto const & results = request->Results();

    Rules rules{ExactMatch(countryId, cafeMoscow), ExactMatch(countryId, cafeKhimki)};
    TEST(ResultsMatch(results, rules), ());

    for (auto const & result : results)
    {
      if (ResultMatches(result, ExactMatch(countryId, cafeMoscow)))
      {
        TEST_EQUAL(result.GetAddress(), "Москва, Россия", ());
      }
      else
      {
        TEST(ResultMatches(result, ExactMatch(countryId, cafeKhimki)), ());
        TEST_EQUAL(result.GetAddress(), "Химки, Россия", ());
      }
    }
  }
}

// Tests for the non-strict aspects of retrieval.
// Currently, the only possible non-strictness is that
// some tokens in the query may be ignored,
// which results in a pruned parse tree for the query.
UNIT_CLASS_TEST(ProcessorTest, RelaxedRetrieval)
{
  string const countryName = "Wonderland";
  TestCountry country(m2::PointD(10.0, 10.0), countryName, "en");

  TestCity city({{-10.0, -10.0}, {10.0, -10.0}, {10.0, 10.0}, {-10.0, 10.0}} /* boundary */,
                "Sick City", "en", 255 /* rank */);

  TestStreet street(vector<m2::PointD>{m2::PointD(-1.0, 0.0), m2::PointD(1.0, 0.0)}, "Queer Street",
                    "en");
  TestBuilding building0(m2::PointD(-1.0, 0.0), "" /* name */, "0", street.GetName("en"), "en");
  TestBuilding building1(m2::PointD(1.0, 0.0), "", "1", street.GetName("en"), "en");
  TestBuilding building2(m2::PointD(2.0, 0.0), "named building", "" /* house number */, "en");
  TestBuilding building3(m2::PointD(3.0, 0.0), "named building", "", "en");

  TestPOI poi0(m2::PointD(-1.0, 0.0), "Farmacia de guardia", "en");
  poi0.SetTypes({{"amenity", "pharmacy"}});

  // A poi inside building2.
  TestPOI poi2(m2::PointD(2.0, 0.0), "Post box", "en");
  poi2.SetTypes({{"amenity", "post_box"}});

  auto countryId = BuildCountry(countryName, [&](TestMwmBuilder & builder) {
    builder.Add(street);
    builder.Add(building0);
    builder.Add(building1);
    builder.Add(poi0);
  });
  RegisterCountry(countryName, m2::RectD(m2::PointD(-10.0, -10.0), m2::PointD(10.0, 10.0)));

  auto worldId = BuildWorld([&](TestMwmBuilder & builder) {
    builder.Add(country);
    builder.Add(city);
  });

  {
    Rules rulesStrict = {ExactMatch(countryId, building0)};
    Rules rulesRelaxed = {ExactMatch(countryId, street)};

    // "street" instead of "street-building"
    TEST(ResultsMatch("queer street 0 ", rulesStrict), ());
    TEST(ResultsMatch("queer street ", rulesRelaxed), ());
    TEST(ResultsMatch("queer street 2 ", rulesRelaxed), ());
  }

  {
    Rules rulesStrict = {ExactMatch(countryId, poi0), ExactMatch(countryId, street)};
    Rules rulesRelaxed = {ExactMatch(countryId, street)};

    // "country-city-street" instead of "country-city-street-poi"
    TEST(ResultsMatch("wonderland sick city queer street pharmacy ", rulesStrict), ());
    TEST(ResultsMatch("wonderland sick city queer street school ", rulesRelaxed), ());
  }

  {
    Rules rulesStrict = {ExactMatch(countryId, street)};
    Rules rulesRelaxed = {ExactMatch(worldId, city)};

    // Should return relaxed result for city.
    // "city" instead of "city-street"
    TEST(ResultsMatch("sick city queer street ", rulesStrict), ());
    TEST(ResultsMatch("sick city sick street ", rulesRelaxed), ());
  }

  {
    Rules rulesStrict = {ExactMatch(countryId, street)};
    Rules rulesRelaxed = {ExactMatch(worldId, city)};

    // Should return relaxed result for city.
    // "country-city" instead of "country-city-street"
    TEST(ResultsMatch("wonderland sick city queer street ", rulesStrict), ());
    TEST(ResultsMatch("wonderland sick city other street ", rulesRelaxed), ());
  }

  {
    Rules rulesStrict = {ExactMatch(countryId, poi0)};
    Rules rulesRelaxed = {ExactMatch(worldId, city)};

    // Should return relaxed result for city.
    // "city" instead of "city-poi"
    TEST(ResultsMatch("sick city pharmacy ", rulesStrict), ());
    TEST(ResultsMatch("sick city library ", rulesRelaxed), ());
  }
}

UNIT_CLASS_TEST(ProcessorTest, PathsThroughLayers)
{
  string const countryName = "Science Country";

  TestCountry scienceCountry(m2::PointD(0.0, 0.0), countryName, "en");

  TestCity mathTown(vector<m2::PointD>({m2::PointD(-100.0, -100.0), m2::PointD(100.0, -100.0),
                                        m2::PointD(100.0, 100.0), m2::PointD(-100.0, 100.0)}),
                    "Math Town", "en", 100 /* rank */);

  TestStreet computingStreet(
      vector<m2::PointD>{m2::PointD{-16.0, -16.0}, m2::PointD(0.0, 0.0), m2::PointD(16.0, 16.0)},
      "Computing street", "en");

  TestBuilding statisticalLearningBuilding(m2::PointD(8.0, 8.0), "Statistical Learning, Inc.", "0",
                                           computingStreet.GetName("en"), "en");

  TestPOI reinforcementCafe(m2::PointD(8.0, 8.0), "Trattoria Reinforcemento", "en");
  reinforcementCafe.SetTypes({{"amenity", "cafe"}});

  BuildWorld([&](TestMwmBuilder & builder) {
    builder.Add(scienceCountry);
    builder.Add(mathTown);
  });
  RegisterCountry(countryName, m2::RectD(m2::PointD(-100.0, -100.0), m2::PointD(100.0, 100.0)));

  auto countryId = BuildCountry(countryName, [&](TestMwmBuilder & builder) {
    builder.Add(computingStreet);
    builder.Add(statisticalLearningBuilding);
    builder.Add(reinforcementCafe);
  });

  SetViewport(m2::RectD(m2::PointD(-100.0, -100.0), m2::PointD(100.0, 100.0)));

  for (auto const mode :
       {FeaturesLayerPathFinder::MODE_AUTO, FeaturesLayerPathFinder::MODE_TOP_DOWN,
        FeaturesLayerPathFinder::MODE_BOTTOM_UP})
  {
    FeaturesLayerPathFinder::SetModeForTesting(mode);
    SCOPE_GUARD(rollbackMode, [&] {
      FeaturesLayerPathFinder::SetModeForTesting(FeaturesLayerPathFinder::MODE_AUTO);
    });

    auto const ruleStreet = ExactMatch(countryId, computingStreet);
    auto const ruleBuilding = ExactMatch(countryId, statisticalLearningBuilding);
    auto const rulePoi = ExactMatch(countryId, reinforcementCafe);

    // POI-BUILDING-STREET
    TEST(ResultsMatch("computing street statistical learning cafe ", {rulePoi, ruleStreet}), ());
    TEST(ResultsMatch("computing street 0 cafe ", {rulePoi}), ());

    // POI-BUILDING is not supported
    TEST(ResultsMatch("statistical learning cafe ", {}), ());
    TEST(ResultsMatch("0 cafe ", {}), ());

    // POI-STREET
    TEST(ResultsMatch("computing street cafe ", {rulePoi, ruleStreet}), ());

    // BUILDING-STREET
    TEST(ResultsMatch("computing street statistical learning ", {ruleBuilding, ruleStreet}), ());
    TEST(ResultsMatch("computing street 0 ", {ruleBuilding}), ());

    // POI
    TEST(ResultsMatch("cafe ", {rulePoi}), ());

    // BUILDING
    TEST(ResultsMatch("statistical learning ", {ruleBuilding}), ());
    // Cannot find anything only by a house number.
    TEST(ResultsMatch("0 ", Rules{}), ());

    // STREET
    TEST(ResultsMatch("computing street ", {ruleStreet}), ());
  }
}

UNIT_CLASS_TEST(ProcessorTest, SelectProperName)
{
  string const countryName = "Wonderland";

  auto testLanguage = [&](string language, string expectedRes) {
    auto request = MakeRequest("cafe", language);
    auto const & results = request->Results();
    TEST_EQUAL(results.size(), 1, (results));
    TEST_EQUAL(results[0].GetString(), expectedRes, (results));
  };

  TestMultilingualPOI cafe(
      m2::PointD(0.0, 0.0), "Default",
      {{"es", "Spanish"}, {"int_name", "International"}, {"fr", "French"}, {"ru", "Russian"}});
  cafe.SetTypes({{"amenity", "cafe"}});

  auto wonderlandId = BuildCountry(countryName, [&](TestMwmBuilder & builder) {
    builder.Add(cafe);
    builder.SetMwmLanguages({"it", "fr"});
  });

  SetViewport(m2::RectD(-1, -1, 1, 1));

  // Language priorities:
  // - device language
  // - input language
  // - default if mwm has device or input region
  // - english and international
  // - default

  // Device language is English by default.

  // No English(device) or Italian(query) name present but mwm has Italian region.
  testLanguage("it", "Default");

  // No English(device) name present. Mwm has French region but we should prefer explicit French
  // name.
  testLanguage("fr", "French");

  // No English(device) name present. Use query language.
  testLanguage("es", "Spanish");

  // No English(device) or German(query) names present. Mwm does not have German region. We should
  // use international name.
  testLanguage("de", "International");

  // Set Russian as device language.
  m_engine.SetLocale("ru");

  // Device language(Russian) is present and preferred for all queries.
  testLanguage("it", "Russian");
  testLanguage("fr", "Russian");
  testLanguage("es", "Russian");
  testLanguage("de", "Russian");
}

UNIT_CLASS_TEST(ProcessorTest, CuisineTest)
{
  string const countryName = "Wonderland";

  TestPOI vegan(m2::PointD(1.0, 1.0), "Useless name", "en");
  vegan.SetTypes({{"amenity", "cafe"}, {"cuisine", "vegan"}});

  TestPOI pizza(m2::PointD(1.0, 1.0), "Useless name", "en");
  pizza.SetTypes({{"amenity", "bar"}, {"cuisine", "pizza"}});

  auto countryId = BuildCountry(countryName, [&](TestMwmBuilder & builder) {
    builder.Add(vegan);
    builder.Add(pizza);
  });

  SetViewport(m2::RectD(-1, -1, 1, 1));

  {
    Rules rules{ExactMatch(countryId, vegan)};
    TEST(ResultsMatch("vegan ", "en", rules), ());
  }

  {
    Rules rules{ExactMatch(countryId, pizza)};
    TEST(ResultsMatch("pizza ", "en", rules), ());
    TEST(ResultsMatch("pizzeria ", "en", rules), ());
  }
}

UNIT_CLASS_TEST(ProcessorTest, AirportTest)
{
  string const countryName = "Wonderland";

  TestAirport vko(m2::PointD(1.0, 1.0), "Useless name", "en", "VKO");
  TestAirport svo(m2::PointD(1.0, 1.0), "Useless name", "en", "SVO");

  auto countryId = BuildCountry(countryName, [&](TestMwmBuilder & builder) {
    builder.Add(vko);
    builder.Add(svo);
  });

  SetViewport(m2::RectD(-1, -1, 1, 1));

  {
    Rules rules{ExactMatch(countryId, vko)};
    TEST(ResultsMatch("vko ", "en", rules), ());
  }

  {
    Rules rules{ExactMatch(countryId, svo)};
    TEST(ResultsMatch("svo ", "en", rules), ());
  }
}

UNIT_CLASS_TEST(ProcessorTest, OperatorTest)
{
  string const countryName = "Wonderland";

  TestATM sber(m2::PointD(1.0, 1.0), "Sberbank", "en");
  TestATM alfa(m2::PointD(1.0, 1.0), "Alfa bank", "en");

  auto countryId = BuildCountry(countryName, [&](TestMwmBuilder & builder) {
    builder.Add(sber);
    builder.Add(alfa);
  });

  SetViewport(m2::RectD(-1, -1, 1, 1));

  {
    Rules rules{ExactMatch(countryId, sber)};
    TEST(ResultsMatch("sberbank ", "en", rules), ());
    TEST(ResultsMatch("sberbank atm ", "en", rules), ());
    TEST(ResultsMatch("atm sberbank ", "en", rules), ());
  }

  {
    Rules rules{ExactMatch(countryId, alfa)};
    TEST(ResultsMatch("alfa bank ", "en", rules), ());
    TEST(ResultsMatch("alfa bank atm ", "en", rules), ());
    TEST(ResultsMatch("alfa atm ", "en", rules), ());
    TEST(ResultsMatch("atm alfa bank ", "en", rules), ());
    TEST(ResultsMatch("atm alfa ", "en", rules), ());
  }
}

UNIT_CLASS_TEST(ProcessorTest, BrandTest)
{
  string const countryName = "Wonderland";

  TestBrandFeature mac(m2::PointD(1.0, 1.0), "mcdonalds", "en");
  TestBrandFeature sw(m2::PointD(1.0, 1.0), "subway", "en");

  auto countryId = BuildCountry(countryName, [&](TestMwmBuilder & builder) {
    builder.Add(mac);
    builder.Add(sw);
  });

  SetViewport(m2::RectD(-1, -1, 1, 1));

  {
    Rules rules{ExactMatch(countryId, mac)};
    TEST(ResultsMatch("McDonald's", "en", rules), ());
    TEST(ResultsMatch("Mc Donalds", "en", rules), ());
    TEST(ResultsMatch("МакДональд'с", "ru", rules), ());
    TEST(ResultsMatch("Мак Доналдс", "ru", rules), ());
    TEST(ResultsMatch("マクドナルド", "ja", rules), ());
  }

  {
    Rules rules{ExactMatch(countryId, sw)};
    TEST(ResultsMatch("Subway cafe", "en", rules), ());
    TEST(ResultsMatch("Сабвэй", "ru", rules), ());
    TEST(ResultsMatch("サブウェイ", "ja", rules), ());
  }
}

UNIT_CLASS_TEST(ProcessorTest, SquareAsStreetTest)
{
  string const countryName = "Wonderland";

  TestSquare square(m2::RectD(0.0, 0.0, 1.0, 1.0), "revolution square", "en");

  TestPOI nonameHouse(m2::PointD(1.0, 1.0), "", "en");
  nonameHouse.SetHouseNumber("3");
  nonameHouse.SetStreetName(square.GetName("en"));

  auto countryId = BuildCountry(countryName, [&](TestMwmBuilder & builder)
                                   {
                                     builder.Add(square);
                                     builder.Add(nonameHouse);
                                   });

  SetViewport(m2::RectD(m2::PointD(0.0, 0.0), m2::PointD(1.0, 2.0)));
  {
    Rules rules = {ExactMatch(countryId, nonameHouse)};
    TEST(ResultsMatch("revolution square 3", rules), ());
  }
}

UNIT_CLASS_TEST(ProcessorTest, CountrySynonymsTest)
{
  TestCountry usa(m2::PointD(0.5, 0.5), "United States of America", "en");
  TestPOI alabama(m2::PointD(0.5, 0.5), "Alabama", "en");
  alabama.SetTypes({{"place", "state"}});

  auto worldId = BuildWorld([&](TestMwmBuilder & builder)
                            {
                              builder.Add(usa);
                              builder.Add(alabama);
                            });

  SetViewport(m2::RectD(m2::PointD(0.0, 0.0), m2::PointD(1.0, 1.0)));
  {
    Rules rules = {ExactMatch(worldId, usa)};
    TEST(ResultsMatch("United States of America", rules), ());
    TEST(ResultsMatch("USA", rules), ());
    TEST(ResultsMatch("US", rules), ());
  }

  {
    Rules rules = {ExactMatch(worldId, alabama)};
    TEST(ResultsMatch("Alabama", rules), ());
    TEST(ResultsMatch("AL", rules), ());
  }
}

UNIT_CLASS_TEST(ProcessorTest, SynonymsTest)
{
  string const countryName = "Wonderland";

  TestStreet streetEn(
      vector<m2::PointD>{m2::PointD(0.5, -0.5), m2::PointD(0.0, 0.0), m2::PointD(-0.5, 0.5)},
      "Southwest street", "en");

  TestStreet streetRu(
      vector<m2::PointD>{m2::PointD(-0.5, -0.5), m2::PointD(0.0, 0.0), m2::PointD(0.5, 0.5)},
      "большая свято-покровская улица", "ru");

  TestPOI stPeterEn(m2::PointD(2.0, 2.0), "saint peter basilica", "en");
  TestPOI stPeterRu(m2::PointD(-2.0, -2.0), "собор святого петра", "ru");

  auto wonderlandId = BuildCountry(countryName, [&](TestMwmBuilder & builder) {
    builder.Add(streetEn);
    builder.Add(streetRu);
    builder.Add(stPeterEn);
    builder.Add(stPeterRu);
  });

  SetViewport(m2::RectD(-2.0, -2.0, 2.0, 2.0));
  {
    Rules rules = {ExactMatch(wonderlandId, streetEn)};
    TEST(ResultsMatch("southwest street ", rules), ());
    TEST(ResultsMatch("sw street ", rules), ());
    TEST(ResultsMatch("southwest str ", rules), ());
    TEST(ResultsMatch("sw str ", rules), ());
    TEST(ResultsMatch("southwest st ", rules), ());
    TEST(ResultsMatch("sw st ", rules), ());
  }
  {
    Rules rules = {ExactMatch(wonderlandId, streetRu)};
    TEST(ResultsMatch("большая свято покровская улица ", rules), ());
    TEST(ResultsMatch("бол свято покровская улица ", rules), ());
    TEST(ResultsMatch("б свято покровская улица ", rules), ());
    TEST(ResultsMatch("большая св покровская улица ", rules), ());
    TEST(ResultsMatch("бол св покровская улица ", rules), ());
    TEST(ResultsMatch("б св покровская улица ", rules), ());
    TEST(ResultsMatch("большая свято покровская ул ", rules), ());
    TEST(ResultsMatch("бол свято покровская ул ", rules), ());
    TEST(ResultsMatch("б свято покровская ул ", rules), ());
    TEST(ResultsMatch("большая св покровская ул ", rules), ());
    TEST(ResultsMatch("бол св покровская ул ", rules), ());
    TEST(ResultsMatch("б св покровская ул ", rules), ());
  }
  {
    Rules rules = {ExactMatch(wonderlandId, stPeterEn)};
    TEST(ResultsMatch("saint peter basilica ", rules), ());
    TEST(ResultsMatch("st peter basilica ", rules), ());
  }
  {
    Rules rules = {ExactMatch(wonderlandId, stPeterRu)};
    Rules relaxedRules = {ExactMatch(wonderlandId, stPeterRu), ExactMatch(wonderlandId, streetRu)};
    TEST(ResultsMatch("собор святого петра ", rules), ());
    TEST(ResultsMatch("собор св петра ", relaxedRules), ());
  }
}

UNIT_CLASS_TEST(ProcessorTest, PreprocessBeforeTokenizationTest)
{
  string const countryName = "Wonderland";

  TestStreet prt(
      vector<m2::PointD>{m2::PointD(0.5, -0.5), m2::PointD(0.0, 0.0), m2::PointD(-0.5, 0.5)},
      "Октябрьский проспект", "ru");

  TestStreet prd(
      vector<m2::PointD>{m2::PointD(-0.5, -0.5), m2::PointD(0.0, 0.0), m2::PointD(0.5, 0.5)},
      "Жуков проезд", "ru");

  TestStreet nabya(
      vector<m2::PointD>{m2::PointD(0.0, -0.5), m2::PointD(0.0, 0.0), m2::PointD(0.0, 0.5)},
      "Москворецкая набережная", "ru");

  auto wonderlandId = BuildCountry(countryName, [&](TestMwmBuilder & builder) {
    builder.Add(prt);
    builder.Add(prd);
    builder.Add(nabya);
  });

  SetViewport(m2::RectD(-1, -1, 1, 1));
  {
    Rules rules = {ExactMatch(wonderlandId, prt)};
    TEST(ResultsMatch("Октябрьский проспект", rules), ());
    TEST(ResultsMatch("пр-т Октябрьский", rules), ());
    TEST(ResultsMatch("Октябрьский пр-т", rules), ());
  }
  {
    Rules rules = {ExactMatch(wonderlandId, prd)};
    TEST(ResultsMatch("Жуков проезд", rules), ());
    TEST(ResultsMatch("пр-д Жуков", rules), ());
    TEST(ResultsMatch("Жуков пр-д", rules), ());
  }
  {
    Rules rules = {ExactMatch(wonderlandId, nabya)};
    TEST(ResultsMatch("Москворецкая набережная", rules), ());
    TEST(ResultsMatch("наб-я Москворецкая", rules), ());
    TEST(ResultsMatch("Москворецкая наб-я", rules), ());
  }
}

UNIT_CLASS_TEST(ProcessorTest, StreetNameLocaleTest)
{
  string const countryName = "Wonderland";

  StringUtf8Multilang streetName;
  streetName.AddString("default", "default");
  streetName.AddString("en", "english");
  streetName.AddString("ja", "japanese");
  TestStreet street(
      vector<m2::PointD>{m2::PointD(0.0, -0.5), m2::PointD(0.0, 0.0), m2::PointD(0.0, 0.5)},
      streetName);

  TestPOI nonameHouse(m2::PointD(0.0, 0.0), "", "en");
  nonameHouse.SetHouseNumber("3");
  nonameHouse.SetStreetName(street.GetName("ja"));

  auto countryId = BuildCountry(countryName, [&](TestMwmBuilder & builder) {
    builder.Add(street);
    builder.Add(nonameHouse);
  });

  SetViewport(m2::RectD(m2::PointD(0.0, 0.0), m2::PointD(1.0, 1.0)));
  {
    Rules rules = {ExactMatch(countryId, nonameHouse)};
    TEST(ResultsMatch("default 3", rules), ());
  }
}

UNIT_CLASS_TEST(ProcessorTest, RemoveDuplicatingStreets)
{
  string const countryName = "Wonderland";
  string const streetName = "Октябрьский проспект";

  // Distance between centers should be less than 5km.
  TestStreet street1(vector<m2::PointD>{m2::PointD(0.0, 0.0), m2::PointD(0.0, 0.01)},
                     streetName, "ru");
  street1.SetHighwayType("primary");
  TestStreet street2(vector<m2::PointD>{m2::PointD(0.0, 0.01), m2::PointD(0.0, 0.02)},
                     streetName, "ru");
  street1.SetHighwayType("secondary");

  auto wonderlandId = BuildCountry(countryName, [&](TestMwmBuilder & builder) {
    builder.Add(street1);
    builder.Add(street2);
  });

  SetViewport(m2::RectD(-1, -1, 1, 1));
  {
    TEST_EQUAL(GetResultsNumber(streetName, "ru"), 1, ());
  }
}

UNIT_CLASS_TEST(ProcessorTest, ExactMatchTest)
{
  string const countryName = "Wonderland";

  TestCafe lermontov(m2::PointD(1, 1), "Лермонтовъ", "ru");

  TestCity lermontovo(m2::PointD(-1, -1), "Лермонтово", "ru", 0 /* rank */);
  TestCafe cafe(m2::PointD(-1.01, -1.01), "", "ru");

  auto worldId = BuildWorld([&](TestMwmBuilder & builder) { builder.Add(lermontovo); });
  auto wonderlandId = BuildCountry(countryName, [&](TestMwmBuilder & builder) {
    builder.Add(cafe);
    builder.Add(lermontov);
  });

  {
    auto request = MakeRequest("cafe лермонтовъ ");
    auto const & results = request->Results();

    Rules rules{ExactMatch(wonderlandId, cafe), ExactMatch(wonderlandId, lermontov)};
    TEST(ResultsMatch(results, rules), ());

    TEST_EQUAL(2, results.size(), ("Unexpected number of retrieved cafes."));
    TEST(ResultsMatch({results[0]}, {ExactMatch(wonderlandId, lermontov)}), ());
    TEST(results[0].GetRankingInfo().m_exactMatch, ());
    TEST(!results[1].GetRankingInfo().m_exactMatch, ());
  }

  {
    auto request = MakeRequest("cafe лермонтово ");
    auto results = request->Results();

    Rules rules{ExactMatch(wonderlandId, cafe), ExactMatch(wonderlandId, lermontov)};
    TEST(ResultsMatch(results, rules), ());

    TEST_EQUAL(2, results.size(), ("Unexpected number of retrieved cafes."));
    if (!ResultsMatch({results[0]}, {ExactMatch(wonderlandId, cafe)}))
      swap(results[0], results[1]);

    TEST(ResultsMatch({results[0]}, {ExactMatch(wonderlandId, cafe)}), ());
    TEST(ResultsMatch({results[1]}, {ExactMatch(wonderlandId, lermontov)}), ());
    TEST(results[0].GetRankingInfo().m_exactMatch, ());
    TEST(!results[1].GetRankingInfo().m_exactMatch, ());
  }

  {
    auto request = MakeRequest("cafe лермонтов ");
    auto const & results = request->Results();

    Rules rules{ExactMatch(wonderlandId, cafe), ExactMatch(wonderlandId, lermontov)};
    TEST(ResultsMatch(results, rules), ());

    TEST_EQUAL(2, results.size(), ("Unexpected number of retrieved cafes."));
    TEST(!results[0].GetRankingInfo().m_exactMatch, ());
    TEST(!results[1].GetRankingInfo().m_exactMatch, ());
  }

  {
    auto request = MakeRequest("cafe лермонтов");
    auto const & results = request->Results();

    Rules rules{ExactMatch(wonderlandId, cafe), ExactMatch(wonderlandId, lermontov)};
    TEST(ResultsMatch(results, rules), ());

    TEST_EQUAL(2, results.size(), ("Unexpected number of retrieved cafes."));
    TEST(results[0].GetRankingInfo().m_exactMatch, ());
    TEST(results[1].GetRankingInfo().m_exactMatch, ());
  }

  {
    auto request = MakeRequest("cafe лер");
    auto const & results = request->Results();

    Rules rules{ExactMatch(wonderlandId, cafe), ExactMatch(wonderlandId, lermontov)};
    TEST(ResultsMatch(results, rules), ());

    TEST_EQUAL(2, results.size(), ("Unexpected number of retrieved cafes."));
    TEST(results[0].GetRankingInfo().m_exactMatch, ());
    TEST(results[1].GetRankingInfo().m_exactMatch, ());
  }
}

UNIT_CLASS_TEST(ProcessorTest, StreetSynonymPrefix)
{
  string const countryName = "Wonderland";

  // Est is a prefix of "estrada".
  TestStreet street(vector<m2::PointD>{m2::PointD(-1.0, -1.0), m2::PointD(1.0, 1.0)},
                    "Boulevard Maloney Est", "en");

  TestPOI house(m2::PointD(1.0, 1.0), "", "en");
  house.SetHouseNumber("3");
  house.SetStreetName(street.GetName("en"));

  auto countryId = BuildCountry(countryName, [&](TestMwmBuilder & builder) {
    builder.Add(street);
    builder.Add(house);
  });

  SetViewport(m2::RectD(m2::PointD(0.0, 0.0), m2::PointD(1.0, 2.0)));
  {
    Rules rules = {ExactMatch(countryId, house), ExactMatch(countryId, street)};
    TEST(ResultsMatch("3 Boulevard Maloney Est", rules), ());
  }
}

UNIT_CLASS_TEST(ProcessorTest, Strasse)
{
  string const countryName = "Wonderland";

  TestStreet s1(vector<m2::PointD>{m2::PointD(-1.0, -1.0), m2::PointD(1.0, 1.0)}, "abcdstraße",
                "en");
  TestStreet s2(vector<m2::PointD>{m2::PointD(1.0, -1.0), m2::PointD(-1.0, 1.0)}, "xyz strasse",
                "en");

  auto countryId = BuildCountry(countryName, [&](TestMwmBuilder & builder) {
    builder.Add(s1);
    builder.Add(s2);
  });

  auto checkNoErrors = [&](string const & query, Rules const & rules) {
    auto request = MakeRequest(query, "en");
    auto const & results = request->Results();

    TEST(ResultsMatch(results, rules), (query));
    TEST_EQUAL(results.size(), 1, (query));
    TEST_EQUAL(results[0].GetRankingInfo().m_errorsMade, ErrorsMade(0), (query));
    auto const nameScore = results[0].GetRankingInfo().m_nameScore;
    TEST(nameScore == NAME_SCORE_FULL_MATCH || nameScore == NAME_SCORE_PREFIX, (query));
  };

  SetViewport(m2::RectD(m2::PointD(0.0, 0.0), m2::PointD(1.0, 2.0)));
  {
    Rules rules = {ExactMatch(countryId, s1)};
    checkNoErrors("abcdstrasse ", rules);
    checkNoErrors("abcdstrasse", rules);
    checkNoErrors("abcdstr ", rules);
    checkNoErrors("abcdstr", rules);
    checkNoErrors("abcdstraße ", rules);
    checkNoErrors("abcdstraße", rules);
    checkNoErrors("abcd strasse ", rules);
    checkNoErrors("abcd strasse", rules);
    checkNoErrors("abcd straße ", rules);
    checkNoErrors("abcd straße", rules);
    checkNoErrors("abcd ", rules);
    checkNoErrors("abcd", rules);
  }
  {
    Rules rules = {ExactMatch(countryId, s2)};
    checkNoErrors("xyzstrasse ", rules);
    checkNoErrors("xyzstrasse", rules);
    checkNoErrors("xyzstr ", rules);
    checkNoErrors("xyzstr", rules);
    checkNoErrors("xyzstraße ", rules);
    checkNoErrors("xyzstraße", rules);
    checkNoErrors("xyz strasse ", rules);
    checkNoErrors("xyz strasse", rules);
    checkNoErrors("xyz straße ", rules);
    checkNoErrors("xyz straße", rules);
    checkNoErrors("xyz ", rules);
    checkNoErrors("xyz", rules);
  }
}

UNIT_CLASS_TEST(ProcessorTest, StreetSynonymsWithMisprints)
{
  string const countryName = "Wonderland";

  TestStreet leninsky(vector<m2::PointD>{m2::PointD(0.0, -1.0), m2::PointD(0.0, 1.0)},
                      "Ленинский проспект", "ru");
  TestStreet nabrezhnaya(vector<m2::PointD>{m2::PointD(1.0, -1.0), m2::PointD(1.0, 1.0)},
                         "улица набрежная", "ru");
  TestStreet naberezhnaya(vector<m2::PointD>{m2::PointD(2.0, -1.0), m2::PointD(2.0, 1.0)},
                          "улица набережная", "ru");

  auto countryId = BuildCountry(countryName, [&](TestMwmBuilder & builder) {
    builder.Add(leninsky);
    builder.Add(nabrezhnaya);
    builder.Add(naberezhnaya);
  });

  SetViewport(m2::RectD(m2::PointD(0.0, -1.0), m2::PointD(2.0, 1.0)));
  {
    Rules rules = {ExactMatch(countryId, leninsky)};
    TEST(ResultsMatch("ленинский проспект", rules), ());
    TEST(ResultsMatch("ленинский пропект", rules), ());
    TEST(ResultsMatch("ленинский", rules), ());
  }
  {
    Rules rules = {ExactMatch(countryId, nabrezhnaya), ExactMatch(countryId, naberezhnaya)};
    TEST(ResultsMatch("улица набрежная", rules), ());
    TEST(ResultsMatch("набрежная", rules), ());
  }
  {
    Rules rules = {ExactMatch(countryId, naberezhnaya)};
    TEST(ResultsMatch("улица набережная", rules), ());
  }
}

UNIT_CLASS_TEST(ProcessorTest, HouseOnStreetSynonymsWithMisprints)
{
  string const countryName = "Wonderland";

  TestStreet tverskoi(vector<m2::PointD>{m2::PointD(1.0, -1.0), m2::PointD(1.0, 1.0)},
                      "Tverskoi Boulevard", "en");
  TestStreet leninsky(vector<m2::PointD>{m2::PointD(0.0, -1.0), m2::PointD(0.0, 1.0)},
                      "Leninsky Avenue", "en");
  TestStreet mira(vector<m2::PointD>{m2::PointD(-1.0, -1.0), m2::PointD(-1.0, 1.0)},
                  "Проспект Мира", "ru");

  TestPOI houseTverskoi(m2::PointD(1.0, 0.0), "", "en");
  houseTverskoi.SetHouseNumber("3");
  houseTverskoi.SetStreetName(tverskoi.GetName("en"));

  TestPOI houseLeninsky(m2::PointD(0.0, 0.0), "", "en");
  houseLeninsky.SetHouseNumber("5");
  houseLeninsky.SetStreetName(leninsky.GetName("en"));

  TestPOI houseMira(m2::PointD(-1.0, 0.0), "", "en");
  houseMira.SetHouseNumber("7");
  houseMira.SetStreetName(mira.GetName("ru"));

  auto countryId = BuildCountry(countryName, [&](TestMwmBuilder & builder) {
    builder.Add(tverskoi);
    builder.Add(leninsky);
    builder.Add(mira);
    builder.Add(houseTverskoi);
    builder.Add(houseLeninsky);
    builder.Add(houseMira);
  });

  SetViewport(m2::RectD(m2::PointD(-1.0, -1.0), m2::PointD(1.0, 1.0)));
  {
    Rules rules = {ExactMatch(countryId, houseTverskoi)};
    Rules rulesWithStreet = {ExactMatch(countryId, houseTverskoi), ExactMatch(countryId, tverskoi)};
    TEST(AlternativeMatch("tverskoi 3", {rules, rulesWithStreet}), ());
    TEST(AlternativeMatch("tverskoi boulevard 3", {rules, rulesWithStreet}), ());
    TEST(AlternativeMatch("tverskoi bulevard 3", {rules, rulesWithStreet}), ());
    TEST(AlternativeMatch("tverskoi blvd 3", {rules, rulesWithStreet}), ());
    TEST(AlternativeMatch("tverskoi blvrd 3", {rules, rulesWithStreet}), ());
    TEST(AlternativeMatch("tverskoi boulevrd 3", {rules, rulesWithStreet}), ());
    TEST(AlternativeMatch("tverskoi bolevard 3", {rules, rulesWithStreet}), ());
  }
  {
    Rules rules = {ExactMatch(countryId, houseLeninsky)};
    Rules rulesWithStreet = {ExactMatch(countryId, houseLeninsky), ExactMatch(countryId, leninsky)};
    TEST(AlternativeMatch("leninsky 5", {rules, rulesWithStreet}), ());
    TEST(AlternativeMatch("leninsky avenue 5", {rules, rulesWithStreet}), ());
    TEST(AlternativeMatch("leninsky avenu 5", {rules, rulesWithStreet}), ());
    TEST(AlternativeMatch("leninsky avneue 5", {rules, rulesWithStreet}), ());
    TEST(AlternativeMatch("leninsky av 5", {rules, rulesWithStreet}), ());
  }
  {
    Rules rules = {ExactMatch(countryId, houseMira)};
    Rules rulesWithStreet = {ExactMatch(countryId, houseMira), ExactMatch(countryId, mira)};
    TEST(AlternativeMatch("мира 7", {rules, rulesWithStreet}), ());
    TEST(AlternativeMatch("проспект мира 7", {rules, rulesWithStreet}), ());
    TEST(AlternativeMatch("пропект мира 7", {rules, rulesWithStreet}), ());
    TEST(AlternativeMatch("прсопект мира 7", {rules, rulesWithStreet}), ());
    TEST(AlternativeMatch("пр-т мира 7", {rules, rulesWithStreet}), ());
  }
}

UNIT_CLASS_TEST(ProcessorTest, StreetSynonymPrefixMatch)
{
  string const countryName = "Wonderland";

  TestStreet yesenina(
      vector<m2::PointD>{m2::PointD(0.5, -0.5), m2::PointD(0, 0), m2::PointD(-0.5, 0.5)},
      "Yesenina street", "en");

  TestPOI cafe(m2::PointD(0, 0), "", "en");
  cafe.SetTypes({{"amenity", "cafe"}});

  auto countryId = BuildCountry(countryName, [&](TestMwmBuilder & builder) {
    builder.Add(yesenina);
    builder.Add(cafe);
  });

  SetViewport(m2::RectD(-1, -1, 1, 1));
  {
    Rules rules = {ExactMatch(countryId, cafe)};
    TEST(ResultsMatch("Yesenina cafe ", rules), ());
    TEST(ResultsMatch("Cafe Yesenina ", rules), ());
    TEST(ResultsMatch("Cafe Yesenina", rules), ());
  }
  {
    Rules rules = {ExactMatch(countryId, cafe), ExactMatch(countryId, yesenina)};
    // Prefix match with misprints to street synonym gives street as additional result
    // but we still can find the cafe.
    TEST(ResultsMatch("Yesenina cafe", rules), ());
  }
}

UNIT_CLASS_TEST(ProcessorTest, SynonymMisprintsTest)
{
  string const countryName = "Wonderland";

  TestStreet bolshaya({m2::PointD(0.5, -0.5), m2::PointD(-0.5, 0.5)}, "большая дмитровка", "ru");
  TestCafe bolnaya(m2::PointD(0.0, 0.0), "больная дмитровка", "ru");
  TestStreet sw({m2::PointD(0.5, -0.5), m2::PointD(0.5, 0.5)}, "southwest street", "en");
  TestStreet se({m2::PointD(-0.5, -0.5), m2::PointD(-0.5, 0.5)}, "southeast street", "en");

  auto wonderlandId = BuildCountry(countryName, [&](TestMwmBuilder & builder) {
    builder.Add(bolshaya);
    builder.Add(bolnaya);
    builder.Add(sw);
    builder.Add(se);
  });

  {
    Rules rules = {ExactMatch(wonderlandId, bolshaya), ExactMatch(wonderlandId, bolnaya)};
    TEST(ResultsMatch("большая дмитровка", rules), ());
    TEST(ResultsMatch("больная дмитровка", rules), ());
  }
  {
    Rules rules = {ExactMatch(wonderlandId, bolshaya)};
    // "б" is a synonym for "большая" but not for "больная".
    TEST(ResultsMatch("б дмитровка", rules), ());
  }
  {
    // "southeast" and "southwest" len is 9 and Levenstein distance is 2.
    Rules rules = {ExactMatch(wonderlandId, sw), ExactMatch(wonderlandId, se)};
    TEST(ResultsMatch("southeast street", rules), ());
    TEST(ResultsMatch("southwest street", rules), ());
  }
  {
    Rules rules = {ExactMatch(wonderlandId, sw)};
    // "sw" is a synonym for "southwest" but not for "southeast".
    TEST(ResultsMatch("sw street", rules), ());
  }
  {
    Rules rules = {ExactMatch(wonderlandId, se)};
    // "se" is a synonym for "southeast" but not for "southwest".
    TEST(ResultsMatch("se street", rules), ());
  }
}

UNIT_CLASS_TEST(ProcessorTest, VillagePostcodes)
{
  string const countryName = "France";

  TestVillage marckolsheim(m2::PointD(0, 0), "Marckolsheim", "en", 10 /* rank */);
  marckolsheim.SetPostcode("67390");

  TestStreet street(
      vector<m2::PointD>{m2::PointD(-0.5, 0.0), m2::PointD(0, 0), m2::PointD(0.5, 0.0)},
      "Rue des Serpents", "en");

  TestBuilding building4(m2::PointD(0.0, 0.00001), "", "4", street.GetName("en"), "en");
  TestPOI poi(m2::PointD(0.0, -0.00001), "Carrefour", "en");

  auto countryId = BuildCountry(countryName, [&](TestMwmBuilder & builder) {
    builder.Add(marckolsheim);
    builder.Add(street);
    builder.Add(building4);
    builder.Add(poi);
  });

  SetViewport(m2::RectD(-1, -1, 1, 1));
  {
    Rules rules{ExactMatch(countryId, building4), ExactMatch(countryId, street)};
    // Test that we do not require the building to have a postcode if the village has.
    TEST(ResultsMatch("Rue des Serpents 4 Marckolsheim 67390 ", rules), ());
  }
  {
    Rules rules{ExactMatch(countryId, poi), ExactMatch(countryId, street)};
    // Test that we do not require the poi to have a postcode if the village has.
    TEST(ResultsMatch("Carrefour Rue des Serpents Marckolsheim 67390 ", rules), ());
  }
  {
    Rules rules{ExactMatch(countryId, street)};
    // Test that we do not require the street to have a postcode if the village has.
    TEST(ResultsMatch("Rue des Serpents Marckolsheim 67390 ", rules), ());
  }
  {
    Rules rules{ExactMatch(countryId, marckolsheim)};
    TEST(ResultsMatch("67390 ", rules), ());
    TEST(ResultsMatch("67390", rules), ());
    TEST(ResultsMatch("Marckolsheim 67390 ", rules), ());
    TEST(ResultsMatch("Marckolsheim 67390", rules), ());
  }
}

UNIT_CLASS_TEST(ProcessorTest, StreetPostcodes)
{
  string const countryName = "France";

  TestStreet street(
      vector<m2::PointD>{m2::PointD(-0.5, 0.0), m2::PointD(0, 0), m2::PointD(0.5, 0.0)},
      "Rue des Serpents", "en");
  street.SetPostcode("67390");

  TestBuilding building4(m2::PointD(0.0, 0.00001), "", "4", street.GetName("en"), "en");

  auto countryId = BuildCountry(countryName, [&](TestMwmBuilder & builder) {
    builder.Add(street);
    builder.Add(building4);
  });

  SetViewport(m2::RectD(-1, -1, 1, 1));
  {
    Rules rules{ExactMatch(countryId, building4), ExactMatch(countryId, street)};
    // Test that we do not require the building to have a postcode if the street has.
    TEST(ResultsMatch("4 Rue des Serpents 67390 ", "ru", rules), ());
  }
  {
    Rules rules{ExactMatch(countryId, street)};
    TEST(ResultsMatch("67390 ", rules), ());
    TEST(ResultsMatch("67390", rules), ());
  }
}

UNIT_CLASS_TEST(ProcessorTest, CityPostcodes)
{
  string const countryName = "Russia";

  TestCity moscow(m2::PointD(0, 0), "Moscow", "en", 100 /* rank */);
  moscow.SetPostcode("123456");

  TestStreet street(
      vector<m2::PointD>{m2::PointD(-0.5, 0.0), m2::PointD(0, 0), m2::PointD(0.5, 0.0)},
      "Tverskaya", "en");

  TestBuilding building(m2::PointD(0.0, 0.00001), "", "4", street.GetName("en"), "en");

  auto const worldId = BuildWorld([&](TestMwmBuilder & builder) {
    builder.Add(moscow);
  });

  auto countryId = BuildCountry(countryName, [&](TestMwmBuilder & builder) {
    builder.Add(moscow);
    builder.Add(street);
    builder.Add(building);
  });

  SetViewport(m2::RectD(-1, -1, 1, 1));
  {
    Rules rules{ExactMatch(countryId, building), ExactMatch(countryId, street)};
    // Test that we do not require the building to have a postcode if the city has.
    TEST(ResultsMatch("Tverskaya 4 Moscow 123456 ", rules), ());
  }
}

UNIT_CLASS_TEST(ProcessorTest, StreetNumber)
{
  string const countryName = "Wonderland";

  TestStreet street(vector<m2::PointD>{m2::PointD(-1.0, -1.0), m2::PointD(1.0, 1.0)},
                    "Симферопольское шоссе", "ru");
  street.SetRoadNumber("M2");

  auto countryId = BuildCountry(countryName, [&](TestMwmBuilder & builder) {
    builder.Add(street);
  });

  SetViewport(m2::RectD(m2::PointD(0.0, 0.0), m2::PointD(1.0, 2.0)));
  {
    Rules rules = {ExactMatch(countryId, street)};
    TEST(ResultsMatch("M2 ", rules), ());
  }
}

UNIT_CLASS_TEST(ProcessorTest, StreetNumberEnriched)
{
  string const countryName = "Wonderland";

  TestStreet street(vector<m2::PointD>{m2::PointD(-1.0, -1.0), m2::PointD(1.0, 1.0)}, "Нева", "ru");
  street.SetRoadNumber("M-11;ru:national/M-11");

  auto countryId =
      BuildCountry(countryName, [&](TestMwmBuilder & builder) { builder.Add(street); });

  SetViewport(m2::RectD(m2::PointD(0.0, 0.0), m2::PointD(1.0, 2.0)));
  {
    Rules rules = {ExactMatch(countryId, street)};
    TEST(ResultsMatch("M-11 ", rules), ());
  }

  SetViewport(m2::RectD(m2::PointD(0.0, 0.0), m2::PointD(1.0, 2.0)));
  {
    Rules rules = {};
    TEST(ResultsMatch("ru ", rules), ());
    TEST(ResultsMatch("national ", rules), ());
  }
}

UNIT_CLASS_TEST(ProcessorTest, Postbox)
{
  string const countryName = "Wonderland";

  TestPOI postbox(m2::PointD(0.0, 0.0), "127001", "default");
  postbox.SetTypes({{"amenity", "post_box"}});
  
  auto countryId = BuildCountry(countryName, [&](TestMwmBuilder & builder) {
    builder.Add(postbox);
  });

  SetViewport(m2::RectD(m2::PointD(0.0, 0.0), m2::PointD(1.0, 1.0)));
  {
    Rules rules = {ExactMatch(countryId, postbox)};
    TEST(ResultsMatch("127001", rules), ());
  }
  {
    // Misprints are not allowed for postcodes.
    Rules rules = {};
    TEST(ResultsMatch("127002", rules), ());
  }
}

UNIT_CLASS_TEST(ProcessorTest, OrderCountries)
{
  string const cafeLandName = "CafeLand";
  string const UkCountryName = "UK";
  TestCity london(m2::PointD(1.0, 1.0), "London", "en", 100 /* rank */);
  TestStreet piccadilly(vector<m2::PointD>{m2::PointD(0.5, 0.5), m2::PointD(1.5, 1.5)},
                        "Piccadilly Circus", "en");
  TestVillage cambridge(m2::PointD(3.0, 3.0), "Cambridge", "en", 5 /* rank */);
  TestStreet wheeling(vector<m2::PointD>{m2::PointD(2.5, 2.5), m2::PointD(3.5, 3.5)},
                      "Wheeling Avenue", "en");

  TestPOI dummyPoi(m2::PointD(0.0, 4.0), "dummy", "en");
  TestCafe londonCafe(m2::PointD(-10.01, 14.0), "London Piccadilly cafe", "en");
  TestCafe cambridgeCafe(m2::PointD(-10.02, 14.01), "Cambridge Wheeling cafe", "en");

  auto worldId = BuildWorld([&](TestMwmBuilder & builder) { builder.Add(london); });
  auto UkId = BuildCountry(UkCountryName, [&](TestMwmBuilder & builder) {
    builder.Add(london);
    builder.Add(piccadilly);
    builder.Add(cambridge);
    builder.Add(wheeling);
  });
  auto const UkCountryRect = m2::RectD(m2::PointD(0.5, 0.5), m2::PointD(3.5, 3.5));
  RegisterCountry(UkCountryName, UkCountryRect);

  auto cafeLandId = BuildCountry(cafeLandName, [&](TestMwmBuilder & builder) {
    builder.Add(dummyPoi);
    builder.Add(londonCafe);
    builder.Add(cambridgeCafe);
  });

  auto const cafeLandRect = m2::RectD(m2::PointD(-10.5, 4.0), m2::PointD(0.0, 14.5));
  RegisterCountry(cafeLandName, cafeLandRect);

  auto const viewportRect = m2::RectD(m2::PointD(-1.0, 5.0), m2::PointD(0.0, 4.0));
  SetViewport(viewportRect);
  CHECK(!viewportRect.IsIntersect(UkCountryRect), ());
  CHECK(viewportRect.IsIntersect(cafeLandRect), ());
  {
    auto request = MakeRequest("London Piccadilly");
    auto const & results = request->Results();

    TEST_EQUAL(results.size(), 2, ("Unexpected number of results."));
    // (UkId, piccadilly) should be ranked first, it means it was in the first batch.
    TEST(ResultsMatch({results[0]}, {ExactMatch(UkId, piccadilly)}), ());
    TEST(ResultsMatch({results[1]}, {ExactMatch(cafeLandId, londonCafe)}), ());
  }
  {
    auto request = MakeRequest("Cambridge Wheeling");
    auto const & results = request->Results();

    TEST_EQUAL(results.size(), 2, ("Unexpected number of results."));
    TEST(ResultsMatch({results[0]}, {ExactMatch(cafeLandId, cambridgeCafe)}), ());
    TEST(ResultsMatch({results[1]}, {ExactMatch(UkId, wheeling)}), ());
    // (UkId, wheeling) has higher rank but it's second in results because it was not in the first
    // batch.
    TEST_GREATER(results[1].GetRankingInfo().GetLinearModelRank(),
                 results[0].GetRankingInfo().GetLinearModelRank(), ());
  }
}

UNIT_CLASS_TEST(ProcessorTest, Suburbs)
{
  string const countryName = "Wonderland";

  TestPOI suburb(m2::PointD(0, 0), "Bloomsbury", "en");
  suburb.SetTypes({{"place", "suburb"}});

  TestStreet street(
      vector<m2::PointD>{m2::PointD(-0.5, -0.5), m2::PointD(0, 0), m2::PointD(0.5, 0.5)},
      "Malet place", "en");

  TestPOI house(m2::PointD(0.5, 0.5), "", "en");
  house.SetHouseNumber("3");
  house.SetStreetName(street.GetName("en"));

  TestCafe cafe(m2::PointD(0.01, 0.01), "", "en");

  auto countryId = BuildCountry(countryName, [&](TestMwmBuilder & builder) {
    builder.Add(suburb);
    builder.Add(street);
    builder.Add(house);
    builder.Add(cafe);
  });

  auto const testFullMatch = [&](auto const & query, auto const & expected) {
    auto request = MakeRequest(query);
    auto const & results = request->Results();
    TEST_GREATER(results.size(), 0, (results));
    TEST(ResultMatches(results[0], expected), (query, results));

    auto const & info = results[0].GetRankingInfo();
    TEST(info.m_exactMatch, ());
    TEST(info.m_allTokensUsed, ());
    TEST_ALMOST_EQUAL_ABS(info.m_matchedFraction, 1.0, 1e-12, ());
  };

  SetViewport(m2::RectD(-1.0, -1.0, 1.0, 1.0));
  {
    testFullMatch("Malet place 3, Bloomsbury ", ExactMatch(countryId, house));
    testFullMatch("Bloomsbury cafe ", ExactMatch(countryId, cafe));
    testFullMatch("Bloomsbury ", ExactMatch(countryId, suburb));
  }
}

UNIT_CLASS_TEST(ProcessorTest, ViewportFilter)
{
  TestStreet street23({m2::PointD(0.5, -1.0), m2::PointD(0.5, 1.0)}, "23rd February street", "en");
  TestStreet street8({m2::PointD(0.0, -1.0), m2::PointD(0.0, 1.0)}, "8th March street", "en");

  auto const countryId = BuildCountry("Wounderland", [&](TestMwmBuilder & builder) {
    builder.Add(street23);
    builder.Add(street8);
  });

  {
    SearchParams params;
    params.m_query = "8th March street 23";
    params.m_inputLocale = "en";
    params.m_viewport = m2::RectD(m2::PointD(-1.0, -1.0), m2::PointD(1.0, 1.0));
    params.m_mode = Mode::Viewport;

    // |street23| should not appear in viewport search because it has 2 unmatched tokens.
    // |street8| has 1 unmatched token.
    Rules const rulesViewport = {ExactMatch(countryId, street8)};

    TestSearchRequest request(m_engine, params);
    request.Run();
    TEST(ResultsMatch(request.Results(), rulesViewport), ());
  }

  {
    SearchParams params;
    params.m_query = "8th March street 23";
    params.m_inputLocale = "en";
    params.m_viewport = m2::RectD(m2::PointD(-1.0, -1.0), m2::PointD(1.0, 1.0));
    params.m_mode = Mode::Everywhere;

    // |street23| should be in everywhere search results because everywhere search mode does not
    // have matched tokens number restriction.
    Rules const rulesViewport = {ExactMatch(countryId, street23), ExactMatch(countryId, street8)};

    TestSearchRequest request(m_engine, params);
    request.Run();
    TEST(ResultsMatch(request.Results(), rulesViewport), ());
  }
}

UNIT_CLASS_TEST(ProcessorTest, FilterStreetPredictions)
{
  TestCity smallCity(m2::PointD(3.0, 0.0), "SmallCity", "en", 1 /* rank */);

  TestStreet lenina0({m2::PointD(0.0, -1.0), m2::PointD(0.0, 1.0)}, "Lenina", "en");
  TestStreet lenina1({m2::PointD(1.0, -1.0), m2::PointD(1.0, 1.0)}, "Lenina", "en");
  TestStreet lenina2({m2::PointD(2.0, -1.0), m2::PointD(2.0, 1.0)}, "Lenina", "en");
  TestStreet lenina3({m2::PointD(3.0, -1.0), m2::PointD(3.0, 1.0)}, "Lenina", "en");

  auto const countryId = BuildCountry("Wounderland", [&](TestMwmBuilder & builder) {
    builder.Add(lenina0);
    builder.Add(lenina1);
    builder.Add(lenina2);
    builder.Add(lenina3);
  });

  BuildWorld([&](TestMwmBuilder & builder) { builder.Add(smallCity); });

  SearchParams defaultParams;
  defaultParams.m_query = "Lenina";
  defaultParams.m_inputLocale = "en";
  defaultParams.m_viewport = m2::RectD(m2::PointD(-1.0, -1.0), m2::PointD(1.0, 1.0));
  defaultParams.m_mode = Mode::Everywhere;
  defaultParams.m_streetSearchRadiusM = TestSearchRequest::kDefaultTestStreetSearchRadiusM;

  {
    Rules const rules = {ExactMatch(countryId, lenina0), ExactMatch(countryId, lenina1),
                         ExactMatch(countryId, lenina2), ExactMatch(countryId, lenina3)};

    TestSearchRequest request(m_engine, defaultParams);
    request.Run();
    TEST(ResultsMatch(request.Results(), rules), ());
  }

  {
    Rules const rules = {ExactMatch(countryId, lenina0), ExactMatch(countryId, lenina1),
                         ExactMatch(countryId, lenina2)};

    auto params = defaultParams;
    params.m_streetSearchRadiusM =
        mercator::DistanceOnEarth(params.m_viewport.Center(), m2::PointD(3.0, 0.0)) - 1.0;

    TestSearchRequest request(m_engine, params);
    request.Run();
    TEST(ResultsMatch(request.Results(), rules), ());
  }

  {
    Rules const rules = {ExactMatch(countryId, lenina0), ExactMatch(countryId, lenina1)};

    auto params = defaultParams;
    params.m_streetSearchRadiusM =
        mercator::DistanceOnEarth(params.m_viewport.Center(), m2::PointD(2.0, 0.0)) - 1.0;

    TestSearchRequest request(m_engine, params);
    request.Run();
    TEST(ResultsMatch(request.Results(), rules), ());
  }

  {
    Rules const rules = {ExactMatch(countryId, lenina0)};

    auto params = defaultParams;
    params.m_streetSearchRadiusM =
        mercator::DistanceOnEarth(params.m_viewport.Center(), m2::PointD(1.0, 0.0)) - 1.0;

    TestSearchRequest request(m_engine, params);
    request.Run();
    TEST(ResultsMatch(request.Results(), rules), ());
  }

  {
    Rules const rules = {ExactMatch(countryId, lenina0), ExactMatch(countryId, lenina3)};

    auto params = defaultParams;
    params.m_streetSearchRadiusM =
        mercator::DistanceOnEarth(params.m_viewport.Center(), m2::PointD(1.0, 0.0)) - 1.0;
    params.m_position = m2::PointD(3.0, 0.0);

    TestSearchRequest request(m_engine, params);
    request.Run();
    TEST(ResultsMatch(request.Results(), rules), ());
  }

  {
    Rules const rules = {ExactMatch(countryId, lenina0), ExactMatch(countryId, lenina3)};

    auto params = defaultParams;
    params.m_streetSearchRadiusM =
        mercator::DistanceOnEarth(params.m_viewport.Center(), m2::PointD(1.0, 0.0)) - 1.0;
    params.m_query = "SmallCity Lenina";

    TestSearchRequest request(m_engine, params);
    request.Run();
    TEST(ResultsMatch(request.Results(), rules), ());
  }
}

UNIT_CLASS_TEST(ProcessorTest, FilterVillages)
{
  TestState moscowRegion(m2::PointD(10.0, 10.0), "Moscow Region", "en");
  // |moscowRegion| feature should belong to MoscowRegion with some margin due to mwmPointAccuracy.
  TestPOI dummy(m2::PointD(9.99, 9.99), "", "en");
  TestVillage petrovskoeMoscow(m2::PointD(10.5, 10.5), "Petrovskoe", "en", 5 /* rank */);

  TestVillage petrovskoe0(m2::PointD(0.0, 0.0), "Petrovskoe", "en", 5 /* rank */);
  TestVillage petrovskoe1(m2::PointD(1.0, 1.0), "Petrovskoe", "en", 5 /* rank */);
  TestVillage petrovskoe2(m2::PointD(2.0, 2.0), "Petrovskoe", "en", 5 /* rank */);

  auto const moscowId = BuildCountry("MoscowRegion", [&](TestMwmBuilder & builder) {
    builder.Add(petrovskoeMoscow);
    builder.Add(dummy);
  });

  auto const otherId = BuildCountry("OtherRegion", [&](TestMwmBuilder & builder) {
    builder.Add(petrovskoe0);
    builder.Add(petrovskoe1);
    builder.Add(petrovskoe2);
  });

  BuildWorld([&](TestMwmBuilder & builder) { builder.Add(moscowRegion); });

  SearchParams defaultParams;
  defaultParams.m_query = "Petrovskoe";
  defaultParams.m_inputLocale = "en";
  defaultParams.m_viewport = m2::RectD(m2::PointD(-1.0, -1.0), m2::PointD(1.0, 1.0));
  defaultParams.m_mode = Mode::Everywhere;
  defaultParams.m_villageSearchRadiusM = TestSearchRequest::kDefaultTestVillageSearchRadiusM;

  {
    Rules const rules = {ExactMatch(otherId, petrovskoe0), ExactMatch(otherId, petrovskoe1),
                         ExactMatch(otherId, petrovskoe2), ExactMatch(moscowId, petrovskoeMoscow)};

    TestSearchRequest request(m_engine, defaultParams);
    request.Run();
    TEST(ResultsMatch(request.Results(), rules), ());
  }

  {
    Rules const rules = {ExactMatch(otherId, petrovskoe0), ExactMatch(otherId, petrovskoe1),
                         ExactMatch(otherId, petrovskoe2)};

    auto params = defaultParams;
    params.m_villageSearchRadiusM =
        mercator::DistanceOnEarth(params.m_viewport.Center(), petrovskoeMoscow.GetCenter()) - 1.0;

    TestSearchRequest request(m_engine, params);
    request.Run();
    TEST(ResultsMatch(request.Results(), rules), ());
  }

  {
    Rules const rules = {ExactMatch(otherId, petrovskoe0), ExactMatch(otherId, petrovskoe1)};

    auto params = defaultParams;
    params.m_villageSearchRadiusM =
        mercator::DistanceOnEarth(params.m_viewport.Center(), petrovskoe2.GetCenter()) - 1.0;

    TestSearchRequest request(m_engine, params);
    request.Run();
    TEST(ResultsMatch(request.Results(), rules), ());
  }

  {
    Rules const rules = {ExactMatch(otherId, petrovskoe0)};

    auto params = defaultParams;
    params.m_villageSearchRadiusM =
        mercator::DistanceOnEarth(params.m_viewport.Center(), petrovskoe1.GetCenter()) - 1.0;

    TestSearchRequest request(m_engine, params);
    request.Run();
    TEST(ResultsMatch(request.Results(), rules), ());
  }

  {
    Rules const rules = {ExactMatch(otherId, petrovskoe0), ExactMatch(otherId, petrovskoe2)};

    auto params = defaultParams;
    params.m_position = m2::PointD(2.0, 2.0);
    params.m_villageSearchRadiusM =
        min(mercator::DistanceOnEarth(params.m_viewport.Center(), petrovskoe1.GetCenter()),
            mercator::DistanceOnEarth(*params.m_position, petrovskoe1.GetCenter()));
    params.m_villageSearchRadiusM -= 1.0;

    TestSearchRequest request(m_engine, params);
    request.Run();
    TEST(ResultsMatch(request.Results(), rules), ());
  }

  {
    Rules const rules = {ExactMatch(otherId, petrovskoe0), ExactMatch(moscowId, petrovskoeMoscow)};

    auto params = defaultParams;
    params.m_villageSearchRadiusM =
        mercator::DistanceOnEarth(params.m_viewport.Center(), petrovskoe1.GetCenter()) - 1.0;
    params.m_query = "Petrovskoe Moscow Region";

    TestSearchRequest request(m_engine, params);
    request.Run();
    TEST(ResultsMatch(request.Results(), rules), ());
  }
}

UNIT_CLASS_TEST(ProcessorTest, MatchedFraction)
{
  string const countryName = "Wonderland";
  string const streetName = "Октябрьский проспaект";

  TestStreet street1(vector<m2::PointD>{m2::PointD(-1.0, -1.0), m2::PointD(1.0, 1.0)},
                     "Первомайская", "ru");
  TestStreet street2(vector<m2::PointD>{m2::PointD(-1.0, 1.0), m2::PointD(1.0, -1.0)},
                     "8 марта", "ru");

  auto countryId = BuildCountry(countryName, [&](TestMwmBuilder & builder) {
    builder.Add(street1);
    builder.Add(street2);
  });

  SetViewport(m2::RectD(-1.0, -1.0, 1.0, 1.0));
  {
    // |8 марта| should not match because matched fraction is too low (1/13 <= 0.1).
    Rules rules = {ExactMatch(countryId, street1)};
    TEST(ResultsMatch("первомайская 8 ", rules), ());
  }
  {
    Rules rules = {ExactMatch(countryId, street2)};
    TEST(ResultsMatch("8 ", rules), ());
  }
}

UNIT_CLASS_TEST(ProcessorTest, AvoidMatchAroundPivotInMwmWithCity)
{
  string const minskCountryName = "Minsk";

  TestCity minsk(m2::PointD(-10.0, -10.0), "Minsk", "en", 10 /* rank */);
  // World.mwm should intersect viewport.
  TestPOI dummy(m2::PointD(10.0, 10.0), "", "en");

  auto worldId = BuildWorld([&](TestMwmBuilder & builder) {
    builder.Add(minsk);
    builder.Add(dummy);
  });

  TestCafe minskCafe(m2::PointD(-9.99, -9.99), "Minsk cafe", "en");
  auto minskId = BuildCountry(minskCountryName, [&](TestMwmBuilder & builder) {
    builder.Add(minsk);
    builder.Add(minskCafe);
  });

  SetViewport(m2::RectD(m2::PointD(-1.0, -1.0), m2::PointD(1.0, 1.0)));
  {
    // Minsk cafe should not appear here because we have (worldId, minsk) result with all tokens
    // used.
    Rules rules = {ExactMatch(worldId, minsk)};
    TEST(ResultsMatch("Minsk ", rules), ());
    TEST(ResultsMatch("Minsk", rules), ());
  }
  {
    // We search for pois until we find result with all tokens used. We do not emit relaxed result
    // from world.
    Rules rules = {ExactMatch(minskId, minskCafe)};
    TEST(ResultsMatch("Minsk cafe", rules), ());
  }
}

UNIT_CLASS_TEST(ProcessorTest, LocalityScorer)
{
  TestCity sp0(m2::PointD(0.0, 0.0), "San Pedro", "en", 50 /* rank */);
  TestCity sp1(m2::PointD(1.0, 1.0), "San Pedro", "en", 50 /* rank */);
  TestCity sp2(m2::PointD(2.0, 2.0), "San Pedro", "en", 50 /* rank */);
  TestCity sp3(m2::PointD(3.0, 3.0), "San Pedro", "en", 50 /* rank */);
  TestCity sp4(m2::PointD(4.0, 4.0), "San Pedro", "en", 50 /* rank */);
  TestCity spAtacama(m2::PointD(5.5, 5.5), "San Pedro de Atacama", "en", 100 /* rank */);
  TestCity spAlcantara(m2::PointD(6.5, 6.5), "San Pedro de Alcantara", "en", 40 /* rank */);

  auto worldId = BuildWorld([&](TestMwmBuilder & builder) {
    builder.Add(sp0);
    builder.Add(sp1);
    builder.Add(sp2);
    builder.Add(sp3);
    builder.Add(sp4);
    builder.Add(spAtacama);
    builder.Add(spAlcantara);
  });

  SetViewport(m2::RectD(m2::PointD(0.0, 0.0), m2::PointD(2.0, 2.0)));
  {
    Rules rules = {ExactMatch(worldId, spAtacama)};
    TEST(ResultsMatch("San Pedro de Atacama ", rules), ());
  }
  {
    // spAlcantara has the same cosine similarity as sp0..sp4 and lower rank but
    // more matched tokens.
    Rules rules = {ExactMatch(worldId, spAlcantara)};
    TEST(ResultsMatch("San Pedro de Alcantara ", rules), ());
  }
}

UNIT_CLASS_TEST(ProcessorTest, StreetWithNumber)
{
  string const countryName = "Wonderland";

  TestStreet street1(
      vector<m2::PointD>{m2::PointD(-0.5, 1.0), m2::PointD(0.0, 1.0), m2::PointD(0.5, 1.0)},
      "1-я Тверская-Ямская", "ru");

  TestStreet street8(
      vector<m2::PointD>{m2::PointD(-0.5, 1.0), m2::PointD(0.0, 0.0), m2::PointD(0.5, 0.0)},
      "8 Марта", "ru");

  TestStreet street11(
      vector<m2::PointD>{m2::PointD(-0.5, -1.0), m2::PointD(0.0, -1.0), m2::PointD(0.5, -1.0)},
      "11-я Магистральная", "ru");

  TestBuilding building1(m2::PointD(0.0, 1.00001), "", "1", street1.GetName("ru"), "en");
  TestBuilding building8(m2::PointD(0.0, 0.00001), "", "8", street8.GetName("ru"), "en");
  TestBuilding building11(m2::PointD(0.0, -1.00001), "", "11", street11.GetName("ru"), "en");

  auto countryId = BuildCountry(countryName, [&](TestMwmBuilder & builder) {
    builder.Add(street1);
    builder.Add(street8);
    builder.Add(street11);
    builder.Add(building1);
    builder.Add(building8);
    builder.Add(building11);
  });

  SetViewport(m2::RectD(-1.0, -1.0, 1.0, 1.0));
  {
    Rules rules{ExactMatch(countryId, building1), ExactMatch(countryId, street1)};
    TEST(ResultsMatch("1-я Тверская-Ямская 1 ", rules), ());
  }
  {
    Rules rules{ExactMatch(countryId, building8), ExactMatch(countryId, street8)};
    TEST(ResultsMatch("8 Марта 8  ", rules), ());
  }
  {
    Rules rules{ExactMatch(countryId, building11), ExactMatch(countryId, street11)};
    TEST(ResultsMatch("11-я Магистральная 11 ", rules), ());
  }
}
}  // namespace
}  // namespace search
