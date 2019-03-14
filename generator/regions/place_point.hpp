#pragma once

#include "generator/feature_builder.hpp"
#include "generator/regions/collector_region_info.hpp"
#include "generator/regions/region_base.hpp"

#include <unordered_map>

namespace generator
{
namespace regions
{
class PlacePoint;
using PlacePointsMap = std::unordered_map<base::GeoObjectId, PlacePoint>;

class PlacePoint : public RegionWithName, public RegionWithData
{
public:
  explicit PlacePoint(FeatureBuilder1 const & fb, RegionDataProxy const & rd)
    : RegionWithName(fb.GetParams().name),
      RegionWithData(rd)
  {
    auto const p = fb.GetKeyPoint();
    m_position = {p.x, p.y};
  }

  BoostPoint GetPosition() const {  return m_position; }

private:
  BoostPoint m_position;
};
}  // namespace regions
}  // namespace generator
