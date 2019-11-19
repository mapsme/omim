#pragma once

#include <cstdint>
#include <functional>
#include <sstream>
#include <string>

namespace routing
{
// RoadPoint is a unique identifier for any road point in mwm file.
//
// Contains feature id and point id.
// Point id is the ordinal number of the point in the road.
class RoadPoint final
{
public:
  RoadPoint() : m_featureId(0), m_pointId(0) {}

  RoadPoint(uint32_t featureId, uint32_t pointId) : m_featureId(featureId), m_pointId(pointId) {}

  uint32_t GetFeatureId() const { return m_featureId; }

  uint32_t GetPointId() const { return m_pointId; }

  bool operator<(RoadPoint const & rp) const
  {
    if (m_featureId != rp.m_featureId)
      return m_featureId < rp.m_featureId;
    return m_pointId < rp.m_pointId;
  }

  bool operator==(RoadPoint const & rp) const
  {
    return m_featureId == rp.m_featureId && m_pointId == rp.m_pointId;
  }

  bool operator!=(RoadPoint const & rp) const
  {
    return !(*this == rp);
  }

  struct Hash
  {
    uint64_t operator()(RoadPoint const & roadPoint) const
    {
      uint32_t featureId = roadPoint.m_featureId;
      uint32_t pointId = roadPoint.m_pointId;

      return std::hash<uint64_t>()(
        (static_cast<uint64_t>(featureId) << 32) + static_cast<uint64_t>(pointId));
    }
  };

  void SetPointId(uint32_t pointId) { m_pointId = pointId; }

private:
  uint32_t m_featureId;
  uint32_t m_pointId;
};

inline std::string DebugPrint(RoadPoint const & rp)
{
  std::ostringstream out;
  out << "RoadPoint [" << rp.GetFeatureId() << ", " << rp.GetPointId() << "]";
  return out.str();
}
}  // namespace routing
