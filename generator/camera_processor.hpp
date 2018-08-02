#pragma once

#include "base/logging.hpp"

#include "coding/file_writer.hpp"
#include "coding/write_to_sink.hpp"

#include "generator/osm_element.hpp"
#include "generator/routing_helpers.hpp"

#include "geometry/point2d.hpp"
#include "geometry/segment2d.hpp"

#include "indexer/data_source.hpp"
#include "indexer/feature_data.hpp"
#include "indexer/feature_decl.hpp"
#include "indexer/scales.hpp"

#include "routing/base/followed_polyline.hpp"
#include "routing/routing_helpers.hpp"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>


namespace {
enum class CameraDirection
{
  UNKNOWN,
  FORWARD,
  BACKWARD,
  BOTH
};

constexpr double kEqualityEps = 1e-5;
constexpr double kMaxDistFromCameraToClosestSegment = 20; // meters
constexpr double kSearchCameraRadius = 10;
}

class CameraNodeProcessor
{
public:

  explicit CameraNodeProcessor(std::string const & filePath)
    : m_fileWriter(filePath, FileWriter::OP_WRITE_TRUNCATE)
  {
    LOG(LINFO, ("Saving information about cameras and ways, where they lie on, to: ", filePath));
  }

  template<typename Cache>
  void ProcessAndWrite(OsmElement & p, FeatureParams const & params, Cache cache)
  {
    std::string maxSpeedString = params.GetMetadata().Get(feature::Metadata::EType::FMD_MAXSPEED);
    if (maxSpeedString.empty())
      maxSpeedString = "0";

    uint32_t maxSpeed;
    {
      std::stringstream ss;
      ss << maxSpeedString;
      ss >> maxSpeed;
    }

    m_fileWriter.Write(reinterpret_cast<char * >(&p.lat), sizeof(p.lat));
    m_fileWriter.Write(reinterpret_cast<char * >(&p.lon), sizeof(p.lat));
    m_fileWriter.Write(reinterpret_cast<char * >(&maxSpeed), sizeof(maxSpeed));

    std::vector<uint64_t> ways;
    cache.ForEachWayByNode(p.id, [&ways](uint64_t wayId)
    {
      ways.push_back(wayId);
      return false;
    });

    auto size = static_cast<uint32_t>(ways.size());
    m_fileWriter.Write(reinterpret_cast<char * >(&size), sizeof(size));
    for (auto wayId : ways)
      m_fileWriter.Write(reinterpret_cast<char * >(&wayId), sizeof(wayId));
  }

private:

  FileWriter m_fileWriter;
};

class CamerasInfoCollector
{
public:
  CamerasInfoCollector(std::string const & dataFilePath, std::string const & camerasInfoPath,
                       std::string const & osmIdsToFeatureIdsPath);

  bool IsValid() const { return m_valid; }

  struct Camera
  {
    // pair of feature_id and segment_id, where placed camera.
    // uint64_t - because we store osm::Id::Way here at first.
    using SegmentCoord = std::pair<uint64_t, uint32_t>;

    Camera(m2::PointD & center, uint8_t maxSpeed, std::vector<SegmentCoord> & ways)
      : m_center(center), m_maxSpeed(maxSpeed), m_ways(std::move(ways)), m_direction(CameraDirection::UNKNOWN)
    {}

    m2::PointD m_center;
    uint8_t m_maxSpeed;
    std::vector<SegmentCoord> m_ways;
    CameraDirection m_direction;


    void parseDirection()
    {
      // After some research we understood, that camera's direction is a random option.
      // This fact was checked with random-choosed cameras and google maps 3d view.
      // The real direction and the indicated in the .osm differed.
      // So let's this function, space in mwm will be. And implementation won't, until
      // well data appeared in .osm
    }

    // Modify m_ways vector. Set for each way - id of the closest segment.
    void findClosestSegment(FrozenDataSource const & dataSource);

    // Returns pair:
    // 1. id of segment, which starts at camera's center.
    // 2. bool - false if current feature is not the road, and we should erase it from vector of ways.
    std::pair<uint32_t, bool> findMySelf(uint32_t wayId, FrozenDataSource const & dataSource);

    void translateWaysIdFromOsmId(std::map<osm::Id, uint32_t> const & osmIdToFeatureId);

    static void Serialize(FileWriter & writer, Camera const & camera)
    {
      auto waysNumber = static_cast<uint8_t>(camera.m_ways.size());
      WriteToSink(writer, waysNumber);

      for (auto const & way : camera.m_ways)
      {
        auto featureId = static_cast<uint32_t>(way.first);
        WriteToSink(writer, featureId); // featureId
        WriteToSink(writer, way.second); //segmentId
      }

      auto speed = static_cast<uint8_t>(camera.m_maxSpeed);
      WriteToSink(writer, speed);

      auto direction = static_cast<uint8_t>(camera.m_direction);
      WriteToSink(writer, direction);

      // TODO add implementation of this feature
      WriteToSink(writer, 0); // Time, from which camera is on
      WriteToSink(writer, 0); // Time, from which camera is off
    }
  };

  std::vector<Camera> const & Data() { return m_cameras; }

  static void Serialize(FileWriter & writer, std::vector<Camera> const & m_cameras)
  {
    WriteToSink(writer, kLatestVersion);

    auto amount = static_cast<uint32_t>(m_cameras.size());
    WriteToSink(writer, amount);

    for (auto const & camera : m_cameras)
      Camera::Serialize(writer, camera);
  }

private:

  bool ParseIntermediateInfo(string const & camerasInfoPath);

  template<typename T>
  void Read(FileReader & fileReader, uint64_t & pos, T * pointer)
  {
    fileReader.Read(pos, reinterpret_cast<void *>(pointer), sizeof(T));
    pos += sizeof(T);
  }

  bool m_valid;
  std::vector<Camera> m_cameras;
  FrozenDataSource m_dataSource;

  static MwmSet::MwmId m_mwmId;
  static uint32_t const kLatestVersion;
};

namespace routing {

void BuildCamerasInfo(std::string const & dataFilePath, std::string const & camerasInfoPath,
                      std::string const & osmIdsToFeatureIdsPath);
} // namespace routing