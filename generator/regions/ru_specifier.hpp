#pragma once

#include "generator/regions/relative_nesting_specifier.hpp"

namespace generator
{
namespace regions
{
class RuSpecifier : public RelativeNestingSpecifier
{
public:
  // CountrySpecifier overrides:
  void AdjustRegionsLevel(Node::Ptr & tree, RegionsBuilder::PlaceCentersMap const & placeCentersMap) override;
  ObjectLevel GetLevel(RegionPlace const & place) const override;

private:
  void AdjustMoscowAdministrativeDivisions(Node::Ptr & tree);
  Node::Ptr FindMoscowRegion(Node::Ptr const & tree) const;
  Node::Ptr FindMoscowCity(Node::Ptr const & tree) const;

  void MarkMoscowSubregionsByAdministrativeOkrugs(Node::Ptr & node);

  void MarkMoscowSuburbsByAdministrativeDistrics(Node::Ptr & tree);
  void MarkMoscowAdministrativeDistric(Node::Ptr & node);
  void MarkAllSuburbsToSublocalities(Node::Ptr & tree);

  ObjectLevel GetRuAdminObjectLevel(AdminLevel adminLevel) const;
};
}  // namespace regions
}  // namespace generator
