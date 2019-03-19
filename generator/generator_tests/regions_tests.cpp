#include "testing/testing.hpp"

#include "generator/emitter_factory.hpp"
#include "generator/generate_info.hpp"
#include "generator/generator_tests/common.hpp"
#include "generator/osm_element.hpp"
#include "generator/osm2type.hpp"
#include "generator/regions/collector_region_info.hpp"
#include "generator/regions/regions.hpp"
#include "generator/regions/regions_builder.hpp"
#include "generator/regions/to_string_policy.hpp"

#include "indexer/classificator_loader.hpp"

#include "platform/platform.hpp"

#include "coding/file_name_utils.hpp"

#include "base/macros.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <list>
#include <memory>
#include <string>
#include <vector>
#include <utility>

using namespace feature;
using namespace ftype;
using namespace generator;
using namespace generator_tests;
using namespace generator::regions;
using namespace base;
using namespace std;

namespace
{
using Tags = vector<pair<string, string>>;
auto NodeEntry = OsmElement::EntityType::Node;

struct TagValue
{
  string key;
  string value;

  TagValue operator=(string const & value) const { return {key, value}; }
};

TagValue const admin{"admin_level"};
TagValue const place{"place"};
TagValue const name{"name"};
TagValue const ba{"boundary", "administrative"};

struct OsmElementData
{
  uint64_t id;
  vector<TagValue> tags;
  vector<m2::PointD> polygon;
  vector<OsmElement::Member> members;
};

OsmElement MakeOsmElement(uint64_t id, string const & adminLevel, string const & place = "")
{
  OsmElement el;
  el.id = id;
  el.AddTag("place", place);
  el.AddTag("admin_level", adminLevel);

  return el;
}

void CollectRegionInfo(string const & filename, vector<OsmElementData> const & testData)
{
  CollectorRegionInfo collector(filename);

  for (auto const & elementData : testData)
  {
    OsmElement el;
    el.id = elementData.id;
    el.type = elementData.polygon.size() > 1 ? OsmElement::EntityType::Relation : OsmElement::EntityType::Node;
    for (auto const tag : elementData.tags)
      el.AddTag(tag.key, tag.value);
    el.m_members = elementData.members;

    auto osmId = el.type == OsmElement::EntityType::Relation ? MakeOsmRelation(el.id) : MakeOsmNode(el.id);
    collector.Collect(osmId, el);
  }

  collector.Save();
}

void BuildTestData(vector<OsmElementData> const & testData,
                   RegionsBuilder::Regions & regions, PlacePointsMap & placePointsMap,
                   RegionInfo & collector)
{
  for (auto const & elementData : testData)
  {
    OsmElement el;
    el.id = elementData.id;
    el.type = elementData.polygon.size() > 1 ? OsmElement::EntityType::Relation : OsmElement::EntityType::Node;
    for (auto const tag : elementData.tags)
      el.AddTag(tag.key, tag.value);
    el.m_members = elementData.members;

    FeatureBuilder1 fb1;

    CHECK(elementData.polygon.size() == 1 || elementData.polygon.size() == 2, ());
    if (elementData.polygon.size() == 1)
    {
      fb1.SetCenter(elementData.polygon[0]);
    }
    else if (elementData.polygon.size() == 2)
    {
      auto const & p1 = elementData.polygon[0];
      auto const & p2 = elementData.polygon[1];
      fb1.AddPolygon({{p1.x, p1.y}, {p1.x, p2.y}, {p2.x, p2.y}, {p2.x, p1.y}, {p1.x, p1.x}});
      fb1.SetAreaAddHoles({});
    }

    auto osmId = el.type == OsmElement::EntityType::Relation ? MakeOsmRelation(el.id) : MakeOsmNode(el.id);
    fb1.SetOsmId(osmId);

    FeatureParams params;
    GetNameAndType(&el, params, [] (uint32_t type) {
      return classif().IsTypeValid(type);
    });
    fb1.SetParams(params);

    auto const id = fb1.GetMostGenericOsmId();
    if (elementData.polygon.size() == 1)
      placePointsMap.emplace(id, PlacePoint{fb1, collector.Get(id)});
    else
      regions.emplace_back(fb1, collector.Get(id));
  }
}

class Helper : public ToStringPolicyInterface
{
public:
  // ToStringPolicyInterface overrides:
  string ToString(vector<Node::Ptr> const & path) const override
  {
    stringstream stream;
    auto i = path.begin();
    stream << (*i)->GetData().GetName();
    ++i;
    for (; i != path.end(); ++i)
      stream << ", " << GetLabel((*i)->GetData().GetLevel()) << ": " << (*i)->GetData().GetName();

    return stream.str();
  }
};

vector<string> GenerateTestRegions(vector<OsmElementData> const & testData)
{
  classificator::Load();

  auto const filename = GetFileName();
  CollectRegionInfo(filename, testData);

  RegionsBuilder::Regions regions;
  PlacePointsMap placePointsMap;
  RegionInfo collector(filename);
  BuildTestData(testData, regions, placePointsMap, collector);

  RegionsBuilder builder(std::move(regions), std::move(placePointsMap));
  vector<string> kvRegions;
  builder.ForEachCountry([&](string const & name, Node::PtrList const & outers,
      CountryRegionsBuilderStats const &) {
    for (auto const & tree : outers)
    {
      ForEachLevelPath(tree, [&] (vector<Node::Ptr> const & path) {
        kvRegions.push_back(Helper{}.ToString(path));
      });
    }
  });

  return kvRegions;
}

string MakeCollectorData()
{
  auto const filename = GetFileName();
  CollectorRegionInfo collector(filename);
  collector.Collect(MakeOsmRelation(1 /* id */), MakeOsmElement(1 /* id */, "2" /* adminLevel */));
  collector.Collect(MakeOsmRelation(2 /* id */), MakeOsmElement(2 /* id */, "2" /* adminLevel */));
  collector.Collect(MakeOsmRelation(3 /* id */), MakeOsmElement(3 /* id */, "4" /* adminLevel */, "region"));
  collector.Collect(MakeOsmRelation(4 /* id */), MakeOsmElement(4 /* id */, "4" /* adminLevel */, "region"));
  collector.Collect(MakeOsmRelation(5 /* id */), MakeOsmElement(5 /* id */, "4" /* adminLevel */, "region"));
  collector.Collect(MakeOsmRelation(6 /* id */), MakeOsmElement(6 /* id */, "6" /* adminLevel */, "district"));
  collector.Collect(MakeOsmRelation(7 /* id */), MakeOsmElement(7 /* id */, "6" /* adminLevel */, "district"));
  collector.Collect(MakeOsmRelation(8 /* id */), MakeOsmElement(8 /* id */, "4" /* adminLevel */, "region"));
  collector.Save();
  return filename;
}

RegionsBuilder::Regions MakeTestDataSet1(RegionInfo & collector)
{
  RegionsBuilder::Regions regions;
  {
    FeatureBuilder1 fb1;
    fb1.AddName("default", "Country_1");
    fb1.SetOsmId(MakeOsmRelation(1 /* id */));
    fb1.AddPolygon({{2, 8}, {3, 12}, {8, 15}, {13, 12}, {15, 7}, {11, 2}, {4, 4}, {2, 8}});
    fb1.SetAreaAddHoles({{{5, 8}, {7, 10}, {10, 10}, {11, 7}, {10, 4}, {7, 5}, {5, 8}}});
    regions.emplace_back(Region(fb1, collector.Get(MakeOsmRelation(1 /* id */))));
  }

  {
    FeatureBuilder1 fb1;
    fb1.AddName("default", "Country_2");
    fb1.SetOsmId(MakeOsmRelation(2 /* id */));
    fb1.AddPolygon({{5, 8}, {7, 10}, {10, 10}, {11, 7}, {10, 4}, {7, 5}, {5, 8}});
    regions.emplace_back(Region(fb1, collector.Get(MakeOsmRelation(2 /* id */))));
  }

  {
    FeatureBuilder1 fb1;
    fb1.AddName("default", "Country_2");
    fb1.SetOsmId(MakeOsmRelation(2 /* id */));
    fb1.AddPolygon({{0, 0}, {0, 2}, {2, 2}, {2, 0}, {0, 0}});
    regions.emplace_back(Region(fb1, collector.Get(MakeOsmRelation(2 /* id */))));
  }

  {
    FeatureBuilder1 fb1;
    fb1.AddName("default", "Country_1_Region_3");
    fb1.SetOsmId(MakeOsmRelation(3 /* id */));
    fb1.AddPolygon({{4, 4}, {7, 5}, {10, 4}, {12, 9}, {15, 7}, {11, 2}, {4, 4}});
    regions.emplace_back(Region(fb1, collector.Get(MakeOsmRelation(3 /* id */))));
  }

  {
    FeatureBuilder1 fb1;
    fb1.AddName("default", "Country_1_Region_4");
    fb1.SetOsmId(MakeOsmRelation(4 /* id */));
    fb1.AddPolygon({{7, 10}, {9, 12}, {8, 15}, {13, 12}, {15, 7}, {12, 9}, {11, 7}, {10, 10}, {7, 10}});
    regions.emplace_back(Region(fb1, collector.Get(MakeOsmRelation(4 /* id */))));
  }

  {
    FeatureBuilder1 fb1;
    fb1.AddName("default", "Country_1_Region_5");
    fb1.SetOsmId(MakeOsmRelation(5 /* id */));
    fb1.AddPolygon({{4, 4}, {2, 8}, {3, 12}, {8, 15}, {9, 12}, {7, 10}, {5, 8}, {7, 5}, {4, 4}});
    regions.emplace_back(Region(fb1, collector.Get(MakeOsmRelation(5 /* id */))));
  }

  {
    FeatureBuilder1 fb1;
    fb1.AddName("default", "Country_1_Region_5_Subregion_6");
    fb1.SetOsmId(MakeOsmRelation(6 /* id */));
    fb1.AddPolygon({{4, 4}, {2, 8}, {3, 12}, {4, 10}, {5, 10}, {5, 8}, {7, 5}, {4, 4}});
    regions.emplace_back(Region(fb1, collector.Get(MakeOsmRelation(6 /* id */))));
  }

  {
    FeatureBuilder1 fb1;
    fb1.AddName("default", "Country_1_Region_5_Subregion_7");
    fb1.SetOsmId(MakeOsmRelation(7 /* id */));
    fb1.AddPolygon({{3, 12}, {8, 15}, {9, 12}, {7, 10}, {5, 8}, {5, 10}, {4, 10}, {3, 12}});
    regions.emplace_back(Region(fb1, collector.Get(MakeOsmRelation(7 /* id */))));
  }

  {
    FeatureBuilder1 fb1;
    fb1.AddName("default", "Country_2_Region_8");
    fb1.SetOsmId(MakeOsmRelation(8 /* id */));
    fb1.AddPolygon({{0, 0}, {0, 1}, {1, 1}, {1, 0}, {0, 0}});
    regions.emplace_back(Region(fb1, collector.Get(MakeOsmRelation(8 /* id */))));
  }

  return regions;
}

bool ExistsName(vector<string> const & coll, string const name)
{
  auto const end = std::end(coll);
  return std::find(std::begin(coll), end, name) != end;
}

bool ExistsSubname(vector<string> const & coll, string const name)
{
  auto const end = std::end(coll);
  auto hasSubname = [&name] (string const & item) { return item.find(name) != string::npos; };
  return std::find_if(std::begin(coll), end, hasSubname) != end;
}

}  // namespace

