#include "testing/testing.hpp"

#include "platform/local_country_file.hpp"
#include "platform/platform.hpp"
#include "platform/platform_tests_support/scoped_dir.hpp"
#include "platform/platform_tests_support/scoped_file.hpp"

#include "generator/camera_info_collector.hpp"
#include "generator/emitter_factory.hpp"
#include "generator/feature_sorter.hpp"
#include "generator/generate_info.hpp"
#include "generator/generator_tests_support/routing_helpers.hpp"
#include "generator/generator_tests_support/test_mwm_builder.hpp"
#include "generator/metalines_builder.hpp"
#include "generator/osm_source.hpp"

#include "routing/deserialize_speed_cameras.hpp"

#include "indexer/classificator_loader.hpp"
#include "indexer/index_builder.hpp"
#include "indexer/map_style_reader.hpp"

#include "coding/file_name_utils.hpp"
#include "coding/internal/file_data.hpp"

#include "geometry/point2d.hpp"

#include "defines.hpp"

#include <cmath>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace std;

namespace
{
// Directory name for creating test mwm and temporary files.
string const kTestDir = "speed_camera_generation_test";

// Temporary mwm name for testing.
string const kTestMwm = "test";

string const kSpeedCameraDataFileName = "speedcamera_in_osm_ids.bin";
string const kOsmIdsToFeatureIdsName = "osm_ids_to_feature_ids" OSM2FEATURE_FILE_EXTENSION;
string const kIntermediateFileName = "intermediate_data";
string const kOsmFileName = "town" OSM_DATA_FILE_EXTENSION;

double constexpr kCoefEqualityEpsilonm = 1e-5;

using namespace platform::tests_support;
using namespace platform;
using namespace generator;
using namespace feature;
using namespace routing;

// Pair of featureId and segmentId
using routing::SegmentCoord;
using CameraMap = multimap<SegmentCoord, RouteSegment::SpeedCamera>;
using CameraMapItem = pair<SegmentCoord, RouteSegment::SpeedCamera>;

CameraMap LoadSpeedCameraFromMwm(string const & mwmFullPath)
{
  FilesContainerR const cont(mwmFullPath);
  CHECK(cont.IsExist(CAMERAS_INFO_FILE_TAG), ("Cannot find", CAMERAS_INFO_FILE_TAG, "section"));

  try
  {
    FilesContainerR::TReader const reader = cont.GetReader(CAMERAS_INFO_FILE_TAG);
    ReaderSource<FilesContainerR::TReader> src(reader);

    CameraMap result;
    DeserializeSpeedCamsFromMwm(src, result);
    return result;
  }
  catch (Reader::OpenException const & e)
  {
    TEST(false, ("Error while reading", CAMERAS_INFO_FILE_TAG, "section.", e.Msg()));
    return {};
  }
}

bool CheckCameraMapsEquality(CameraMap const & lhs, CameraMap const & rhs)
{
  if (lhs.size() != rhs.size())
    return false;

  vector<CameraMapItem> left, right;
  for (auto it : lhs)
    left.emplace_back(it.first, it.second);

  for (auto it : rhs)
    right.emplace_back(it.first, it.second);

  for (auto const & itemL : left)
  {
    bool found = false;
    for (auto const & itemR : right)
    {
      uint32_t featureID = itemR.first.first;
      uint32_t segmentID = itemR.first.second;
      double coef = itemR.second.first;
      uint8_t maxSpeed = itemR.second.second;

      if (itemL.first.first == featureID &&
          itemL.first.second == segmentID &&
          itemL.second.second == maxSpeed &&
          abs(coef - itemL.second.first) < kCoefEqualityEpsilonm)
      {
        found = true;
        break;
      }
    }
    if (!found)
      return false;
  }

  return true;
}

void TestSpeedCameraDataBuilding(string const & osmContent, CameraMap const & answer)
{
  GetStyleReader().SetCurrentStyle(MapStyleMerged);
  classificator::Load();
  Platform & platform = GetPlatform();

  string const & writableDir = platform.WritableDir();

  // Create test dir
  string const & testDirFullPath = my::JoinPath(writableDir, kTestDir);
  (void)Platform::MkDir(testDirFullPath);

  string const osmRelativePath = my::JoinPath(kTestDir, kOsmFileName);
  ScopedFile const osmScopedFile(osmRelativePath, osmContent);

  // Step 1. Generate intermediate data.
  GenerateInfo genInfo;
  genInfo.m_fileName = kTestMwm;
  genInfo.m_bucketNames.push_back(kTestMwm);
  genInfo.m_tmpDir = testDirFullPath;
  genInfo.m_targetDir = testDirFullPath;
  genInfo.m_intermediateDir = testDirFullPath;
  genInfo.m_nodeStorageType = feature::GenerateInfo::NodeStorageType::Index;
  genInfo.m_osmFileName = my::JoinPath(writableDir, osmRelativePath);
  genInfo.m_osmFileType = feature::GenerateInfo::OsmSourceType::XML;

  TEST(GenerateIntermediateData(genInfo), ("Can not generate intermediate data for speed cam"));

  // Building empty mwm.
  LocalCountryFile country(my::JoinPath(writableDir, kTestDir), CountryFile(kTestMwm), 0 /* version */);
  string const mwmRelativePath = my::JoinPath(kTestDir, kTestMwm + DATA_FILE_EXTENSION);
  ScopedFile const scopedMwm(mwmRelativePath, ScopedFile::Mode::Create);

  // Step 2. Generate binary file about cameras.
  {
    auto emitter = CreateEmitter(EmitterType::PLANET, genInfo);
    TEST(GenerateFeatures(genInfo, emitter), ("Can not generate features for speed camera"));
  }

  TEST(GenerateFinalFeatures(genInfo, country.GetCountryName(),
                             feature::DataHeader::country), ("Cannot generate final feature"));

  string const & mwmFullPath = scopedMwm.GetFullPath();

  // Creating addition info for building section.
  TEST(BuildOffsetsTable(mwmFullPath), ("Cannot create offsets table"));

  TEST(indexer::BuildIndexFromDataFile(mwmFullPath, testDirFullPath + country.GetCountryName()), ());

  // Step 3. Build section into mwm.
  string const camerasFilename =
    genInfo.GetIntermediateFileName(CAMERAS_TO_WAYS_FILENAME);

  if (!answer.empty())
  {
    // Check, that intermediate file is non empty
    TEST_NOT_EQUAL(my::FileData(camerasFilename, my::FileData::OP_READ).Size(), 0,
                   ("SpeedCam intermediate file is empty"));
  }
  else
  {
    // Check, that intermediate file is empty
    TEST_EQUAL(my::FileData(camerasFilename, my::FileData::OP_READ).Size(), 0,
               ("SpeedCam intermediate file is non empty"));
  }

  string const osmToFeatureFilename =
    genInfo.GetTargetFileName(kTestMwm) + OSM2FEATURE_FILE_EXTENSION;

  BuildCamerasInfo(mwmFullPath, camerasFilename, osmToFeatureFilename);

  CameraMap cameras = LoadSpeedCameraFromMwm(mwmFullPath);
  TEST(CheckCameraMapsEquality(answer, cameras), ("Test answer and parsed cameras are differ!"));
}

UNIT_TEST(SpeedCameraGenerationTest_Empty)
{
  string const osmContent = R"(
 <osm version="0.6" generator="osmconvert 0.8.4" timestamp="2018-07-16T02:00:00Z">
    <node id="10000000" lat="55.7793084" lon="37.3699375" version="1">
    </node>
    <node id="10000001" lat="55.7793085" lon="37.3699399" version="1"></node>
    <way id="2000000" version="1">
      <nd ref="10000000"/>
      <nd ref="10000001"/>
      <tag k="highway" v="primary"/>
    </way>
  </osm>
  )";

