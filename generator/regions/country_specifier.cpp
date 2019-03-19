#include "generator/regions/country_specifier.hpp"

namespace generator
{
namespace regions
{
void CountrySpecifier::AddPlaces(Node::PtrList & outers, PlacePointsMap & placePointsMap)
{ }

void CountrySpecifier::AdjustRegionsLevel(Node::PtrList & outers)
{ }

ObjectLevel CountrySpecifier::GetLevel(RegionPlace const & place) const
{
  auto const placeLevel = GetLevel(place.GetPlaceType());
  if (placeLevel != ObjectLevel::Unknown)
    return placeLevel;

  if (place.GetAdminLevel() == AdminLevel::Two)
    return ObjectLevel::Country;

  return ObjectLevel::Unknown;
}

int CountrySpecifier::RelateByWeight(LevelPlace const & lhs, LevelPlace const & rhs) const
{
  if (lhs.GetLevel() != ObjectLevel::Unknown && rhs.GetLevel() != ObjectLevel::Unknown)
  {
    if (lhs.GetLevel() > rhs.GetLevel())
      return -1;
    if (lhs.GetLevel() < rhs.GetLevel())
      return 1;
  }

  auto const lhsPlaceType = lhs.GetPlaceType();
  auto const rhsPlaceType = rhs.GetPlaceType();
  if (lhsPlaceType != PlaceType::Unknown && rhsPlaceType != PlaceType::Unknown)
  {
    if (lhsPlaceType > rhsPlaceType)
      return -1;
    if (lhsPlaceType < rhsPlaceType)
      return 1;
    // Check by admin level (administrative city (district + city) > city).
  }

  auto const lhsAdminLevel = lhs.GetRegion().GetAdminLevel();
  auto const rhsAdminLevel = rhs.GetRegion().GetAdminLevel();
  if (lhsAdminLevel != AdminLevel::Unknown && rhsAdminLevel != AdminLevel::Unknown)
    return lhsAdminLevel > rhsAdminLevel ? -1 : (lhsAdminLevel < rhsAdminLevel ? 1 : 0);
  if (lhsAdminLevel != AdminLevel::Unknown)
    return 1;
  if (rhsAdminLevel != AdminLevel::Unknown)
    return -1;

  return 0;
}

ObjectLevel CountrySpecifier::GetLevel(PlaceType placeType) const
{
  switch (placeType)
  {
  case PlaceType::Country:
    return ObjectLevel::Country;
  case PlaceType::State:
  case PlaceType::Region:
  case PlaceType::Province:
    return ObjectLevel::Region;
  case PlaceType::District:
  case PlaceType::County:
  case PlaceType::Municipality:
    return ObjectLevel::Subregion;
  case PlaceType::City:
  case PlaceType::Town:
  case PlaceType::Village:
  case PlaceType::Hamlet:
    return ObjectLevel::Locality;
  case PlaceType::Suburb:
  case PlaceType::Quarter:
  case PlaceType::Neighbourhood:
    return ObjectLevel::Suburb;
  case PlaceType::Locality:
  case PlaceType::IsolatedDwelling:
    return ObjectLevel::Sublocality;
  case PlaceType::Unknown:
    break;
  }

  return ObjectLevel::Unknown;
}
}  // namespace regions
}  // namespace generator
