#pragma once

#include "generator/regions/country_specifier.hpp"

namespace generator
{
namespace regions
{
class RelativeNestingSpecifier : public CountrySpecifier
{
public:
  void AddPlaces(Node::Ptr & tree, RegionsBuilder::PlaceCentersMap const & placeCentersMap) override;

private:
  void AddLocalities(Node::Ptr & tree, RegionsBuilder::PlaceCentersMap const & placeCentersMap);
  void AddSuburbs(Node::Ptr & tree, RegionsBuilder::PlaceCentersMap const & placeCentersMap);
};
}  // namespace regions
}  // namespace generator