  CameraMap answer = {};
  TestSpeedCameraDataBuilding(osmContent, answer);
}

UNIT_TEST(SpeedCameraGenerationTest_CameraIsOnePointOfFeature_1)
{
  string const osmContent = R"(
 <osm version="0.6" generator="osmconvert 0.8.4" timestamp="2018-07-16T02:00:00Z">
    <node id="10000000" lat="55.7793084" lon="37.3699375" version="1">
      <tag k="highway" v="speed_camera"/>
      <tag k="maxspeed" v="100"/>
    </node>
    <node id="10000001" lat="55.7793085" lon="37.3699399" version="1"></node>
    <way id="2000000" version="1">
      <nd ref="10000000"/>
      <nd ref="10000001"/>
      <tag k="highway" v="primary"/>
    </way>
  </osm>
  )";

  // Geometry:
  // Feature number 0: <node-camera>----<node>
  // Result:
  // {(0, 0), (0, 100)} - featureId - 0, segmentId - 0,
  //                      coef - position on segment (at the beginning of segment) - 0,
  //                      maxSpeed - 100.

  CameraMap answer = {
    {SegmentCoord(0, 0), RouteSegment::SpeedCamera(0, 100)}
  };
  TestSpeedCameraDataBuilding(osmContent, answer);
}

