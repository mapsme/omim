//
//  intermediate_data_test.cpp
//  generator_tool
//
//  Created by Sergey Yershov on 20.08.15.
//  Copyright (c) 2015 maps.me. All rights reserved.
//

#include "testing/testing.hpp"

#include "generator/generate_info.hpp"
#include "generator/intermediate_data.hpp"
#include "generator/intermediate_elements.hpp"

#include "coding/reader.hpp"
#include "coding/writer.hpp"

#include "defines.hpp"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

using namespace generator;
using namespace std;

UNIT_TEST(Intermediate_Data_empty_way_element_save_load_test)
{
  WayElement e1(1 /* fake osm id */);

  using TBuffer = vector<uint8_t>;
  TBuffer buffer;
  MemWriter<TBuffer> w(buffer);

  e1.Write(w);

  MemReader r(buffer.data(), buffer.size());

  WayElement e2(1 /* fake osm id */);

  e2.Read(r);

  TEST_EQUAL(e2.nodes.size(), 0, ());
}

UNIT_TEST(Intermediate_Data_way_element_save_load_test)
{
  vector<uint64_t> testData = {0, 1, 2, 3, 0xFFFFFFFF, 0xFFFFFFFFFFFFFFFF};

  WayElement e1(1 /* fake osm id */);

  e1.nodes = testData;

  using TBuffer = vector<uint8_t>;
  TBuffer buffer;
  MemWriter<TBuffer> w(buffer);

  e1.Write(w);

  MemReader r(buffer.data(), buffer.size());

  WayElement e2(1 /* fake osm id */);

  e2.Read(r);

  TEST_EQUAL(e2.nodes, testData, ());
}

UNIT_TEST(Intermediate_Data_relation_element_save_load_test)
{
  std::vector<RelationElement::Member> testData = {
      {1, "inner"}, {2, "outer"}, {3, "unknown"}, {4, "inner role"}};

  RelationElement e1;

  e1.nodes = testData;
  e1.ways = testData;

  e1.tags.emplace("key1", "value1");
  e1.tags.emplace("key2", "value2");
  e1.tags.emplace("key3", "value3");
  e1.tags.emplace("key4", "value4");

  using TBuffer = vector<uint8_t>;
  TBuffer buffer;
  MemWriter<TBuffer> w(buffer);

  e1.Write(w);

  MemReader r(buffer.data(), buffer.size());

  RelationElement e2;

  e2.nodes.emplace_back(30, "000unknown");
  e2.nodes.emplace_back(40, "000inner role");
  e2.ways.emplace_back(10, "000inner");
  e2.ways.emplace_back(20, "000outer");
  e2.tags.emplace("key1old", "value1old");
  e2.tags.emplace("key2old", "value2old");

  e2.Read(r);

  TEST_EQUAL(e2.nodes, testData, ());
  TEST_EQUAL(e2.ways, testData, ());

  TEST_EQUAL(e2.tags.size(), 4, ());
  TEST_EQUAL(e2.tags["key1"], "value1", ());
  TEST_EQUAL(e2.tags["key2"], "value2", ());
  TEST_EQUAL(e2.tags["key3"], "value3", ());
  TEST_EQUAL(e2.tags["key4"], "value4", ());

  TEST_NOT_EQUAL(e2.tags["key1old"], "value1old", ());
  TEST_NOT_EQUAL(e2.tags["key2old"], "value2old", ());
}
