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
using Tags = std::vector<std::pair<std::string, std::string>>;

struct TagValue
{
  string key;
  string value;

  TagValue operator = (string const & value)
  { return {key, value}; }
};

TagValue admin{"admin_level"};
TagValue place{"place"};
TagValue name{"name"};
TagValue ba{"boundary", "administrative"};

struct OsmElementData
{
  uint64_t id;
  vector<TagValue> tags;
  vector<m2::PointD> polygon;
};

OsmElement MakeOsmElement(uint64_t id, std::string const & adminLevel,
                          std::string const & place = "")
{
  OsmElement el;
  el.id = id;
  el.AddTag("place", place);
  el.AddTag("admin_level", adminLevel);

  return el;
}

void BuildTestData(GenerateInfo const & genInfo, string const & collectorFilename,
    vector<OsmElementData> const & testData)
{
  classificator::Load();

  CollectorRegionInfo collector(collectorFilename);
  auto emitter = CreateEmitter(EmitterType::Simple, genInfo);

  for (auto const & elementData : testData)
  {
    OsmElement el;
    el.id = elementData.id;
    el.type = elementData.polygon.size() > 1 ? OsmElement::EntityType::Relation : OsmElement::EntityType::Node;
    for (auto const tag : elementData.tags)
      el.AddTag(tag.key, tag.value);

    collector.Collect(MakeOsmRelation(el.id), el);

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
      vector<m2::PointD> poly{{p1.x, p1.y}, {p1.x, p2.y}, {p2.x, p2.y}, {p2.x, p1.y}, {p1.x, p1.x}};
      fb1.AddPolygon(poly);
      fb1.SetAreaAddHoles({});
    }

    fb1.SetOsmId(MakeOsmRelation(elementData.id));

    FeatureParams params;
    GetNameAndType(&el, params, [] (uint32_t type) {
      return classif().IsTypeValid(type);
    });
    fb1.SetParams(params);

    (*emitter)(fb1);
  }

  collector.Save();
}

