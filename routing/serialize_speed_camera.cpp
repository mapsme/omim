#include "routing/serialize_speed_camera.hpp"

#include "coding/pointd_to_pointu.hpp"
#include "coding/varint.hpp"
#include "coding/write_to_sink.hpp"

#include "base/assert.hpp"

namespace routing
{
uint32_t constexpr SpeedCameraMwmHeader::kLatestVersion;

void SerializeSpeedCamera(FileWriter & writer, uint32_t prevFeatureId, const routing::SpeedCameraMetadata & data)
{
  CHECK_EQUAL(data.m_ways.size(), 1, ());

  auto const & way = data.m_ways.back();

  auto featureId = static_cast<uint32_t>(way.m_featureId);
  CHECK_GREATER_OR_EQUAL(featureId, prevFeatureId, ());
  featureId -= prevFeatureId;  // delta coding
  WriteVarUint(writer, featureId);
  WriteVarUint(writer, way.m_segmentId);

  uint32_t const coef = DoubleToUint32(way.m_coef, 0, 1, 32);
  WriteToSink(writer, coef);

  WriteToSink(writer, data.m_maxSpeedKmPH);

  auto const direction = static_cast<uint8_t>(data.m_direction);
  WriteToSink(writer, direction);

  // TODO (@gmoryes) add implementation of this feature
  // List of time conditions will be saved here. For example:
  //    "maxspeed:conditional": "60 @ (23:00-05:00)"
  //                       or
  //    "60 @ (Su 00:00-24:00; Mo-Sa 00:00-06:00,22:30-24:00)"
  // We store number of conditions first, then each condition in 3 bytes:
  //    1. Type of condition (day, hour, month) + start value of range.
  //    2. End value of range.
  //    3. Maxspeed in this time range.
  WriteVarInt(writer, 0 /* number of time condition */);
}
}  // namespace routing