UNIT_TEST(SpeedCameraGenerationTest_CameraIsOnePointOfFeature_2)
{
  string const osmContent = R"(
 <osm version="0.6" generator="osmconvert 0.8.4" timestamp="2018-07-16T02:00:00Z">
    <node id="10000001" lat="55.7793000" lon="37.3699000" version="1"></node>
    <node id="10000002" lat="55.7793100" lon="37.3699100" version="1">
      <tag k="highway" v="speed_camera"/>
      <tag k="maxspeed" v="100"/>
    </node>
    <node id="10000003" lat="55.7793200" lon="37.3699200" version="1"></node>
    <way id="2000000" version="1">
      <nd ref="10000001"/>
      <nd ref="10000002"/>
      <nd ref="10000003"/>
      <tag k="highway" v="primary"/>
    </way>
  </osm>
  )";

  // Geometry:
  // Feature number 0: <done>----<node-camera>----<node>
  //                             ^____segmentID = 1____^
  // Result:
  // {(0, 1), (0, 100)} - featureId - 0, segmentId - 1,
  //                      coef - position on segment (at the beginning of segment) - 0,
  //                      maxSpeed - 100.

  CameraMap answer = {
    {SegmentCoord(0, 1), RouteSegment::SpeedCamera(0, 100)}
  };
  TestSpeedCameraDataBuilding(osmContent, answer);
}

UNIT_TEST(SpeedCameraGenerationTest_CameraIsOnePointOfFeature_3)
{
  string const osmContent = R"(
 <osm version="0.6" generator="osmconvert 0.8.4" timestamp="2018-07-16T02:00:00Z">
    <node id="10000001" lat="55.7793000" lon="37.3699000" version="1"></node>
    <node id="10000002" lat="55.7793100" lon="37.3699100" version="1"></node>
    <node id="10000003" lat="55.7793200" lon="37.3699200" version="1">
      <tag k="highway" v="speed_camera"/>
      <tag k="maxspeed" v="100"/>
    </node>
    <way id="2000000" version="1">
      <nd ref="10000001"/>
      <nd ref="10000002"/>
      <nd ref="10000003"/>
      <tag k="highway" v="primary"/>
    </way>
  </osm>
  )";

  // Geometry:
  // Feature number 0: <node>----<node>----<node-camera>
  //                             ^____segmentID = 1____^
  // Result:
  // {(0, 1), (1, 100)} - featureId - 0, segmentId - 1,
  //                      coef - position on segment (at the end of segment) - 1,
  //                      maxSpeed - 100.

  CameraMap answer = {
    {SegmentCoord(0, 1), RouteSegment::SpeedCamera(1, 100)}
  };
  TestSpeedCameraDataBuilding(osmContent, answer);
}

