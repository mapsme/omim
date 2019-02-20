#include "generator/regions/country_specifier.hpp"

namespace generator
{
namespace regions
{
ObjectLevel CountrySpecifier::GetLevel(Region const & region,
                                       boost::optional<PlaceCenter> const & placeLabel) const
{
  if (placeLabel)
    return GetLevel(placeLabel->GetPlaceType());

  auto placeLevel = GetLevel(region.GetPlaceType());
  if (placeLevel != ObjectLevel::Unknown)
    return placeLevel;

  if (region.GetAdminLevel() == AdminLevel::Two)
    return ObjectLevel::Country;

  return ObjectLevel::Unknown;
}

int CountrySpecifier::CompareWeight(RegionPlace const & lhs, RegionPlace const & rhs) const
{
  if (lhs.GetLevel() != ObjectLevel::Unknown && rhs.GetLevel() != ObjectLevel::Unknown)
    return lhs.GetLevel() > rhs.GetLevel() ? -1 : lhs.GetLevel() < rhs.GetLevel();

  auto lhsAdminLevel = lhs.GetRegion().GetAdminLevel();
  auto rhsAdminLevel = rhs.GetRegion().GetAdminLevel();
  if (lhsAdminLevel != AdminLevel::Unknown && rhsAdminLevel != AdminLevel::Unknown)
    return lhsAdminLevel > rhsAdminLevel ? -1 : lhsAdminLevel < rhsAdminLevel;

  auto lhsPlaceType = lhs.GetPlaceType();
  auto rhsPlaceType = rhs.GetPlaceType();
  if (lhsPlaceType != PlaceType::Unknown && rhsPlaceType != PlaceType::Unknown)
    return lhsPlaceType > rhsPlaceType ? -1 : lhsPlaceType < rhsPlaceType;

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

PlaceType CountrySpecifier::GetWeightPlaceType(PlaceType placeType) const
{
  switch (placeType)
  {
  case PlaceType::Region:
  case PlaceType::Province:
    return PlaceType::State;
  case PlaceType::County:
    return PlaceType::District;
  case PlaceType::Town:
  case PlaceType::Village:
  case PlaceType::Hamlet:
    return PlaceType::City;
  case PlaceType::Quarter:
    return PlaceType::Neighbourhood;
  default:
    break;
  }

  return placeType;
}
}  // namespace regions
}  // namespace generator