std::string MakeCollectorData()
{
  auto const filename = GetFileName();
  CollectorRegionInfo collector(filename);
  collector.Collect(MakeOsmRelation(1 /* id */), MakeOsmElement(1 /* id */, "2" /* adminLevel */));
  collector.Collect(MakeOsmRelation(2 /* id */), MakeOsmElement(2 /* id */, "2" /* adminLevel */));
  collector.Collect(MakeOsmRelation(3 /* id */), MakeOsmElement(3 /* id */, "4" /* adminLevel */));
  collector.Collect(MakeOsmRelation(4 /* id */), MakeOsmElement(4 /* id */, "4" /* adminLevel */));
  collector.Collect(MakeOsmRelation(5 /* id */), MakeOsmElement(5 /* id */, "4" /* adminLevel */));
  collector.Collect(MakeOsmRelation(6 /* id */), MakeOsmElement(6 /* id */, "6" /* adminLevel */));
  collector.Collect(MakeOsmRelation(7 /* id */), MakeOsmElement(7 /* id */, "6" /* adminLevel */));
  collector.Collect(MakeOsmRelation(8 /* id */), MakeOsmElement(8 /* id */, "4" /* adminLevel */));
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
    vector<m2::PointD> poly = {{2, 8}, {3, 12}, {8, 15}, {13, 12}, {15, 7}, {11, 2}, {4, 4}, {2, 8}};
    fb1.AddPolygon(poly);
    fb1.SetAreaAddHoles({{{5, 8}, {7, 10}, {10, 10}, {11, 7}, {10, 4}, {7, 5}, {5, 8}}});
    regions.emplace_back(Region(fb1, collector.Get(MakeOsmRelation(1 /* id */))));
  }

  {
    FeatureBuilder1 fb1;
    fb1.AddName("default", "Country_2");
    fb1.SetOsmId(MakeOsmRelation(2 /* id */));
    vector<m2::PointD> poly = {{5, 8}, {7, 10}, {10, 10}, {11, 7}, {10, 4}, {7, 5}, {5, 8}};
    fb1.AddPolygon(poly);
    regions.emplace_back(Region(fb1, collector.Get(MakeOsmRelation(2 /* id */))));
  }

  {
    FeatureBuilder1 fb1;
    fb1.AddName("default", "Country_2");
    fb1.SetOsmId(MakeOsmRelation(2 /* id */));
    vector<m2::PointD> poly = {{0, 0}, {0, 2}, {2, 2}, {2, 0}, {0, 0}};
    fb1.AddPolygon(poly);
    regions.emplace_back(Region(fb1, collector.Get(MakeOsmRelation(2 /* id */))));
  }

  {
    FeatureBuilder1 fb1;
    fb1.AddName("default", "Country_1_Region_3");
    fb1.SetOsmId(MakeOsmRelation(3 /* id */));
    vector<m2::PointD> poly = {{4, 4}, {7, 5}, {10, 4}, {12, 9}, {15, 7}, {11, 2}, {4, 4}};
    fb1.AddPolygon(poly);
    regions.emplace_back(Region(fb1, collector.Get(MakeOsmRelation(3 /* id */))));
  }

  {
    FeatureBuilder1 fb1;
    fb1.AddName("default", "Country_1_Region_4");
    fb1.SetOsmId(MakeOsmRelation(4 /* id */));
    vector<m2::PointD> poly = {{7, 10}, {9, 12}, {8, 15}, {13, 12}, {15, 7}, {12, 9},
                               {11, 7}, {10, 10}, {7, 10}};
    fb1.AddPolygon(poly);
    regions.emplace_back(Region(fb1, collector.Get(MakeOsmRelation(4 /* id */))));
  }

  {
    FeatureBuilder1 fb1;
    fb1.AddName("default", "Country_1_Region_5");
    fb1.SetOsmId(MakeOsmRelation(5 /* id */));
    vector<m2::PointD> poly = {{4, 4}, {2, 8}, {3, 12}, {8, 15}, {9, 12}, {7, 10}, {5, 8},
                               {7, 5}, {4, 4}};
    fb1.AddPolygon(poly);
    regions.emplace_back(Region(fb1, collector.Get(MakeOsmRelation(5 /* id */))));
  }

  {
    FeatureBuilder1 fb1;
    fb1.AddName("default", "Country_1_Region_5_Subregion_6");
    fb1.SetOsmId(MakeOsmRelation(6 /* id */));
    vector<m2::PointD> poly = {{4, 4}, {2, 8}, {3, 12}, {4, 10}, {5, 10}, {5, 8}, {7, 5}, {4, 4}};
    fb1.AddPolygon(poly);
    regions.emplace_back(Region(fb1, collector.Get(MakeOsmRelation(6 /* id */))));
  }

  {
    FeatureBuilder1 fb1;
    fb1.AddName("default", "Country_1_Region_5_Subregion_7");
    fb1.SetOsmId(MakeOsmRelation(7 /* id */));
    vector<m2::PointD> poly = {{3, 12}, {8, 15}, {9, 12}, {7, 10}, {5, 8}, {5, 10}, {4, 10}, {3, 12}};
    fb1.AddPolygon(poly);
    regions.emplace_back(Region(fb1, collector.Get(MakeOsmRelation(7 /* id */))));
  }

  {
    FeatureBuilder1 fb1;
    fb1.AddName("default", "Country_2_Region_8");
    fb1.SetOsmId(MakeOsmRelation(8 /* id */));
    vector<m2::PointD> poly = {{0, 0}, {0, 1}, {1, 1}, {1, 0}, {0, 0}};
    fb1.AddPolygon(poly);
    regions.emplace_back(Region(fb1, collector.Get(MakeOsmRelation(8 /* id */))));
  }

  return regions;
}

GenerateInfo TestGenerateInfo()
{
  Platform & platform = GetPlatform();

  auto const tmpDir = platform.TmpDir();
  platform.SetWritableDirForTests(tmpDir);

  GenerateInfo genInfo;
  genInfo.m_tmpDir = tmpDir;
  return genInfo;
}

class Helper : public ToStringPolicyInterface
{
public:
  std::string ToString(Node::PtrList const & nodePtrList) const override
  {
    std::stringstream stream;
    auto i = nodePtrList.rbegin();
    stream << (*i)->GetData().GetName();
    ++i;
    for (; i != nodePtrList.rend(); ++i)
      stream << "," << (*i)->GetData().GetName();

    return stream.str();
  }
};

bool ExistsName(std::vector<std::string> const & coll, std::string const name)
{
  auto const end = std::end(coll);
  return std::find(std::begin(coll), end, name) != end;
}

bool ExistsSubname(std::vector<std::string> const & coll, std::string const name)
{
  auto const end = std::end(coll);
  auto hasSubname = [&name] (std::string const & item) { return item.find(name) != std::string::npos; };
  return std::find_if(std::begin(coll), end, hasSubname) != end;
}

}  // namespace

