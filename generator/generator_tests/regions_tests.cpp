#include "testing/testing.hpp"

#include "generator/osm_element.hpp"
#include "generator/regions.hpp"
#include "generator/region_info_collector.hpp"

#include "platform/platform.hpp"

#include "coding/file_name_utils.hpp"

#include "base/macros.hpp"

#include <algorithm>
#include <limits>
#include <memory>
#include <string>
#include <vector>
#include <utility>

using namespace generator;
using namespace generator::regions;

namespace
{
RegionsBuilder::Regions MakeTestDataSet1()
{
  RegionsBuilder::Regions regions;
  {
    FeatureBuilder1 fb1;
    fb1.AddName("default", "Country_1");
    fb1.SetOsmId(base::GeoObjectId(base::GeoObjectId::Type::OsmRelation, 1));
    vector<m2::PointD> poly = {{2, 8}, {3, 12}, {8, 15}, {13, 12}, {15, 7}, {11, 2}, {4, 4}, {2, 8}};
    fb1.AddPolygon(poly);
    fb1.SetAreaAddHoles({{{5, 8}, {7, 10}, {10, 10}, {11, 7}, {10, 4}, {7, 5}, {5, 8}}});

    RegionData data;
    data.m_osmId = 1;
    data.m_adminLevel = AdminLevel::Two;

    regions.emplace_back(Region(fb1, data));
  }

  {
    FeatureBuilder1 fb1;
    fb1.AddName("default", "Country_2");
    fb1.SetOsmId(base::GeoObjectId(base::GeoObjectId::Type::OsmRelation, 2));
    vector<m2::PointD> poly = {{5, 8}, {7, 10}, {10, 10}, {11, 7}, {10, 4}, {7, 5}, {5, 8}};
    fb1.AddPolygon(poly);

    RegionData data;
    data.m_osmId = 2;
    data.m_adminLevel = AdminLevel::Two;

    regions.emplace_back(Region(fb1, data));
  }

  {
    FeatureBuilder1 fb1;
    fb1.AddName("default", "Country_2");
    fb1.SetOsmId(base::GeoObjectId(base::GeoObjectId::Type::OsmRelation, 2));
    vector<m2::PointD> poly = {{0, 0}, {0, 2}, {2, 2}, {2, 0}, {0, 0}};
    fb1.AddPolygon(poly);

    RegionData data;
    data.m_osmId = 2;
    data.m_adminLevel = AdminLevel::Two;

    regions.emplace_back(Region(fb1, data));
  }

  {
    FeatureBuilder1 fb1;
    fb1.AddName("default", "Country_1_Region_3");
    fb1.SetOsmId(base::GeoObjectId(base::GeoObjectId::Type::OsmRelation, 3));
    vector<m2::PointD> poly = {{4, 4}, {7, 5}, {10, 4}, {12, 9}, {15, 7}, {11, 2}, {4, 4}};
    fb1.AddPolygon(poly);

    RegionData data;
    data.m_osmId = 3;
    data.m_adminLevel = AdminLevel::Four;

    regions.emplace_back(Region(fb1, data));
  }

  {
    FeatureBuilder1 fb1;
    fb1.AddName("default", "Country_1_Region_4");
    fb1.SetOsmId(base::GeoObjectId(base::GeoObjectId::Type::OsmRelation, 4));
    vector<m2::PointD> poly = {{7, 10}, {9, 12}, {8, 15}, {13, 12}, {15, 7}, {12, 9},
                               {11, 7}, {10, 10}, {7, 10}};
    fb1.AddPolygon(poly);

    RegionData data;
    data.m_osmId = 4;
    data.m_adminLevel = AdminLevel::Four;

    regions.emplace_back(Region(fb1, data));
  }

  {
    FeatureBuilder1 fb1;
    fb1.AddName("default", "Country_1_Region_5");
    fb1.SetOsmId(base::GeoObjectId(base::GeoObjectId::Type::OsmRelation, 5));
    vector<m2::PointD> poly = {{4, 4}, {2, 8}, {3, 12}, {8, 15}, {9, 12}, {7, 10}, {5, 8},
                               {7, 5}, {4, 4}};
    fb1.AddPolygon(poly);

    RegionData data;
    data.m_osmId = 5;
    data.m_adminLevel = AdminLevel::Four;

    regions.emplace_back(Region(fb1, data));
  }

  {
    FeatureBuilder1 fb1;
    fb1.AddName("default", "Country_1_Region_5_Subregion_6");
    fb1.SetOsmId(base::GeoObjectId(base::GeoObjectId::Type::OsmRelation, 6));
    vector<m2::PointD> poly = {{4, 4}, {2, 8}, {3, 12}, {4, 10}, {5, 10}, {5, 8}, {7, 5}, {4, 4}};
    fb1.AddPolygon(poly);

    RegionData data;
    data.m_osmId = 6;
    data.m_adminLevel = AdminLevel::Six;

    regions.emplace_back(Region(fb1, data));
  }

  {
    FeatureBuilder1 fb1;
    fb1.AddName("default", "Country_1_Region_5_Subregion_7");
    fb1.SetOsmId(base::GeoObjectId(base::GeoObjectId::Type::OsmRelation, 7));
    vector<m2::PointD> poly = {{3, 12}, {8, 15}, {9, 12}, {7, 10}, {5, 8}, {5, 10}, {4, 10}, {3, 12}};
    fb1.AddPolygon(poly);

    RegionData data;
    data.m_osmId = 7;
    data.m_adminLevel = AdminLevel::Six;

    regions.emplace_back(Region(fb1, data));
  }

  {
    FeatureBuilder1 fb1;
    fb1.AddName("default", "Country_2_Region_8");
    fb1.SetOsmId(base::GeoObjectId(base::GeoObjectId::Type::OsmRelation, 8));
    vector<m2::PointD> poly = {{0, 0}, {0, 1}, {1, 1}, {1, 0}, {0, 0}};
    fb1.AddPolygon(poly);

    RegionData data;
    data.m_osmId = 8;
    data.m_adminLevel = AdminLevel::Four;

    regions.emplace_back(Region(fb1, data));
  }

  return regions;
}

class Helper : public ToStringPolicyInterface
{
public:
  Helper(std::vector<std::string> & bankOfNames) : m_bankOfNames(bankOfNames)
  {
  }

