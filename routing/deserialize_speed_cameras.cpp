#include "routing/deserialize_speed_cameras.hpp"

#include "routing/routing_session.hpp"

#include "generator/camera_info_collector.hpp"

#include "coding/pointd_to_pointu.hpp"
#include "coding/varint.hpp"

#include "base/assert.hpp"
#include "base/macros.hpp"

#include <limits>

namespace routing
{
void DeserializeSpeedCamsFromMwm(ReaderSource<FilesContainerR::TReader> & src,
                                 std::multimap<SegmentCoord, RouteSegment::SpeedCamera> & map)
{
  generator::CamerasInfoCollector::Header header;
  header.Deserialize(src);
  CHECK(header.IsValid(), ("Bad header of speed cam section"));
  uint32_t const amount = header.GetAmount();

  SegmentCoord segment;
  RouteSegment::SpeedCamera speedCamera;
  uint32_t prevFeatureId = 0;
  for (uint32_t i = 0; i < amount; i++)
  {
    auto featureId = ReadVarUint<uint32_t>(src);
    featureId += prevFeatureId;  // delta coding
    prevFeatureId = featureId;

    auto const segmentId = ReadVarUint<uint32_t>(src);

    uint32_t coefInt;
    ReadPrimitiveFromSource(src, coefInt);
    double const coef = Uint32ToDouble(coefInt, 0, 1, 32);

    auto speed = ReadVarUint<uint32_t>(src);
    CHECK_LESS(speed, generator::CamerasInfoCollector::kMaxCameraSpeedKmpH, ());
    if (speed == 0)
      speed = routing::SpeedCameraOnRoute::kNoSpeedInfo;

    // We don't use direction of camera, because of bad data in OSM.
    UNUSED_VALUE(ReadVarUint<uint32_t>(src));  // direction

    // Number of time conditions of camera.
    auto const conditionsNumber = ReadVarUint<uint32_t>(src);
    CHECK_EQUAL(conditionsNumber, 0,
      ("Number of conditions should be 0, non zero number is not implemented now"));

    segment = {featureId, segmentId};
    speedCamera = {coef, static_cast<uint8_t>(speed)};

    map.emplace(segment, speedCamera);
  }
}
}  // namespace routing
