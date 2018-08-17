#pragma once

#include "generator/osm_element.hpp"
#include "generator/routing_helpers.hpp"

#include "routing/base/followed_polyline.hpp"
#include "routing/routing_helpers.hpp"

#include "indexer/data_source.hpp"
#include "indexer/feature_data.hpp"
#include "indexer/feature_decl.hpp"
#include "indexer/scales.hpp"

#include "coding/file_writer.hpp"
#include "coding/write_to_sink.hpp"

#include "geometry/point2d.hpp"
#include "geometry/segment2d.hpp"

#include "base/assert.hpp"
#include "base/checked_cast.hpp"
#include "base/logging.hpp"
#include "base/string_utils.hpp"

#include <cstdint>
#include <limits>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "boost/optional.hpp"

namespace generator
{
class CamerasInfoCollector
{
private:
  struct Camera;
public:
  // Don't touch the order of enum!
  enum class CameraDirection
  {
    Unknown,
    Forward,
    Backward,
    Both
  };

  class Header
  {
  public:
    void SetVersion(uint32_t v) { m_version = v; }
    void SetAmount(uint32_t n) { m_amount = n; }
    uint32_t GetAmount() const { return m_amount; }

    template <typename T>
    void Serialize(T & sink)
    {
      WriteToSink(sink, m_version);
      WriteToSink(sink, m_amount);
    }

    template <typename T>
    void Deserialize(T & sink)
    {
      ReadPrimitiveFromSource(sink, m_version);
      ReadPrimitiveFromSource(sink, m_amount);
    }

    bool IsValid() const { return m_version == CamerasInfoCollector::kLatestVersion; }

  private:
    uint32_t m_version = 0;
    uint32_t m_amount = 0;
  };

  CamerasInfoCollector(std::string const & dataFilePath, std::string const & camerasInfoPath,
                       std::string const & osmIdsToFeatureIdsPath);

  static uint32_t constexpr kLatestVersion = 1;
  static uint32_t constexpr kMaxCameraSpeedKmpH = std::numeric_limits<uint8_t>::max();

  std::vector<Camera> & GetData() { return m_cameras; }

  static void Serialize(FileWriter & writer, std::vector<Camera> const & cameras);

  bool IsValid() const { return m_valid; }

private:

  struct Camera
  {
    // feature_id and segment_id, where placed camera.
    // m_coef - coef from 0 to 1 where it placed at segment from the begin.
    struct SegmentCoord
    {
      SegmentCoord() = default;
      SegmentCoord(uint32_t fId, uint32_t sId, double k) : m_featureId(fId), m_segmentId(sId), m_coef(k) {}

      uint32_t m_featureId = 0;
      uint32_t m_segmentId = 0;
      double m_coef = 0.0;
    };

    static double constexpr kEqualityEps = 1e-5;

    Camera(m2::PointD const & center, uint8_t const maxSpeed, std::vector<SegmentCoord> && ways)
      : m_center(center), m_maxSpeedKmPH(maxSpeed), m_ways(std::move(ways))
    {
    }

    void ParseDirection()
    {
      // After some research we understood, that camera's direction is a random option.
      // This fact was checked with random-choosed cameras and google maps 3d view.
      // The real direction and the indicated in the .osm differed.
      // So let's this function, space in mwm will be. And implementation won't, until
      // well data appeared in .osm
    }

    // Modify m_ways vector. Set for each way - id of the closest segment.
    void FindClosestSegment(FrozenDataSource const & dataSource, MwmSet::MwmId const & mwmId);

    // Returns empty object, if current feature - |wayId| is not the road, and we should erase it
    // from vector of |m_ways|.
    // Otherwise returns id of segment from feature with id - |wayId|, which starts at camera's
    // center and coefficient - where it placed at the segment.
    boost::optional<std::pair<double, uint32_t>> FindMyself(
      uint32_t wayId, FrozenDataSource const & dataSource, MwmSet::MwmId const & mwmId) const;

    static void Serialize(FileWriter & writer, Camera const & camera, uint32_t prevFeatureId);

    m2::PointD m_center;
    uint8_t m_maxSpeedKmPH;
    std::vector<SegmentCoord> m_ways;
    CameraDirection m_direction = CameraDirection::Unknown;
  };

  static double constexpr kMaxDistFromCameraToClosestSegmentMeters = 20.0;
  static double constexpr kSearchCameraRadiusMeters = 10.0;

  bool ParseIntermediateInfo(std::string const & camerasInfoPath,
                             std::map<osm::Id, uint32_t> const & osmIdToFeatureId);

  bool m_valid = true;
  std::vector<Camera> m_cameras;
  FrozenDataSource m_dataSource;
};
}  // namespace generator

namespace routing
{
// To start build camera info, next data must be ready:
// 1. GenerateIntermediateData(). Cached data about node to ways.
// 2. GenerateFeatures(). Data about cameras from OSM.
// 3. GenerateFinalFeatures(). Calculate some data for features.
// 4. BuildOffsetsTable().
// 5. BuildIndexFromDataFile() - for doing search in rect.
void BuildCamerasInfo(std::string const & dataFilePath, std::string const & camerasInfoPath,
                      std::string const & osmIdsToFeatureIdsPath);
}  // namespace routing
