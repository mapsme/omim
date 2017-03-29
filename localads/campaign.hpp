#pragma once

#include "base/assert.hpp"

#include <cstdint>
#include <string>


namespace localads
{
struct Campaign
{
  Campaign(uint32_t featureId,
           uint16_t iconId,
           uint8_t daysBeforeExpired,
           bool priorityBit)
    : m_featureId(featureId)
    , m_iconId(iconId)
    , m_daysBeforeExpired(daysBeforeExpired)
    , m_priorityBit(priorityBit)
  {
  }

  // TODO(mgsergio): Provide a working imlpementation.
  std::string GetIconName() const { return "test-l"; }

  uint32_t m_featureId;
  uint16_t m_iconId;
  uint8_t m_daysBeforeExpired;
  bool m_priorityBit;
};

inline bool operator==(Campaign const & a, Campaign const & b)
{
   return
       a.m_featureId == b.m_featureId &&
       a.m_iconId == b.m_iconId &&
       a.m_daysBeforeExpired == b.m_daysBeforeExpired &&
       a.m_priorityBit == b.m_priorityBit;
}
}  // namespace localads
