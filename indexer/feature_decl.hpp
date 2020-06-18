#pragma once

#include "indexer/features_tag.hpp"
#include "indexer/mwm_set.hpp"

#include <cstdint>
#include <string>
#include <tuple>

namespace feature
{
enum class GeomType : int8_t
{
  Undefined = -1,
  Point = 0,
  Line = 1,
  Area = 2
};

std::string DebugPrint(GeomType type);
}  // namespace feature

struct FeatureID
{
  static char const * const kInvalidFileName;
  static int64_t const kInvalidMwmVersion;

  FeatureID() = default;

  FeatureID(MwmSet::MwmId const & mwmId, FeaturesTag tag, uint32_t index)
    : m_mwmId(mwmId), m_tag(tag), m_index(index)
  {
  }

  FeatureID(MwmSet::MwmId const & mwmId, uint32_t index)
    : FeatureID(mwmId, FeaturesTag::Common, index)
  {
  }

  bool IsValid() const { return m_mwmId.IsAlive(); }

  bool operator<(FeatureID const & r) const
  {
    return std::tie(m_mwmId, m_tag, m_index) < std::tie(r.m_mwmId, r.m_tag, r.m_index);
  }

  bool operator==(FeatureID const & r) const
  {
    return m_mwmId == r.m_mwmId && m_tag == r.m_tag && m_index == r.m_index;
  }

  bool operator!=(FeatureID const & r) const { return !(*this == r); }

  std::string GetMwmName() const;
  int64_t GetMwmVersion() const;

  MwmSet::MwmId m_mwmId;
  FeaturesTag m_tag = FeaturesTag::Common;
  uint32_t m_index = 0;
};

std::string DebugPrint(FeatureID const & id);
