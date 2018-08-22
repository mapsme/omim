#include "routing/deserialize_speed_cameras.hpp"

#include "routing/routing_session.hpp"
#include "routing/serialize_speed_camera.hpp"

#include "generator/camera_info_collector.hpp"

#include "coding/pointd_to_pointu.hpp"
#include "coding/varint.hpp"

#include "base/assert.hpp"
#include "base/macros.hpp"

#include <limits>
#include <vector>

namespace routing
{
std::vector<RouteSegment::SpeedCamera> kEmptyVectorOfSpeedCameras = {};

void DeserializeSpeedCamsFromMwm(ReaderSource<FilesContainerR::TReader> & src,
                                 std::map<SegmentCoord, vector<RouteSegment::SpeedCamera>> & map)
{
  SpeedCameraMwmHeader header;
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

    uint32_t coefInt = 0;
    ReadPrimitiveFromSource(src, coefInt);
    double const coef = Uint32ToDouble(coefInt, 0, 1, 32);

    uint8_t speed;
    ReadPrimitiveFromSource(src, speed);
    CHECK_LESS(speed, kMaxCameraSpeedKmpH, ());
    if (speed == 0)
      speed = routing::SpeedCameraOnRoute::kNoSpeedInfo;

    // We don't use direction of camera, because of bad data in OSM.
    UNUSED_VALUE(ReadPrimitiveFromSource<uint8_t>(src));  // direction

    // Number of time conditions of camera.
    auto const conditionsNumber = ReadVarUint<uint32_t>(src);
    CHECK_EQUAL(conditionsNumber, 0,
      ("Number of conditions should be 0, non zero number is not implemented now"));

    segment = {featureId, segmentId};
    speedCamera = {coef, static_cast<uint8_t>(speed)};

    auto it = map.find(segment);
    if (it == map.end())
      map.emplace(segment, vector<RouteSegment::SpeedCamera>{speedCamera});
    else
      it->second.emplace_back(speedCamera);
  }
}
}  // namespace routing
