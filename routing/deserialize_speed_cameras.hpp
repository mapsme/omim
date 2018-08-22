#pragma once

#include "routing/route.hpp"
#include "routing/segment.hpp"

#include "coding/reader.hpp"

#include <cstdint>
#include <map>
#include <utility>

namespace routing
{
// Pair of featureId and segmentId
struct SegmentCoord {
  SegmentCoord() = default;
  SegmentCoord(uint32_t fId, uint32_t sId) : m_featureId(fId), m_segmentId(sId) {}

  friend bool operator<(SegmentCoord const & lhs, SegmentCoord const & rhs)
  {
    return lhs.m_featureId < rhs.m_featureId && lhs.m_segmentId < rhs.m_featureId;
  }

  uint32_t m_featureId;
  uint32_t m_segmentId;
};

extern std::vector<RouteSegment::SpeedCamera> kEmptyVectorOfSpeedCameras;

void DeserializeSpeedCamsFromMwm(
    ReaderSource<FilesContainerR::TReader> & src,
    std::map<SegmentCoord, std::vector<RouteSegment::SpeedCamera>> & camerasVector);
}  // namespace routing
