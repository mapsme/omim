#pragma once

#include "generator/regions/relative_nesting_specifier.hpp"

namespace generator
{
namespace regions
{
class RuSpecifier final : public RelativeNestingSpecifier
{
public:
  // CountrySpecifier overrides:
  void AdjustRegionsLevel(Node::PtrList & outers) override;
  ObjectLevel GetLevel(RegionPlace const & place) const override;

private:
  void AdjustMoscowAdministrativeDivisions(Node::PtrList & outers);
  void AdjustMoscowAdministrativeDivisions(Node::Ptr const & tree);
  void MarkMoscowSubregionsByAdministrativeOkrugs(Node::Ptr & node);

  void AdjustMoscowCitySuburbs(Node::Ptr const & tree);
  void MarkMoscowSuburbsByAdministrativeDistrics(Node::Ptr & tree);
  void MarkMoscowAdministrativeDistric(Node::Ptr & node);
  void MarkAllSuburbsToSublocalities(Node::Ptr & tree);

  ObjectLevel GetRuAdminObjectLevel(AdminLevel adminLevel) const;

  bool m_moscowRegionWasProcessed{false};
  bool m_moscowCityWasProcessed{false};
};
}  // namespace regions
}  // namespace generator
