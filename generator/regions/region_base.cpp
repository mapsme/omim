#include "generator/regions/region_base.hpp"

#include "base/assert.hpp"
#include "base/control_flow.hpp"

namespace generator
{
namespace regions
{
std::string RegionWithName::GetName(int8_t lang) const
{
  std::string s;
  VERIFY(m_name.GetString(lang, s) != s.empty(), ());
  return s;
}

StringUtf8Multilang const & RegionWithName::GetMultilangName() const
{
  return m_name;
}

void RegionWithName::SetMultilangName(StringUtf8Multilang const & name)
{
  m_name = name;
}

base::GeoObjectId RegionWithData::GetId() const
{
  return m_regionData.GetOsmId();
}

bool RegionWithData::HasIsoCode() const
{
  return m_regionData.HasIsoCodeAlpha2();
}

std::string RegionWithData::GetIsoCode() const
{
  return m_regionData.GetIsoCodeAlpha2();
}

boost::optional<base::GeoObjectId> RegionWithData::GetLabelOsmIdOptional() const
{
  return m_regionData.GetLabelOsmIdOptional();
}
}  // namespace regions
}  // namespace generator
