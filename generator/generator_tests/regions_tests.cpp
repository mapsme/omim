#include "testing/testing.hpp"

#include "generator/feature_builder.hpp"
#include "generator/generator_tests/common.hpp"
#include "generator/osm2type.hpp"
#include "generator/osm_element.hpp"
#include "generator/regions/collector_region_info.hpp"
#include "generator/regions/place_point.hpp"
#include "generator/regions/regions.hpp"
#include "generator/regions/regions_builder.hpp"

#include "indexer/classificator.hpp"
#include "indexer/classificator_loader.hpp"

#include "platform/platform.hpp"

#include "coding/transliteration.hpp"

#include "base/file_name_utils.hpp"
#include "base/macros.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <sstream>
#include <utility>
#include <vector>

using namespace generator_tests;
using namespace generator;
using namespace generator::regions;
using namespace feature;
using namespace base;

namespace
{
using Tags = std::vector<std::pair<std::string, std::string>>;
auto NodeEntry = OsmElement::EntityType::Node;

FeatureBuilder const kEmptyFeature;

OsmElement CreateOsmRelation(uint64_t id, std::string const & adminLevel,
                             std::string const & place = "")
{
  OsmElement el;
  el.m_id = id;
  el.m_type = OsmElement::EntityType::Relation;
  el.AddTag("place", place);
  el.AddTag("admin_level", adminLevel);

  return el;
}

struct TagValue
{
  std::string m_key;
  std::string m_value;

  TagValue(std::string key, std::string value = {})
    : m_key{std::move(key)}, m_value{std::move(value)}
  { }

