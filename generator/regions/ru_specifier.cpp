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
  auto && moscowRegion = FindMoscowRegion(tree);
  if (!moscowRegion)
  {
    LOG(LERROR, ("Failed to find Moscow region"));
    return;
  }

  for (auto & subtree : moscowRegion->GetChildren())
    MarkMoscowSubregionsByAdministrativeOkrugs(subtree);

  auto && moscowCity = FindMoscowCity(moscowRegion);
  if (!moscowCity)
  {
    LOG(LERROR, ("Failed to find Moscow city"));
    return;
  }

  for (auto & subtree : moscowCity->GetChildren())
    MarkMoscowSuburbsByAdministrativeDistrics(subtree);
}

Node::Ptr RuSpecifier::FindMoscowRegion(Node::Ptr const & tree) const
{
  auto const & place = tree->GetData();
  auto const & region = place.GetRegion();
  if (AdminLevel::Four == region.GetAdminLevel() && u8"Москва" == place.GetName())
    return tree;

  if (AdminLevel::Four < region.GetAdminLevel())
    return {};

  for (auto & subtree : tree->GetChildren())
  {
    if (auto moscow = FindMoscowRegion(subtree))
      return moscow;
  }

  return {};
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

void RuSpecifier::MarkMoscowSubregionsByAdministrativeOkrugs(Node::Ptr & tree)
{
  auto & place = tree->GetData();
  auto const & region = place.GetRegion();
  auto const adminLevel = region.GetAdminLevel();
  if (AdminLevel::Five == adminLevel)
  {
    place.SetLevel(ObjectLevel::Subregion);
    return;
  }

  if (AdminLevel::Five < adminLevel)
    return;
  
  for (auto & subtree : tree->GetChildren())
    MarkMoscowSubregionsByAdministrativeOkrugs(subtree);
}

void RuSpecifier::MarkMoscowSuburbsByAdministrativeDistrics(Node::Ptr & tree)
{
  auto & place = tree->GetData();
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

ObjectLevel RuSpecifier::GetLevel(RegionPlace const & place) const
{
  auto placeLevel = CountrySpecifier::GetLevel(place.GetPlaceType());
  if (placeLevel != ObjectLevel::Unknown)
    return placeLevel;

  return GetRuAdminObjectLevel(place.GetAdminLevel());
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