UNIT_TEST(RegionsBuilderTest_GetCountryNames)
{
  auto const filename = MakeCollectorData();
  RegionInfo collector(filename);
  RegionsBuilder builder(MakeTestDataSet1(collector));
  auto const countryNames = builder.GetCountryNames();
  TEST_EQUAL(countryNames.size(), 2, ());
  TEST(std::count(std::begin(countryNames), std::end(countryNames), "Country_1"), ());
  TEST(std::count(std::begin(countryNames), std::end(countryNames), "Country_2"), ());
}

UNIT_TEST(RegionsBuilderTest_GetCountries)
{
  auto const filename = MakeCollectorData();
  RegionInfo collector(filename);
  RegionsBuilder builder(MakeTestDataSet1(collector));
  auto const countries = builder.GetCountries();
  TEST_EQUAL(countries.size(), 3, ());
  TEST_EQUAL(std::count_if(std::begin(countries), std::end(countries),
                           [](const Region & r) {return r.GetName() == "Country_1"; }), 1, ());
  TEST_EQUAL(std::count_if(std::begin(countries), std::end(countries),
                           [](const Region & r) {return r.GetName() == "Country_2"; }), 2, ());
}

UNIT_TEST(RegionsBuilderTest_GetCountryTrees)
{
  auto const filename = MakeCollectorData();
  RegionInfo collector(filename);
  std::vector<std::string> bankOfNames;
  RegionsBuilder builder(MakeTestDataSet1(collector), std::make_unique<Helper>());
  builder.ForEachNormalizedCountry([&](std::string const & name, Node::Ptr const & tree) {
    auto const idStringList = builder.ToIdStringList(tree);
    for (auto const & idString : idStringList)
      bankOfNames.push_back(idString.second);
  });

  TEST(ExistsName(bankOfNames, "Country_2"), ());
  TEST(ExistsName(bankOfNames, "Country_2,Country_2_Region_8"), ());

  TEST(ExistsName(bankOfNames, "Country_1"), ());
  TEST(ExistsName(bankOfNames, "Country_1,Country_1_Region_3"), ());
  TEST(ExistsName(bankOfNames, "Country_1,Country_1_Region_4"), ());
  TEST(ExistsName(bankOfNames, "Country_1,Country_1_Region_5"), ());
  TEST(ExistsName(bankOfNames, "Country_1,Country_1_Region_5,Country_1_Region_5_Subregion_6"), ());
  TEST(ExistsName(bankOfNames, "Country_1,Country_1_Region_5,Country_1_Region_5_Subregion_7"), ());
}

UNIT_TEST(RegionsBuilderTest_GetRusFedSuburbNode)
{
  auto const collectorFilename = GetFileName();
  auto genInfo = TestGenerateInfo();
  genInfo.m_fileName = "regions";

  BuildTestData(genInfo, collectorFilename, {
    {1, {name = "Russia", admin = "2", ba}, {{0, 0}, {50, 50}}},
    {2, {name = "Central Federal District", admin = "3", ba}, {{10, 10}, {20, 20}}},
    {3, {name = "MOW", admin = "4", ba}, {{12, 12}, {18, 18}}},
    {4, {name = "Moscow", place = "city"}, {{12, 12}, {17, 17}}},
    {5, {name = "ZAO", admin = "5", ba}, {{14, 14}, {16, 16}}},
    {6, {name = "Ramenki District", admin = "8", ba}, {{14, 14}, {15, 15}}},
    {7, {name = "Ramenki Suburb", place = "suburb"}, {{14.5, 14.5}}},
    {8, {name = "Outside Suburb", place = "suburb"}, {{15.5, 15.5}}},
  });

  RegionInfo collector(collectorFilename);
  auto pathInRegionsMwm = genInfo.GetTmpFileName(genInfo.m_fileName);
  auto builder = StartRegionsBuilder(pathInRegionsMwm, collector, std::make_unique<Helper>());
  std::vector<std::string> regions;
  builder.ForEachNormalizedCountry([&](std::string const & name, Node::Ptr const & tree) {
    auto const idStringList = builder.ToIdStringList(tree);
    for (auto const & idString : idStringList)
      regions.push_back(idString.second);
  });

  TEST(ExistsName(regions, "Russia,MOW,Moscow,Ramenki Suburb"), ());
  TEST(!ExistsSubname(regions, "Outside Suburb"), ());
}
