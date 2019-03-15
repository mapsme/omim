#pragma once

#include "generator/regions/collector_region_info.hpp"
#include "generator/regions/place_point.hpp"
#include "generator/regions/region.hpp"

#include <string>

#include <boost/optional.hpp>

namespace generator
{
namespace regions
{

class RegionPlace
{
public:
  RegionPlace(Region const & region, boost::optional<PlacePoint> && placeLabel = {});

  base::GeoObjectId GetId() const;

  // This function will take the following steps:
  // 1. Return the english name if it exists.
  // 2. Return transliteration if it succeeds.
  // 3. Otherwise, return empty string.
  std::string GetEnglishOrTransliteratedName() const;
  std::string GetName() const;

  PlaceType GetPlaceType() const;
  AdminLevel GetAdminLevel() const;
  BoostPoint GetCenter() const;

  Region const & GetRegion() const noexcept { return m_region; }
  boost::optional<PlacePoint> const & GetPlaceLabel() const noexcept { return m_placeLabel; }

private:
  Region m_region;
  boost::optional<PlacePoint> m_placeLabel;
};
}  // namespace regions
}  // namespace generator
