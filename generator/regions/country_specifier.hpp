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

  virtual void AddPlaces(Node::Ptr & tree, RegionsBuilder::PlaceCentersMap const & placeCentersMap)
  { }
  virtual void AdjustRegionsLevel(Node::Ptr & tree, RegionsBuilder::PlaceCentersMap const & placeCentersMap)
  { }

  virtual ObjectLevel GetLevel(Region const & region, boost::optional<PlaceCenter> const & placeLabel) const;
  // Return -1, 0, 1.
  virtual int CompareWeight(RegionPlace const & lhs, RegionPlace const & rhs) const;

protected:
  ObjectLevel GetLevel(PlaceType placeType) const;
  PlaceType GetWeightPlaceType(PlaceType placeType) const;
};
}  // namespace regions
}  // namespace generator
