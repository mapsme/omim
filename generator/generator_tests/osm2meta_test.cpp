#include <testing/testing.hpp>

#include <generator/osm2meta.hpp>

UNIT_TEST(ValidateAndFormat_cuisine_test)
{
  FeatureParams params;
  MetadataTagProcessorImpl tagProc(params);
  TEST_EQUAL(tagProc.ValidateAndFormat_cuisine(" ,ABC, CDE;   FgH,   "), "abc;cde;fgh", ());
  TEST_EQUAL(tagProc.ValidateAndFormat_cuisine(";;;ABc,   cef,,,"), "abc;cef", ());
  TEST_EQUAL(tagProc.ValidateAndFormat_cuisine("abc bca"), "abc_bca", ());
  TEST_EQUAL(tagProc.ValidateAndFormat_cuisine("abc      def  gh"), "abc_def_gh", ());
  TEST_EQUAL(tagProc.ValidateAndFormat_cuisine(""), "", ());
  TEST_EQUAL(tagProc.ValidateAndFormat_cuisine(" ; , "), "", ());
  TEST_EQUAL(tagProc.ValidateAndFormat_cuisine(" Korean bbq;barbeque;grill,bbq; "), "korean_bbq;barbecue;grill", ());
}

UNIT_TEST(ValidateAndFormat_ele)
{
  FeatureParams params;
  MetadataTagProcessorImpl tagProc(params);
  TEST_EQUAL(tagProc.ValidateAndFormat_ele(""), "", ());
  TEST_EQUAL(tagProc.ValidateAndFormat_ele("not a number"), "", ());
  TEST_EQUAL(tagProc.ValidateAndFormat_ele("0"), "0", ());
  TEST_EQUAL(tagProc.ValidateAndFormat_ele("0.0"), "0", ());
  TEST_EQUAL(tagProc.ValidateAndFormat_ele("0.0000000"), "0", ());
  TEST_EQUAL(tagProc.ValidateAndFormat_ele("22.5"), "22.5", ());
  TEST_EQUAL(tagProc.ValidateAndFormat_ele("-100.3"), "-100.3", ());
  TEST_EQUAL(tagProc.ValidateAndFormat_ele("99.0000000"), "99", ());
  TEST_EQUAL(tagProc.ValidateAndFormat_ele("8900.000023"), "8900", ());
  TEST_EQUAL(tagProc.ValidateAndFormat_ele("-300.9999"), "-301", ());
  TEST_EQUAL(tagProc.ValidateAndFormat_ele("-300.9"), "-300.9", ());
  TEST_EQUAL(tagProc.ValidateAndFormat_ele("15 m"), "15", ());
  TEST_EQUAL(tagProc.ValidateAndFormat_ele("15.9 m"), "15.9", ());
  TEST_EQUAL(tagProc.ValidateAndFormat_ele("15.9m"), "15.9", ());
  TEST_EQUAL(tagProc.ValidateAndFormat_ele("3000 ft"), "914.4", ());
  TEST_EQUAL(tagProc.ValidateAndFormat_ele("3000ft"), "914.4", ());
  TEST_EQUAL(tagProc.ValidateAndFormat_ele("100 feet"), "30.48", ());
  TEST_EQUAL(tagProc.ValidateAndFormat_ele("100feet"), "30.48", ());
  TEST_EQUAL(tagProc.ValidateAndFormat_ele("11'"), "3.35", ());
  TEST_EQUAL(tagProc.ValidateAndFormat_ele("11'4\""), "3.45", ());
}

UNIT_TEST(ValidateAndFormat_building_levels)
{
  FeatureParams params;
  MetadataTagProcessorImpl tp(params);
  TEST_EQUAL(tp.ValidateAndFormat_building_levels("４"), "4", ());
  TEST_EQUAL(tp.ValidateAndFormat_building_levels("４floors"), "4", ());
  TEST_EQUAL(tp.ValidateAndFormat_building_levels("between 1 and ４"), "", ());
  TEST_EQUAL(tp.ValidateAndFormat_building_levels("0"), "0", ("OSM has many zero-level buildings."));
  TEST_EQUAL(tp.ValidateAndFormat_building_levels("0.0"), "0", ());
  TEST_EQUAL(tp.ValidateAndFormat_building_levels(""), "", ());
  TEST_EQUAL(tp.ValidateAndFormat_building_levels("Level 1"), "", ());
  TEST_EQUAL(tp.ValidateAndFormat_building_levels("2.51"), "2.5", ());
  TEST_EQUAL(tp.ValidateAndFormat_building_levels("250"), "", ("Too many levels."));
}

UNIT_TEST(ValidateAndFormat_sponsored_vendor)
{
  FeatureParams params;
  MetadataTagProcessorImpl tp(params);
  TEST_EQUAL(tp.ValidateAndFormat_sponsored_vendor(""), "", ());
  TEST_EQUAL(tp.ValidateAndFormat_sponsored_vendor("test"), "test", ());
  TEST_EQUAL(tp.ValidateAndFormat_sponsored_vendor("12eq_5"), "12eq_5", ());
  TEST_EQUAL(tp.ValidateAndFormat_sponsored_vendor("a.b"), "", ());
  TEST_EQUAL(tp.ValidateAndFormat_sponsored_vendor("b_"), "b_", ());
}

UNIT_TEST(ValidateAndFormat_sponsored_date)
{
  FeatureParams params;
  MetadataTagProcessorImpl tp(params);
  string const date2016_11_05 = "1478293";  // 1478293200
  string const date2016_11_06 = "1478379";  // 1478379600
  string const date2016_11_07 = "1478466";  // 1478466000
  TEST_EQUAL(tp.ValidateAndFormat_sponsored_date(""), "", ());
  TEST_EQUAL(tp.ValidateAndFormat_sponsored_date("asd-w-3"), "", ());
  TEST_EQUAL(tp.ValidateAndFormat_sponsored_date("2016-11-05"), date2016_11_05, ());
  TEST_EQUAL(tp.ValidateAndFormat_sponsored_date("2016-11-05", true), date2016_11_06, ());
  TEST_EQUAL(tp.ValidateAndFormat_sponsored_date("2016-11"), "", ());
  TEST_EQUAL(tp.ValidateAndFormat_sponsored_date("2015-12-31"), "", ());
}
