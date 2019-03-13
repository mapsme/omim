#pragma once

#include "generator/regions/country_specifier.hpp"

namespace generator
{
namespace regions
{
class RelativeNestingSpecifier : public CountrySpecifier
{
public:
  // CountrySpecifier overrides:
  void AddPlaces(Node::PtrList & outers, RegionsBuilder::PlacePointsMap & placePointsMap) override;

private:
  void AddLocalities(Node::PtrList & outers, RegionsBuilder::PlacePointsMap & placePointsMap);
  bool AddPlacePoint(Node::PtrList & outers, PlacePoint const & placePoint);
};
}  // namespace regions
}  // namespace generator
