#pragma once

#include "coding/file_writer.hpp"

#include "geometry/point2d.hpp"

#include <cstdint>
#include <vector>

namespace routing
{
static uint32_t constexpr kMaxCameraSpeedKmpH = std::numeric_limits<uint8_t>::max();

/// \brief |m_featureId| and |m_segmentId| identify the place where the camera is located.
/// |m_coef| is a factor [0, 1] where the camera is placed at the segment. |m_coef| == 0
/// means the camera is placed at the beginning of the segment.
struct SpeedCameraMwmPosition
{
  SpeedCameraMwmPosition() = default;
  SpeedCameraMwmPosition(uint32_t fId, uint32_t sId, double k) : m_featureId(fId), m_segmentId(sId), m_coef(k) {}

  uint32_t m_featureId = 0;
  uint32_t m_segmentId = 0;
  double m_coef = 0.0;
};

// Don't touch the order of enum (this is part of mwm).
enum class SpeedCameraDirection
{
  Unknown = 0,
  Forward = 1,
  Backward = 2,
  Both = 3
};

struct SpeedCameraMetadata
{
  SpeedCameraMetadata() = default;
  SpeedCameraMetadata(m2::PointD const & center, uint8_t maxSpeed,
                      std::vector<routing::SpeedCameraMwmPosition> && ways)
    : m_center(center), m_maxSpeedKmPH(maxSpeed), m_ways(std::move(ways)) {}

  m2::PointD m_center;
  uint8_t m_maxSpeedKmPH;
  std::vector<routing::SpeedCameraMwmPosition> m_ways;
  SpeedCameraDirection m_direction = SpeedCameraDirection::Unknown;
};

class SpeedCameraMwmHeader
{
public:
  static uint32_t constexpr kLatestVersion = 1;

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

  bool IsValid() const { return m_version == kLatestVersion; }

private:
  uint32_t m_version = 0;
  uint32_t m_amount = 0;
};

void SerializeSpeedCamera(FileWriter & writer, uint32_t prevFeatureId, SpeedCameraMetadata const & data);
}  // namespace routing
