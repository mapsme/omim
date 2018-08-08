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

#include "base/checked_cast.hpp"
#include "base/logging.hpp"
#include "base/string_utils.hpp"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "boost/optional.hpp"

namespace generator
{
class CamerasInfoCollector
{
public:
  CamerasInfoCollector(std::string const & dataFilePath, std::string const & camerasInfoPath,
                       std::string const & osmIdsToFeatureIdsPath);

  enum class CameraDirection
  {
    Unknown,
    Forward,
    Backward,
    Both
  };

  struct Camera
  {

    // feature_id and segment_id, where placed camera.
    // k - coef from 0 to 1 where it placed at segment.
    // uint64_t - because we store osm::Id::Way here at first.
    struct SegmentCoord
    {
      SegmentCoord() = default;
      SegmentCoord(uint64_t fId, uint32_t sId, double k) : m_featureId(fId), m_segmentId(sId), k(k)
      {
      }

      uint64_t m_featureId = 0;
      uint32_t m_segmentId = 0;
      double k = 0.0;
    };

    static double constexpr kEqualityEps = 1e-5;
    static double constexpr kSignificantPartOfSegmentCoef = 1e9;

    Camera(m2::PointD & center, uint8_t maxSpeed, std::vector<SegmentCoord> && ways)
      : m_center(center), m_maxSpeed(maxSpeed), m_ways(std::move(ways))
    {}

    m2::PointD m_center;
    uint8_t m_maxSpeed;
    std::vector<SegmentCoord> m_ways;
    CameraDirection m_direction = CameraDirection::Unknown;

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

    // Returns pair:
    // 1. id of segment, which starts at camera's center.
    // 2. bool - false if current feature is not the road, and we should erase it from vector of ways.
    boost::optional<uint32_t> FindMyself(uint32_t wayId, FrozenDataSource const & dataSource,
                                         MwmSet::MwmId const & mwmId);

    void TranslateWaysIdFromOsmId(std::map<osm::Id, uint32_t> const & osmIdToFeatureId);

    static void Serialize(FileWriter & writer, Camera const & camera);
  };

  std::vector<Camera> const & GetData() const { return m_cameras; }

  static void Serialize(FileWriter & writer, std::vector<Camera> const & cameras);

  bool IsValid() const { return m_valid; }

private:
  static uint32_t constexpr kLatestVersion = 1;
  static double constexpr kMaxDistFromCameraToClosestSegmentMeters = 20.0;
  static double constexpr kSearchCameraRadiusMeters = 10.0;
  static uint32_t constexpr kMaxCameraSpeedKmpH = 255;

  bool ParseIntermediateInfo(string const & camerasInfoPath);

  bool m_valid = true;
  std::vector<Camera> m_cameras;
  FrozenDataSource m_dataSource;
};
}  // namespace generator

namespace routing
{
void BuildCamerasInfo(std::string const & dataFilePath, std::string const & camerasInfoPath,
                      std::string const & osmIdsToFeatureIdsPath);
}  // namespace routing
