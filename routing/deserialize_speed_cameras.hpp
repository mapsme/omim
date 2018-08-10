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
using SegmentCoord = std::pair<uint32_t, uint32_t>;

void DeserializeSpeedCamsFromMwm(
    ReaderSource<FilesContainerR::TReader> & src,
    std::multimap<std::pair<uint32_t, uint32_t>, RouteSegment::SpeedCamera> & map);
}  // namespace routing
