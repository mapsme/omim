#include "generator/regions/ru_specifier.hpp"

#include "base/stl_helpers.hpp"

namespace generator
{
namespace regions
{
void RuSpecifier::AdjustRegionsLevel(Node::PtrList & outers)
{
  RelativeNestingSpecifier::AdjustRegionsLevel(outers);
  AdjustMoscowAdministrativeDivisions(outers);
}

void RuSpecifier::AdjustMoscowAdministrativeDivisions(Node::PtrList & outers)
{
  for (auto const & tree : outers)
    AdjustMoscowAdministrativeDivisions(tree);

  if (!m_moscowRegionWasProcessed)
    LOG(LWARNING, ("Failed to find Moscow region"));
  else if (!m_moscowCityWasProcessed)
    LOG(LWARNING, ("Failed to find Moscow city"));
}

void RuSpecifier::AdjustMoscowAdministrativeDivisions(Node::Ptr const & tree)
{
  auto const & place = tree->GetData();
  auto const & region = place.GetRegion();
  if (AdminLevel::Four == region.GetAdminLevel() && u8"Москва" == place.GetName())
  {
    for (auto & subtree : tree->GetChildren())
    {
      MarkMoscowSubregionsByAdministrativeOkrugs(subtree);
      AdjustMoscowCitySuburbs(subtree);
    }
    return;
  }

  if (AdminLevel::Four < region.GetAdminLevel())
    return;

  for (auto & subtree : tree->GetChildren())
    AdjustMoscowAdministrativeDivisions(subtree);
}

void RuSpecifier::MarkMoscowSubregionsByAdministrativeOkrugs(Node::Ptr & tree)
{
  auto & place = tree->GetData();
  auto const & region = place.GetRegion();
  auto const adminLevel = region.GetAdminLevel();
  if (AdminLevel::Five == adminLevel)
  {
    place.SetLevel(ObjectLevel::Subregion);
    m_moscowRegionWasProcessed = true;
    return;
  }

  if (AdminLevel::Five < adminLevel)
    return;
  
  for (auto & subtree : tree->GetChildren())
    MarkMoscowSubregionsByAdministrativeOkrugs(subtree);
}

void RuSpecifier::AdjustMoscowCitySuburbs(Node::Ptr const & tree)
{
  auto & place = tree->GetData();
  if (PlaceType::City == place.GetPlaceType() && u8"Москва" == place.GetName())
  {
    for (auto & subtree : tree->GetChildren())
      MarkMoscowSuburbsByAdministrativeDistrics(subtree);
    return;
  }

  if (ObjectLevel::Locality <= place.GetLevel())
    return;

  for (auto & subtree : tree->GetChildren())
    AdjustMoscowCitySuburbs(subtree);
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
  m_moscowCityWasProcessed = true;

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
