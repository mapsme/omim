#pragma once

#include "generator/regions/collector_region_info.hpp"
#include "generator/regions/place_center.hpp"
#include "generator/regions/region.hpp"

#include <boost/optional.hpp>

namespace generator
{
namespace regions
{

class RegionPlace
{
public:
  RegionPlace(ObjectLevel level, Region const & region, boost::optional<PlaceCenter> placeLabel = {});

  ObjectLevel GetLevel() const noexcept { return m_level; }
  void SetLevel(ObjectLevel level);

  base::GeoObjectId GetId() const;
  std::string GetName() const;
  std::string GetEnglishOrTransliteratedName() const;
  PlaceType GetPlaceType() const;
  BoostPoint GetCenter() const;

  Region const & GetRegion() const noexcept { return m_region; }
  boost::optional<PlaceCenter> const & GetPlaceLabel() const noexcept { return m_placeLabel; }

private:
  ObjectLevel m_level;
  Region m_region;
  boost::optional<PlaceCenter> m_placeLabel;
};
}  // namespace regions
}  // namespace generator