UNIT_TEST(RegionsBuilderTest_GetCountryNames)
{
  auto const filename = MakeCollectorData();
  RegionInfo collector(filename);
  RegionsBuilder builder(MakeTestDataSet1(collector), {});
  auto const countryNames = builder.GetCountryNames();
  TEST_EQUAL(countryNames.size(), 2, ());
  TEST(std::count(std::begin(countryNames), std::end(countryNames), "Country_1"), ());
  TEST(std::count(std::begin(countryNames), std::end(countryNames), "Country_2"), ());
}

UNIT_TEST(RegionsBuilderTest_GetCountries)
{
  auto const filename = MakeCollectorData();
  RegionInfo collector(filename);
  RegionsBuilder builder(MakeTestDataSet1(collector), {});
  auto const countries = builder.GetCountriesOuters();
  TEST_EQUAL(countries.size(), 3, ());
  TEST_EQUAL(std::count_if(std::begin(countries), std::end(countries),
                           [](const RegionPlace & p) {return p.GetName() == "Country_1"; }), 1, ());
  TEST_EQUAL(std::count_if(std::begin(countries), std::end(countries),
                           [](const RegionPlace & p) {return p.GetName() == "Country_2"; }), 2, ());
}

UNIT_TEST(RegionsBuilderTest_GetCountryTrees)
{
  auto const filename = MakeCollectorData();
  RegionInfo collector(filename);
  vector<string> bankOfNames;
  RegionsBuilder builder(MakeTestDataSet1(collector), {});
  builder.ForEachCountry([&](string const & name, Node::PtrList const & outers,
      CountryRegionsBuilderStats const &) {
    for (auto const & tree : outers)
    {
      DebugPrintTree(tree);
      ForEachLevelPath(tree, [&] (vector<Node::Ptr> const & path) {
        bankOfNames.push_back(Helper{}.ToString(path));
      });
    }
  });

  TEST(ExistsName(bankOfNames, "Country_2"), ());
  TEST(ExistsName(bankOfNames, "Country_2, region: Country_2_Region_8"), ());

  TEST(ExistsName(bankOfNames, "Country_1"), ());
  TEST(ExistsName(bankOfNames, "Country_1, region: Country_1_Region_3"), ());
  TEST(ExistsName(bankOfNames, "Country_1, region: Country_1_Region_4"), ());
  TEST(ExistsName(bankOfNames, "Country_1, region: Country_1_Region_5"), ());
  TEST(ExistsName(bankOfNames, "Country_1, region: Country_1_Region_5, subregion: Country_1_Region_5_Subregion_6"), ());
  TEST(ExistsName(bankOfNames, "Country_1, region: Country_1_Region_5, subregion: Country_1_Region_5_Subregion_7"), ());
}