  TagValue operator=(std::string const & value) const
  {
    CHECK(m_value.empty(), ());
    return {m_key, value};
  }
};

struct OsmElementData
{
  uint64_t m_id;
  std::vector<TagValue> m_tags;
  std::vector<m2::PointD> m_polygon;
  std::vector<OsmElement::Member> m_members;
};

OsmElement MakeOsmElement(OsmElementData const & elementData)
{
  OsmElement el;
  el.m_id = elementData.m_id;
  el.m_type = elementData.m_polygon.size() > 1 ? OsmElement::EntityType::Relation
                                               : OsmElement::EntityType::Node;
  for (auto const tag : elementData.m_tags)
    el.AddTag(tag.m_key, tag.m_value);
  el.m_members = elementData.m_members;
  return el;
}

void CollectRegionInfo(std::string const & filename, std::vector<OsmElementData> const & testData)
{
  CollectorRegionInfo collector(filename);

  for (auto const & elementData : testData)
  {
    auto el = MakeOsmElement(elementData);
    collector.Collect(el);
  }

  collector.Save();
}

void BuildTestData(std::vector<OsmElementData> const & testData,
                   RegionsBuilder::Regions & regions, PlacePointsMap & placePointsMap,
                   RegionInfo & collector)
{
  for (auto const & elementData : testData)
  {
    auto el = MakeOsmElement(elementData);
    FeatureBuilder fb;

    CHECK(elementData.m_polygon.size() == 1 || elementData.m_polygon.size() == 2, ());
    if (elementData.m_polygon.size() == 1)
    {
      fb.SetCenter(elementData.m_polygon[0]);
    }
    else if (elementData.m_polygon.size() == 2)
    {
      auto const & p1 = elementData.m_polygon[0];
      auto const & p2 = elementData.m_polygon[1];
      vector<m2::PointD> poly = {{p1.x, p1.y}, {p1.x, p2.y}, {p2.x, p2.y}, {p2.x, p1.y}, {p1.x, p1.x}};
      fb.AddPolygon(poly);
      fb.SetHoles({});
      fb.SetArea();
    }

    auto osmId = el.m_type == OsmElement::EntityType::Relation ? MakeOsmRelation(el.m_id)
                                                               : MakeOsmNode(el.m_id);
    fb.SetOsmId(osmId);

    FeatureParams params;
    ftype::GetNameAndType(&el, params, [] (uint32_t type) {
      return classif().IsTypeValid(type);
    });
    fb.SetParams(params);

    auto const id = fb.GetMostGenericOsmId();
    if (elementData.m_polygon.size() == 1)
      placePointsMap.emplace(id, PlacePoint{fb, collector.Get(id)});
    else
      regions.emplace_back(fb, collector.Get(id));
  }
}

std::string ToLabelingString(vector<Node::Ptr> const & path)
{
  CHECK(!path.empty(), ());
  std::stringstream stream;
  auto i = path.begin();
  auto const & countryName = (*i)->GetData().GetName();
  CHECK(!countryName.empty(), ());
  stream << (*i)->GetData().GetName();
  ++i;
  for (; i != path.end(); ++i)
    stream << ", " << GetLabel((*i)->GetData().GetLevel()) << ": " << (*i)->GetData().GetName();

  return stream.str();
}

std::vector<std::string> GenerateTestRegions(std::vector<OsmElementData> const & testData)
{
  classificator::Load();

  auto const filename = GetFileName();
  CollectRegionInfo(filename, testData);

  RegionsBuilder::Regions regions;
  PlacePointsMap placePointsMap;
  RegionInfo collector(filename);
  BuildTestData(testData, regions, placePointsMap, collector);

  RegionsBuilder builder(std::move(regions), {});
  std::vector<std::string> kvRegions;
  builder.ForEachCountry([&](std::string const & name, Node::PtrList const & outers) {
    for (auto const & tree : outers)
    {
      ForEachLevelPath(tree, [&] (std::vector<Node::Ptr> const & path) {
        kvRegions.push_back(ToLabelingString(path));
      });
    }
  });

  return kvRegions;
}

bool HasName(std::vector<string> const & coll, std::string const & name)
{
  auto const end = std::end(coll);
  return std::find(std::begin(coll), end, name) != end;
}

bool ContainsSubname(std::vector<string> const & coll, std::string const & name)
{
  auto const end = std::end(coll);
  auto hasSubname = [&name] (std::string const & item) { return item.find(name) != string::npos; };
  return std::find_if(std::begin(coll), end, hasSubname) != end;
}

std::string MakeCollectorData()
{
  auto const filename = GetFileName();
  CollectorRegionInfo collector(filename);
  collector.Collect(CreateOsmRelation(1 /* id */, "2" /* adminLevel */));
  collector.Collect(CreateOsmRelation(2 /* id */, "2" /* adminLevel */));
  collector.Collect(CreateOsmRelation(3 /* id */, "4" /* adminLevel */, "state"));
  collector.Collect(CreateOsmRelation(4 /* id */, "4" /* adminLevel */, "state"));
  collector.Collect(CreateOsmRelation(5 /* id */, "4" /* adminLevel */, "state"));
  collector.Collect(CreateOsmRelation(6 /* id */, "6" /* adminLevel */, "district"));
  collector.Collect(CreateOsmRelation(7 /* id */, "6" /* adminLevel */, "district"));
  collector.Collect(CreateOsmRelation(8 /* id */, "4" /* adminLevel */, "state"));
  collector.Save();
  return filename;
}

RegionsBuilder::Regions MakeTestDataSet1(RegionInfo & collector)
{
  RegionsBuilder::Regions regions;
  {
    FeatureBuilder fb;
    fb.AddName("default", "Country_1");
    fb.SetOsmId(MakeOsmRelation(1 /* id */));
    vector<m2::PointD> poly = {{2, 8},  {3, 12}, {8, 15}, {13, 12},
                               {15, 7}, {11, 2}, {4, 4},  {2, 8}};
    fb.AddPolygon(poly);
    fb.SetHoles({{{5, 8}, {7, 10}, {10, 10}, {11, 7}, {10, 4}, {7, 5}, {5, 8}}});
    fb.SetArea();
    regions.emplace_back(Region(fb, collector.Get(MakeOsmRelation(1 /* id */))));
  }

  {
    FeatureBuilder fb;
    fb.AddName("default", "Country_2");
    fb.SetOsmId(MakeOsmRelation(2 /* id */));
    vector<m2::PointD> poly = {{5, 8}, {7, 10}, {10, 10}, {11, 7}, {10, 4}, {7, 5}, {5, 8}};
    fb.AddPolygon(poly);
    fb.SetArea();
    regions.emplace_back(Region(fb, collector.Get(MakeOsmRelation(2 /* id */))));
  }

  {
    FeatureBuilder fb;
    fb.AddName("default", "Country_2");
    fb.SetOsmId(MakeOsmRelation(2 /* id */));
    vector<m2::PointD> poly = {{0, 0}, {0, 2}, {2, 2}, {2, 0}, {0, 0}};
    fb.AddPolygon(poly);
    fb.SetArea();
    regions.emplace_back(Region(fb, collector.Get(MakeOsmRelation(2 /* id */))));
  }

  {
    FeatureBuilder fb;
    fb.AddName("default", "Country_1_Region_3");
    fb.SetOsmId(MakeOsmRelation(3 /* id */));
    vector<m2::PointD> poly = {{4, 4}, {7, 5}, {10, 4}, {12, 9}, {15, 7}, {11, 2}, {4, 4}};
    fb.AddPolygon(poly);
    fb.SetArea();
    regions.emplace_back(Region(fb, collector.Get(MakeOsmRelation(3 /* id */))));
  }

  {
    FeatureBuilder fb;
    fb.AddName("default", "Country_1_Region_4");
    fb.SetOsmId(MakeOsmRelation(4 /* id */));
    vector<m2::PointD> poly = {{7, 10}, {9, 12}, {8, 15},  {13, 12}, {15, 7},
                               {12, 9}, {11, 7}, {10, 10}, {7, 10}};
    fb.AddPolygon(poly);
    fb.SetArea();
    regions.emplace_back(Region(fb, collector.Get(MakeOsmRelation(4 /* id */))));
  }

  {
    FeatureBuilder fb;
    fb.AddName("default", "Country_1_Region_5");
    fb.SetOsmId(MakeOsmRelation(5 /* id */));
    vector<m2::PointD> poly = {{4, 4},  {2, 8}, {3, 12}, {8, 15}, {9, 12},
                               {7, 10}, {5, 8}, {7, 5},  {4, 4}};
    fb.AddPolygon(poly);
    fb.SetArea();
    regions.emplace_back(Region(fb, collector.Get(MakeOsmRelation(5 /* id */))));
  }

  {
    FeatureBuilder fb;
    fb.AddName("default", "Country_1_Region_5_Subregion_6");
    fb.SetOsmId(MakeOsmRelation(6 /* id */));
    vector<m2::PointD> poly = {{4, 4}, {2, 8}, {3, 12}, {4, 10}, {5, 10}, {5, 8}, {7, 5}, {4, 4}};
    fb.AddPolygon(poly);
    fb.SetArea();
    regions.emplace_back(Region(fb, collector.Get(MakeOsmRelation(6 /* id */))));
  }

  {
    FeatureBuilder fb;
    fb.AddName("default", "Country_1_Region_5_Subregion_7");
    fb.SetOsmId(MakeOsmRelation(7 /* id */));
    vector<m2::PointD> poly = {{3, 12}, {8, 15}, {9, 12}, {7, 10},
                               {5, 8},  {5, 10}, {4, 10}, {3, 12}};
    fb.AddPolygon(poly);
    fb.SetArea();
    regions.emplace_back(Region(fb, collector.Get(MakeOsmRelation(7 /* id */))));
  }

  {
    FeatureBuilder fb;
    fb.AddName("default", "Country_2_Region_8");
    fb.SetOsmId(MakeOsmRelation(8 /* id */));
    vector<m2::PointD> poly = {{0, 0}, {0, 1}, {1, 1}, {1, 0}, {0, 0}};
    fb.AddPolygon(poly);
    fb.SetArea();
    regions.emplace_back(Region(fb, collector.Get(MakeOsmRelation(8 /* id */))));
  }

  return regions;
}

class StringJoinPolicy
{
public:
  std::string ToString(NodePath const & nodePath) const
  {
    std::stringstream stream;
    for (auto const & n : nodePath)
      stream << n->GetData().GetName();

    return stream.str();
  }
};

bool NameExists(std::vector<std::string> const & coll, std::string const & name)
{
  auto const end = std::end(coll);
  return std::find(std::begin(coll), end, name) != end;
};
}  // namespace

