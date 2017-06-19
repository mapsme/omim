#pragma once

#include "routing/num_mwm_id.hpp"

#include <cstdint>
#include <sstream>
#include <string>

namespace routing
{
enum class SubwayType : uint8_t
{
  Line,
  Change,
};

class SubwayVertex final
{
public:
  SubwayVertex() = default;

  SubwayVertex(NumMwmId mwmId, uint32_t featureId, uint32_t pointId, SubwayType type)
    : m_mwmId(mwmId), m_featureId(featureId), m_pointId(pointId), m_type(type)
  {
  }

  NumMwmId GetMwmId() const { return m_mwmId; }

  uint32_t GetFeatureId() const { return m_featureId; }

  uint32_t GetPointId() const { return m_pointId; }

  SubwayType GetType() const { return m_type; }

  bool operator==(SubwayVertex const & rp) const
  {
    return m_mwmId == rp.m_mwmId && m_featureId == rp.m_featureId && m_pointId == rp.m_pointId &&
           m_type == rp.m_type;
  }

  bool operator!=(SubwayVertex const & rp) const
  {
    return !(*this == rp);
  }

  bool operator<(SubwayVertex const & rp) const
  {
    if (m_mwmId != rp.m_mwmId)
      return m_mwmId < rp.m_mwmId;

    if (m_featureId != rp.m_featureId)
      return m_featureId < rp.m_featureId;

    if (m_pointId != rp.m_pointId)
      return m_pointId < rp.m_pointId;

    return m_type < rp.m_type;
  }

private:
  NumMwmId m_mwmId = kFakeNumMwmId;
  uint32_t m_featureId = 0;
  uint32_t m_pointId = 0;
  SubwayType m_type = SubwayType::Line;
};

inline std::string DebugPrint(SubwayType type)
{
  switch (type)
  {
  case SubwayType::Line: return "line";
  case SubwayType::Change: return "change";
  }
}

inline std::string DebugPrint(SubwayVertex const & rp)
{
  std::ostringstream out;
  out << "SubwayVertex [" << rp.GetMwmId() << rp.GetFeatureId() << ", " << rp.GetPointId() << ", "
      << DebugPrint(rp.GetType()) << "]";
  return out.str();
}
}  // namespace routing
