#pragma once

#include "generator/regions/collector_region_info.hpp"
#include "generator/regions/node.hpp"
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

  virtual void AddPlaces(Node::PtrList & outers, RegionsBuilder::PlacePointsMap & placePointsMap)
  { }
  virtual void AdjustRegionsLevel(Node::PtrList & outers, RegionsBuilder::PlacePointsMap & placePointsMap)
  { }

  virtual ObjectLevel GetLevel(RegionPlace const & place) const;
  // Return -1, 0, 1.
  virtual int CompareWeight(LevelPlace const & lhs, LevelPlace const & rhs) const;

protected:
  ObjectLevel GetLevel(PlaceType placeType) const;
  PlaceType GetWeightPlaceType(PlaceType placeType) const;
};
}  // namespace regions
}  // namespace generator