UNIT_TEST(RegionsBuilderTest_GetCountryNames)
{
  auto const filename = MakeCollectorData();
  RegionInfo collector(filename);
  RegionsBuilder builder(MakeTestDataSet1(collector), {} /* placePointsMap */);
  auto const & countryNames = builder.GetCountryNames();
  TEST_EQUAL(countryNames.size(), 2, ());
  TEST(std::count(std::begin(countryNames), std::end(countryNames), "Country_1"), ());
  TEST(std::count(std::begin(countryNames), std::end(countryNames), "Country_2"), ());
}

UNIT_TEST(RegionsBuilderTest_GetCountries)
{
  auto const filename = MakeCollectorData();
  RegionInfo collector(filename);
  RegionsBuilder builder(MakeTestDataSet1(collector), {} /* placePointsMap */);
  auto const & countries = builder.GetCountriesOuters();
  TEST_EQUAL(countries.size(), 3, ());
  TEST_EQUAL(std::count_if(std::begin(countries), std::end(countries),
                           [](const Region & r) { return r.GetName() == "Country_1"; }),
             1, ());
  TEST_EQUAL(std::count_if(std::begin(countries), std::end(countries),
                           [](const Region & r) { return r.GetName() == "Country_2"; }),
             2, ());
}

UNIT_TEST(RegionsBuilderTest_GetCountryTrees)
{
  auto const filename = MakeCollectorData();
  RegionInfo collector(filename);
  std::vector<std::string> bankOfNames;
  RegionsBuilder builder(MakeTestDataSet1(collector), {} /* placePointsMap */);
  builder.ForEachCountry([&](std::string const & name, Node::PtrList const & outers) {
    for (auto const & tree : outers)
    {
      ForEachLevelPath(tree, [&](NodePath const & path) {
        StringJoinPolicy stringifier;
        bankOfNames.push_back(stringifier.ToString(path));
      });
    }
  });

  TEST(NameExists(bankOfNames, "Country_2"), ());
  TEST(NameExists(bankOfNames, "Country_2Country_2_Region_8"), ());

  TEST(NameExists(bankOfNames, "Country_1"), ());
  TEST(NameExists(bankOfNames, "Country_1Country_1_Region_3"), ());
  TEST(NameExists(bankOfNames, "Country_1Country_1_Region_4"), ());
  TEST(NameExists(bankOfNames, "Country_1Country_1_Region_5"), ());
  TEST(NameExists(bankOfNames, "Country_1Country_1_Region_5Country_1_Region_5_Subregion_6"), ());
  TEST(NameExists(bankOfNames, "Country_1Country_1_Region_5Country_1_Region_5_Subregion_7"), ());
}

