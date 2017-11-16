#pragma once

#include "routing_common/transit_types.hpp"

#include "base/visitor.hpp"

#include <cstdint>

namespace routing
{
namespace connector
{
// Identifier to find a border edge in neighbouring mwm while cross mwm transition
// for pedestrian, bicycle and car routing.
using OsmId = uint64_t;

// Identifier to find a border edge in neighbouring mwm while cross mwm transition
// for transit routing.
struct TransitId
{
  DECLARE_VISITOR_AND_DEBUG_PRINT(TransitId, visitor(m_stop1Id, "stop1_id"),
                                  visitor(m_stop2Id, "stop2_id"), visitor(m_lineId, "line_id"))

  bool operator==(TransitId const & rhs) const
  {
    return m_stop1Id == rhs.m_stop1Id && m_stop2Id == rhs.m_stop2Id && m_lineId == rhs.m_lineId;
  }

  bool operator!=(TransitId const & rhs) const { return !(rhs == *this); }

  bool operator<(TransitId const & rhs) const
  {
    if (m_stop1Id != rhs.m_stop1Id)
      return m_stop1Id < rhs.m_stop1Id;
    if (m_stop2Id != rhs.m_stop2Id)
      return m_stop2Id < rhs.m_stop2Id;
    return m_lineId < rhs.m_lineId;
  }

  bool operator>(TransitId const & rhs) const { return rhs < *this; }
  bool operator<=(TransitId const & rhs) const { return !(rhs < *this); }
  bool operator>=(TransitId const & rhs) const { return !(*this < rhs); }

  transit::StopId m_stop1Id = transit::kInvalidStopId;
  transit::StopId m_stop2Id = transit::kInvalidStopId;
  transit::LineId m_lineId = transit::kInvalidLineId;
};

struct HashKey
{
  size_t operator()(OsmId const & key) const
  {
    return std::hash<OsmId>()(key);
  }

  size_t operator()(TransitId const & key) const
  {
    return std::hash<uint64_t>()(key.m_stop1Id ^ key.m_stop2Id ^
                                 static_cast<uint64_t>(key.m_lineId));
  }
};
}  // namespace connector
}  // namespace routing
