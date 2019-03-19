#pragma once

#include "generator/regions/collector_region_info.hpp"
#include "generator/regions/node.hpp"
#include "generator/regions/place_point.hpp"
#include "generator/regions/region.hpp"
#include "generator/regions/regions_builder.hpp"

#include <boost/optional.hpp>

namespace generator
{
namespace regions
{
class CountrySpecifier
{
public:
  virtual ~CountrySpecifier() = default;

  virtual void AddPlaces(Node::PtrList & outers, PlacePointsMap & placePointsMap);
  virtual void AdjustRegionsLevel(Node::PtrList & outers);

  virtual ObjectLevel GetLevel(RegionPlace const & place) const;
  // Return -1 - |lhs| is under place of |rhs|, 0 - undefined relation, 1 - |rhs| is under place of |lhs|.
  // Non-transitive.
  virtual int RelateByWeight(LevelPlace const & lhs, LevelPlace const & rhs) const;

protected:
  ObjectLevel GetLevel(PlaceType placeType) const;
};
}  // namespace regions
}  // namespace generator
