//
//  intermediate_data_test.cpp
//  generator_tool
//
//  Created by Sergey Yershov on 20.08.15.
//  Copyright (c) 2015 maps.me. All rights reserved.
//

#include "testing/testing.hpp"

#include "platform/platform.hpp"
#include "platform/platform_tests_support/scoped_dir.hpp"
#include "platform/platform_tests_support/scoped_file.hpp"

#include "generator/generate_info.hpp"
#include "generator/intermediate_data.hpp"
#include "generator/intermediate_elements.hpp"
#include "generator/osm_source.hpp"

#include "defines.hpp"

using namespace platform::tests_support;
using namespace platform;
using namespace generator;
using namespace feature;
using namespace cache;

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
  RelationElement::TMembers testData = {{1, "inner"}, {2, "outer"}, {3, "unknown"}, {4, "inner role"}};

  RelationElement e1;

  e1.nodes = testData;
  e1.ways = testData;

  e1.tags.emplace("key1","value1");
  e1.tags.emplace("key2","value2");
  e1.tags.emplace("key3","value3");
  e1.tags.emplace("key4","value4");

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
  e2.tags.emplace("key1old","value1old");
  e2.tags.emplace("key2old","value2old");

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

UNIT_TEST(Intermediate_Data_cache_nodes_to_ways_test)
{
  string const osmSourceXML = R"(
<osm version="0.6" generator="osmconvert 0.8.4" timestamp="2018-07-16T02:00:00Z">

    <node id="1" lat="55.779384" lon="37.3699375" version="1"></node>
    <node id="2" lat="55.779304" lon="37.3699375" version="1"></node>
    <node id="3" lat="55.773084" lon="37.3699375" version="1"></node>

    <way id="10" version="1">
      <nd ref="1"/>
    </way>
    <way id="20" version="1">
      <nd ref="1"/>
      <nd ref="2"/>
      <nd ref="3"/>
    </way>

  </osm>
                                )";

  // Directory name for creating test mwm and temporary files.
  string const kTestDir = "nodes_to_ways_test";
  string const kOsmFileName = "town" OSM_DATA_FILE_EXTENSION;

  Platform & platform = GetPlatform();

  string const tmpDir = platform.TmpDir();
  platform.SetWritableDirForTests(tmpDir);

  string const & writableDir = platform.WritableDir();

  // Create test dir into kTmpDir
  ScopedDir const scopedDir(kTestDir);

  string const osmRelativePath = my::JoinPath(kTestDir, kOsmFileName);
  ScopedFile const osmScopedFile(osmRelativePath, osmSourceXML);

  // Generate intermediate data
  GenerateInfo genInfo;
  genInfo.m_intermediateDir = writableDir;
  genInfo.m_nodeStorageType = feature::GenerateInfo::NodeStorageType::Index;
  genInfo.m_osmFileName = my::JoinPath(tmpDir, osmRelativePath);
  genInfo.m_osmFileType = feature::GenerateInfo::OsmSourceType::XML;

  // Test save intermediate data is OK.
  TEST(GenerateIntermediateData(genInfo), ("Can not generate intermediate data for speed cam"));

  // Test load this data from cached file.
  IntermediateDataReader cache({}, genInfo);
  cache.LoadIndex();

  std::set<std::pair<uint64_t, uint64_t>> trueAnswers = {
    {1, 10}, {1, 20}, {2, 20}, {3, 20}
  };

  for (uint64_t i = 1; i <= 3UL; ++i) {
    cache.ForEachWayByNode(i, [&](uint64_t wayId) {
      auto const it = trueAnswers.find({i, wayId});
      TEST(it != trueAnswers.cend(), ("Has found pair, that should not be here"));
      trueAnswers.erase(it);
      return base::ControlFlow::Continue;
    });
  }

  TEST(trueAnswers.empty(), ("Some data didn't cached!"));
}