UNIT_TEST(RegionsGenerationTest_GenerateCityRegionByPlacePoint)
{
  auto regions = GenerateTestRegions({
    {1, {name = u8"Nederland", admin = "2", ba}, {{0, 0}, {50, 50}}},
    {2, {name = u8"Nederland", admin = "3", ba}, {{10, 10}, {20, 20}}},
    {3, {name = u8"Noord-Holland", admin = "4", ba}, {{12, 12}, {18, 18}}},
    {4, {name = u8"Amsterdam", admin = "8", ba}, {{14, 14}, {16, 16}}},
    {5, {name = u8"Amsterdam", admin = "10", ba}, {{14, 14}, {16, 16}}},
    {6, {name = u8"Amsterdam", place = "city", admin= "2"}, {{14.5, 14.5}}},
  });

  TEST(ExistsName(regions, u8"Nederland"), ());
  TEST(ExistsName(regions, u8"Nederland, locality: Amsterdam"), ());
}

UNIT_TEST(RegionsGenerationTest_GenerateRusCitySuburb)
{
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

  TEST(ExistsName(regions, u8"Россия, region: Омская область, subregion: городской округ Омск, "
                           u8"locality: Омск"), ());
  TEST(ExistsName(regions, u8"Россия, region: Омская область, subregion: городской округ Омск, "
                           u8"locality: Омск, suburb: Кировский административный округ"), ());
}