  std::string ToString(Node::PtrList const & nodePtrList) override
  {
    std::stringstream stream;
    for (auto const & n : nodePtrList)
      stream << n->GetData().GetName();

    auto str = stream.str();
    m_bankOfNames.push_back(str);
    return str;
  }

  std::vector<std::string> & m_bankOfNames;
};

bool ExistsName(std::vector<std::string> const & coll, std::string const name)
{
  auto const end = std::end(coll);
  return std::find(std::begin(coll), end, name) != end;
}
}  // namespace

UNIT_TEST(RegionsBuilderTest_GetCountryNames)
{
  RegionsBuilder builder(MakeTestDataSet1());
  auto const countryNames = builder.GetCountryNames();
  TEST_EQUAL(countryNames.size(), 2, ());
  TEST(std::count(std::begin(countryNames), std::end(countryNames), "Country_1"), ());
  TEST(std::count(std::begin(countryNames), std::end(countryNames), "Country_2"), ());
}

UNIT_TEST(RegionsBuilderTest_GetCountries)
{
  RegionsBuilder builder(MakeTestDataSet1());
  auto const countries = builder.GetCountries();
  TEST_EQUAL(countries.size(), 3, ());
  TEST_EQUAL(std::count_if(std::begin(countries), std::end(countries),
                           [](const Region & r) {return r.GetName() == "Country_1"; }), 1, ());
  TEST_EQUAL(std::count_if(std::begin(countries), std::end(countries),
                           [](const Region & r) {return r.GetName() == "Country_2"; }), 2, ());
}

UNIT_TEST(RegionsBuilderTest_GetCountryTrees)
{
  std::vector<std::string> bankOfNames;
  RegionsBuilder builder(MakeTestDataSet1(), std::make_unique<Helper>(bankOfNames));

  auto const countryTrees = builder.GetCountryTrees();
  for (auto const & countryName : builder.GetCountryNames())
  {
    auto const keyRange = countryTrees.equal_range(countryName);
    for (auto it = keyRange.first; it != keyRange.second; ++it)
    {
      auto const unused = builder.ToIdStringList(it->second);
      UNUSED_VALUE(unused);
    }
  }

  TEST_EQUAL(std::count(std::begin(bankOfNames), std::end(bankOfNames), "Country_2"), 2, ());
  TEST(ExistsName(bankOfNames, "Country_1"), ());
  TEST(ExistsName(bankOfNames, "Country_1_Region_3Country_1"), ());
  TEST(ExistsName(bankOfNames, "Country_1_Region_4Country_1"), ());
  TEST(ExistsName(bankOfNames, "Country_1_Region_5Country_1"), ());
  TEST(ExistsName(bankOfNames, "Country_2_Region_8Country_2"), ());
  TEST(ExistsName(bankOfNames, "Country_1_Region_5_Subregion_6Country_1_Region_5Country_1"), ());
  TEST(ExistsName(bankOfNames, "Country_1_Region_5_Subregion_7Country_1_Region_5Country_1"), ());
}
