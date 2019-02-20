#pragma once

#include "generator/feature_builder.hpp"
#include "generator/regions/collector_region_info.hpp"
#include "generator/regions/region_base.hpp"

#include <unordered_map>

namespace generator
{
namespace regions
{
class PlaceCenter : public RegionWithName, public RegionWithData
{
public:
  explicit PlaceCenter(FeatureBuilder1 const & fb, RegionDataProxy const & rd)
    : RegionWithName(fb.GetParams().name),
      RegionWithData(rd)
  {
    auto const p = fb.GetKeyPoint();
    m_center = {p.x, p.y};
  }

  BoostPoint GetCenter() const {  return m_center; }

private:
  BoostPoint m_center;
};

//using PlaceCentersMap = std::unordered_map<base::GeoObjectId, PlaceCenter>;
}  // namespace regions
}  // namespace generator