UNIT_TEST(SpeedCameraGenerationTest_CameraIsOnePointOfFeature_4)
{
  string const osmContent = R"(
 <osm version="0.6" generator="osmconvert 0.8.4" timestamp="2018-07-16T02:00:00Z">
    <node id="10000001" lat="55.7793100" lon="37.3699100" version="1"></node>
    <node id="10000002" lat="55.7793200" lon="37.3699200" version="1"></node>
    <node id="10000003" lat="55.7793300" lon="37.3699300" version="1">
      <tag k="highway" v="speed_camera"/>
      <tag k="maxspeed" v="100"/>
    </node>

    <node id="10000004" lat="55.7793400" lon="37.3699400" version="1"></node>
    <node id="10000005" lat="55.7793500" lon="37.3699500" version="1">
      <tag k="highway" v="speed_camera"/>
      <tag k="maxspeed" v="100"/>
    </node>
    <node id="10000006" lat="55.7793600" lon="37.3699600" version="1"></node>

    <way id="2000000" version="1">
      <nd ref="10000001"/>
      <nd ref="10000002"/>
      <nd ref="10000003"/>
      <tag k="highway" v="primary"/>
    </way>

    <way id="2000001" version="1">
      <nd ref="10000004"/>
      <nd ref="10000005"/>
      <nd ref="10000006"/>
      <tag k="highway" v="primary"/>
    </way>
  </osm>
  )";

  // Geometry:
  // Feature number 0: <node>----<node>----<node-camera>
  //                             ^____segmentID = 1____^
  //
  // Feature number 1: <node>----<node-camera>----<node>
  //                             ^____segmentID = 1____^
  // Result:
  // {
  //   (0, 1), (1, 100) - featureId - 0, segmentId - 1,
  //                      coef - position on segment (at the end of segment) - 1,
  //                      maxSpeed - 100.
  //
  //   (1, 1), (0, 100) - featureId - 1, segmentId - 1,
  //                      coef - position on segment (at the beginning of segment) - 0,
  //                      maxSpeed - 100.

  CameraMap answer = {
    {SegmentCoord(0, 1), RouteSegment::SpeedCamera(1, 100)},
    {SegmentCoord(1, 1), RouteSegment::SpeedCamera(0, 100)}
  };
  TestSpeedCameraDataBuilding(osmContent, answer);
}

UNIT_TEST(SpeedCameraGenerationTest_CameraIsNearFeature_1)
{
  string const osmContent = R"(
 <osm version="0.6" generator="osmconvert 0.8.4" timestamp="2018-07-16T02:00:00Z">
    <node id="10000001" lat="55.7793100" lon="37.3699100" version="1"></node>
    <node id="10000002" lat="55.7793200" lon="37.3699200" version="1">
      <tag k="highway" v="speed_camera"/>
      <tag k="maxspeed" v="100"/>
    </node>
    <node id="10000003" lat="55.7793300" lon="37.3699300" version="1"></node>

    <way id="2000000" version="1">
      <nd ref="10000001"/>
      <nd ref="10000003"/>
      <tag k="highway" v="primary"/>
    </way>

  </osm>
  )";

  // Geometry:
  // Feature number 0: <node>--------<node>
  //                             ^___ somewhere here camera, but it is not the point of segment.
  //                                  We add it with coef close to 0.5 because of points' coords (lat, lon).
  //                                  Coef was calculated by this program and checked by eye.
  // Result:
  // {(0, 0), (0.486934, 100)} - featureId - 0, segmentId - 0,
  //                             coef - position on segment (at the half of segment) ~ 0.486934,
  //                             maxSpeed - 100.

  CameraMap answer = {
    {SegmentCoord(0, 0), RouteSegment::SpeedCamera(0.486934529, 100)}
  };
  TestSpeedCameraDataBuilding(osmContent, answer);
}

UNIT_TEST(SpeedCameraGenerationTest_CameraIsNearFeature_2)
{
  string const osmContent = R"(
 <osm version="0.6" generator="osmconvert 0.8.4" timestamp="2018-07-16T02:00:00Z">
    <node id="10000001" lat="55.7793100" lon="37.3699100" version="1"></node>
    <node id="10000002" lat="55.7793150" lon="37.3699150" version="1">
      <tag k="highway" v="speed_camera"/>
      <tag k="maxspeed" v="100"/>
    </node>
    <node id="10000003" lat="55.7793300" lon="37.3699300" version="1"></node>

    <way id="2000000" version="1">
      <nd ref="10000001"/>
      <nd ref="10000003"/>
      <tag k="highway" v="primary"/>
    </way>

  </osm>
  )";

  // Geometry:
  // Feature number 0: <node>--------<node>
  //                           ^___ somewhere here camera, but it is not the point of segment.
  //                                Coef was calculated by this program and checked by eye.
  // Result:
  // {(0, 0), (0.229410536, 100)} - featureId - 0, segmentId - 0,
  //                                coef - position on segment (at the half of segment) ~ 0.2294105,
  //                                maxSpeed - 100.

  CameraMap answer = {
    {SegmentCoord(0, 0), RouteSegment::SpeedCamera(0.229410536, 100)}
  };
  TestSpeedCameraDataBuilding(osmContent, answer);
}
}  // namespace