// Russia regions tests ----------------------------------------------------------------------------
UNIT_TEST(RegionsBuilderTest_GenerateRusCitySuburb)
{
  TagValue const admin{"admin_level"};
  TagValue const place{"place"};
  TagValue const name{"name"};
  TagValue const ba{"boundary", "administrative"};

  auto regions = GenerateTestRegions({
    {1, {name = u8"Россия", admin = "2", ba}, {{0, 0}, {50, 50}}},
    {2, {name = u8"Сибирский федеральный округ", admin = "3", ba}, {{10, 10}, {20, 20}}},
    {3, {name = u8"Омская область", admin = "4", ba}, {{12, 12}, {18, 18}}},
    {4, {name = u8"Омск", place = "city"}, {{14, 14}, {16, 16}}},
    {5, {name = u8"городской округ Омск", admin = "6", ba}, {{14, 14}, {16, 16}},
      {{6, NodeEntry, "admin_centre"}}},
    {6, {name = u8"Омск", place = "city"}, {{14.5, 14.5}}},
    {7, {name = u8"Кировский административный округ", admin = "9", ba}, {{14, 14}, {15, 15}}},
  });

/*
  TEST(HasName(regions, u8"Россия, region: Омская область, subregion: городской округ Омск, "
                        u8"locality: Омск"),
       ());
  TEST(HasName(regions, u8"Россия, region: Омская область, subregion: городской округ Омск, "
                        u8"locality: Омск, suburb: Кировский административный округ"),
       ());
*/
}

