#pragma once

#include "routing/num_mwm_id.hpp"

#include "std/cstdint.hpp"
#include "std/sstream.hpp"
#include "std/string.hpp"

namespace routing
{
// WorldRoadPoint is a unique identifier for any road point in the whole world.
//
// Contains mwm id, feature id and point idx.
// Point idx is the ordinal number of the point in the road.
class WorldRoadPoint final
{
public:
  WorldRoadPoint() = default;

  WorldRoadPoint(NumMwmId mwmId, uint32_t featureId, uint32_t pointId)
    : m_mwmId(mwmId), m_featureId(featureId), m_pointId(pointId)
  {
  }

  NumMwmId GetMwmId() const { return m_mwmId; }

  uint32_t GetFeatureId() const { return m_featureId; }

  uint32_t GetPointId() const { return m_pointId; }

  bool operator==(WorldRoadPoint const & rp) const
  {
    return m_mwmId == rp.m_mwmId && m_featureId == rp.m_featureId && m_pointId == rp.m_pointId;
  }

  bool operator!=(WorldRoadPoint const & rp) const
  {
    return !(*this == rp);
  }

private:
  NumMwmId m_mwmId = kFakeNumMwmId;
  uint32_t m_featureId = 0;
  uint32_t m_pointId = 0;
};

inline string DebugPrint(WorldRoadPoint const & rp)
{
  ostringstream out;
  out << "WorldRoadPoint [" << rp.GetMwmId() << rp.GetFeatureId() << ", " << rp.GetPointId() << "]";
  return out.str();
}
}  // namespace routing
