#include "generator/regions/region_place.hpp"

namespace generator
{
namespace regions
{
RegionPlace::RegionPlace(Region const & region, boost::optional<PlaceCenter> placeLabel)
  : m_region{region}, m_placeLabel{placeLabel}
{ }

base::GeoObjectId RegionPlace::GetId() const
{
  if (m_placeLabel)
    return m_placeLabel->GetId();

  return m_region.GetId();
}

std::string RegionPlace::GetName() const
{
  if (m_placeLabel)
    return m_placeLabel->GetName();

  return m_region.GetName();
}
  
std::string RegionPlace::GetEnglishOrTransliteratedName() const
{
  if (m_placeLabel)
  {
    auto name = m_placeLabel->GetEnglishOrTransliteratedName();
    if (!name.empty())
      return name;
  }

  return m_region.GetEnglishOrTransliteratedName();
}

PlaceType RegionPlace::GetPlaceType() const
{
  if (m_placeLabel)
    return m_placeLabel->GetPlaceType();

  return m_region.GetPlaceType();
}

AdminLevel RegionPlace::GetAdminLevel() const
{
  return m_region.GetAdminLevel();
}

BoostPoint RegionPlace::GetCenter() const
{
  if (m_placeLabel)
    return m_placeLabel->GetCenter();

  return m_region.GetCenter();
}
}  // namespace regions
}  // namespace generator