UNIT_TEST(RegionsBuilderTest_GenerateRusMoscowSuburb)
{
  TagValue const admin{"admin_level"};
  TagValue const place{"place"};
  TagValue const name{"name"};
  TagValue const ba{"boundary", "administrative"};

  auto regions = GenerateTestRegions({
    {1, {name = u8"Россия", admin = "2", ba}, {{0, 0}, {50, 50}}},
    {2, {name = u8"Центральный федеральный округ", admin = "3", ba}, {{10, 10}, {20, 20}}},
    {3, {name = u8"Москва", admin = "4", ba}, {{12, 12}, {18, 18}}},
    {4, {name = u8"Москва", place = "city"}, {{12, 12}, {17, 17}}},
    {5, {name = u8"Западный административный округ", admin = "5", ba}, {{14, 14}, {16, 16}}},
    {6, {name = u8"район Раменки", admin = "8", ba}, {{14, 14}, {15, 15}}, {{7, NodeEntry, "label"}}},
    {7, {name = u8"Раменки", place = "suburb"}, {{14.5, 14.5}}}, // label
    {8, {name = u8"Тропарёво", place = "suburb"}, {{15.1, 15.1}}}, // no label
    {9, {name = u8"Воробъёвы горы", place = "suburb"}, {{14.5, 14.5}, {14.6, 14.6}}},
    {10, {name = u8"Центр", place = "suburb"}, {{15, 15}, {15.5, 15.5}}},
  });

  TEST(HasName(regions, u8"Россия, region: Москва"), ());
  TEST(HasName(regions, u8"Россия, region: Москва, subregion: Западный административный округ, "
                        u8"locality: Москва"),
       ());
  /* FIXME:
  TEST(HasName(regions, u8"Россия, region: Москва, subregion: Западный административный округ, "
                        u8"locality: Москва, suburb: Раменки"),
       ());
  TEST(HasName(regions, u8"Россия, region: Москва, subregion: Западный административный округ, "
                        u8"locality: Москва, suburb: Раменки, sublocality: Воробъёвы горы"),
       ());
  */
  TEST(HasName(regions, u8"Россия, region: Москва, subregion: Западный административный округ, "
                        u8"locality: Москва, sublocality: Центр"),
       ());
  TEST(!ContainsSubname(regions, u8"Тропарёво"), ());
}