UNIT_TEST(RegionsGenerationTest_GenerateRusMoscowSuburb)
{
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

  TEST(ExistsName(regions, u8"Россия, region: Москва"), ());
  TEST(ExistsName(regions, u8"Россия, region: Москва, subregion: Западный административный округ, "
                           u8"locality: Москва"), ());
  TEST(ExistsName(regions, u8"Россия, region: Москва, subregion: Западный административный округ, "
                           u8"locality: Москва, suburb: Раменки"), ());
  TEST(ExistsName(regions, u8"Россия, region: Москва, subregion: Западный административный округ, "
                           u8"locality: Москва, suburb: Раменки, sublocality: Воробъёвы горы"), ());
  TEST(ExistsName(regions, u8"Россия, region: Москва, subregion: Западный административный округ, "
                           u8"locality: Москва, sublocality: Центр"), ());
  TEST(!ExistsSubname(regions, u8"Тропарёво"), ());
}

UNIT_TEST(RegionsGenerationTest_GenerateRusSPetersburgSuburb)
{
  auto regions = GenerateTestRegions({
    {1, {name = u8"Россия", admin = "2", ba}, {{0, 0}, {50, 50}}},
    {2, {name = u8"Северо-Западный федеральный округ", admin = "3", ba}, {{10, 10}, {20, 20}}},
    {3, {name = u8"Санкт-Петербург", admin = "4", ba}, {{12, 12}, {18, 18}}},
    {4, {name = u8"Санкт-Петербург", place = "city"}, {{12, 12}, {17, 17}}},
    {5, {name = u8"Центральный район", admin = "5", ba}, {{14, 14}, {16, 16}}},
    {6, {name = u8"Дворцовый округ", admin = "8", ba}, {{14, 14}, {15, 15}}},
  });

  TEST(ExistsName(regions, u8"Россия, region: Санкт-Петербург, locality: Санкт-Петербург"), ());
  TEST(ExistsName(regions, u8"Россия, region: Санкт-Петербург, locality: Санкт-Петербург, "
                           u8"suburb: Центральный район"), ());
  TEST(ExistsName(regions, u8"Россия, region: Санкт-Петербург, locality: Санкт-Петербург, "
                           u8"suburb: Центральный район, sublocality: Дворцовый округ"), ());
}
