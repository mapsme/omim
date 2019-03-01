#include "generator/regions/ru_specifier.hpp"

#include "base/stl_helpers.hpp"

namespace generator
{
namespace regions
{
void RuSpecifier::AdjustRegionsLevel(Node::Ptr & tree, RegionsBuilder::PlaceCentersMap const & placeCentersMap)
{
  RelativeNestingSpecifier::AdjustRegionsLevel(tree, placeCentersMap);
  AdjustMoscowAdministrativeDivisions(tree);
}

void RuSpecifier::AdjustMoscowAdministrativeDivisions(Node::Ptr & tree)
{
  auto && moscowCity = FindMoscowCity(tree);
  if (!moscowCity)
    return;

  for (auto & subtree : moscowCity->GetChildren())
  {
    MarkMoscowSubregionsByAdministrativeOkrugs(subtree);
    MarkMoscowSuburbsByAdministrativeDistrics(subtree);
  }
}

Node::Ptr RuSpecifier::FindMoscowCity(Node::Ptr const & tree) const
{
  auto & place = tree->GetData();
  if (PlaceType::City == place.GetPlaceType() && u8"Москва" == place.GetName())
    return tree;

  if (ObjectLevel::Locality <= place.GetLevel())
    return {};

  for (auto & subtree : tree->GetChildren())
  {
    if (auto moscow = FindMoscowCity(subtree))
      return moscow;
  }

  return {};
}

void RuSpecifier::MarkMoscowSubregionsByAdministrativeOkrugs(Node::Ptr & node)
{
  auto & place = node->GetData();
  auto const & region = place.GetRegion();
  if (AdminLevel::Five != region.GetAdminLevel())
    return;

  place.SetLevel(ObjectLevel::Subregion);
}

void RuSpecifier::MarkMoscowSuburbsByAdministrativeDistrics(Node::Ptr & tree)
{
  auto & place = tree->GetData();
  if (ObjectLevel::Locality == place.GetLevel()) // nested locality
    return;

  auto & region = place.GetRegion();
  if (AdminLevel::Eight == region.GetAdminLevel())
  {
    MarkMoscowAdministrativeDistric(tree);
    return;
  }

  if (ObjectLevel::Suburb == place.GetLevel())
    place.SetLevel(ObjectLevel::Sublocality);
  
  for (auto & subtree : tree->GetChildren())
    MarkMoscowSuburbsByAdministrativeDistrics(subtree);
}

void RuSpecifier::MarkMoscowAdministrativeDistric(Node::Ptr & node)
{
  auto & place = node->GetData();
  place.SetLevel(ObjectLevel::Suburb);

  for (auto & subtree : node->GetChildren())
    MarkAllSuburbsToSublocalities(subtree);
}

void RuSpecifier::MarkAllSuburbsToSublocalities(Node::Ptr & tree)
{
  auto & place = tree->GetData();
  auto const level = place.GetLevel();
  if (ObjectLevel::Locality == level)  // nested locality
    return;

  if (level == ObjectLevel::Suburb)
    place.SetLevel(ObjectLevel::Sublocality);
  
  for (auto & subtree : tree->GetChildren())
    MarkAllSuburbsToSublocalities(subtree);
}

ObjectLevel RuSpecifier::GetLevel(Region const & region, boost::optional<PlaceCenter> const & placeLabel) const
{
  if (placeLabel)
    return CountrySpecifier::GetLevel(placeLabel->GetPlaceType());

  auto placeLevel = CountrySpecifier::GetLevel(region.GetPlaceType());
  if (placeLevel != ObjectLevel::Unknown)
    return placeLevel;

  return GetRuAdminObjectLevel(region.GetAdminLevel());
}

ObjectLevel RuSpecifier::GetRuAdminObjectLevel(AdminLevel adminLevel) const
{
  switch (adminLevel)
  {
  case AdminLevel::Two:
    return ObjectLevel::Country;
  case AdminLevel::Four:
    return ObjectLevel::Region;
  case AdminLevel::Six:
    return ObjectLevel::Subregion;
  default:
    break;
  }

  return ObjectLevel::Unknown;
}
}  // namespace regions
}  // namespace generator