UNIT_TEST(RegionsBuilderTest_GenerateRusSPetersburgSuburb)
{
  TagValue const admin{"admin_level"};
  TagValue const place{"place"};
  TagValue const name{"name"};
  TagValue const ba{"boundary", "administrative"};

  auto regions = GenerateTestRegions({
    {1, {name = u8"Россия", admin = "2", ba}, {{0, 0}, {50, 50}}},
    {2, {name = u8"Северо-Западный федеральный округ", admin = "3", ba}, {{10, 10}, {20, 20}}},
    {3, {name = u8"Санкт-Петербург", admin = "4", ba}, {{12, 12}, {18, 18}}},
    {4, {name = u8"Санкт-Петербург", place = "city"}, {{12, 12}, {17, 17}}},
    {5, {name = u8"Центральный район", admin = "5", ba}, {{14, 14}, {16, 16}}},
    {6, {name = u8"Дворцовый округ", admin = "8", ba}, {{14, 14}, {15, 15}}},
  });

  TEST(HasName(regions, u8"Россия, region: Санкт-Петербург, locality: Санкт-Петербург"), ());
  /* FIXME:
  TEST(HasName(regions, u8"Россия, region: Санкт-Петербург, locality: Санкт-Петербург, "
                        u8"suburb: Центральный район"),
       ());
  TEST(HasName(regions, u8"Россия, region: Санкт-Петербург, locality: Санкт-Петербург, "
                        u8"suburb: Центральный район, sublocality: Дворцовый округ"),
       ());
  */
}

// Transliteration tests ---------------------------------------------------------------------------
using Translations = std::vector<std::pair<std::string, std::string>>;
bool TestTransliteration(Translations const & translations,
                         std::string const & expectedTransliteration)
{
  StringUtf8Multilang regionName;
  for (auto const & langAndTranslation : translations)
  {
    regionName.AddString(langAndTranslation.first, langAndTranslation.second);
  }
  RegionWithName region(regionName);
  return region.GetEnglishOrTransliteratedName() == expectedTransliteration;
}

UNIT_TEST(RegionTransliteration)
{
  Transliteration & translit = Transliteration::Instance();
  translit.Init(GetPlatform().ResourcesDir());

  Translations const scotlandTranslations = {
      {"default", "Scotland"},  {"be", "Шатландыя"},  {"cs", "Skotsko"},   {"cy", "Yr Alban"},
      {"da", "Skotland"},       {"de", "Schottland"}, {"eo", "Skotlando"}, {"es", "Escocia"},
      {"eu", "Eskozia"},        {"fi", "Skotlanti"},  {"fr", "Écosse"},    {"ga", "Albain"},
      {"gd", "Alba"},           {"hr", "Škotska"},    {"ia", "Scotia"},    {"io", "Skotia"},
      {"ja", "スコットランド"}, {"ku", "Skotland"},   {"lfn", "Scotland"}, {"nl", "Schotland"},
      {"pl", "Szkocja"},        {"ru", "Шотландия"},  {"sco", "Scotland"}, {"sk", "Škótsko"},
      {"sr", "Шкотска"},        {"sv", "Skottland"},  {"tok", "Sukosi"},   {"tzl", "Escot"},
      {"uk", "Шотландія"},      {"vo", "Skotän"},     {"zh", "苏格兰"}};

  Translations const michiganTranslations = {
      {"default", "Michigan"}, {"ar", "ميشيغان"},    {"az", "Miçiqan"},   {"be", "Мічыган"},
      {"bg", "Мичиган"},       {"br", "Michigan"},   {"en", "Michigan"},  {"eo", "Miĉigano"},
      {"es", "Míchigan"},      {"fa", "میشیگان"},    {"haw", "Mikikana"}, {"he", "מישיגן"},
      {"hy", "Միչիգան"},       {"ja", "ミシガン州"}, {"ko", "미시간"},    {"lt", "Mičiganas"},
      {"lv", "Mičigana"},      {"nv", "Míshigin"},   {"pl", "Michigan"},  {"ru", "Мичиган"},
      {"sr", "Мичиген"},       {"ta", "மிச்சிகன்"},    {"th", "รัฐมิชิแกน"},   {"tl", "Misigan"},
      {"uk", "Мічиган"},       {"yi", "מישיגן"},     {"zh", "密歇根州"}};

  TEST(TestTransliteration(scotlandTranslations, "Shotlandiya"), ());
  TEST(TestTransliteration(michiganTranslations, "Michigan"), ());
}
