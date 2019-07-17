#include "generator/regions/specs/china.hpp"

#include "generator/regions/country_specifier_builder.hpp"

namespace generator
{
namespace regions
{
namespace specs
{
REGISTER_COUNTRY_SPECIFIER(ChinaSpecifier);

PlaceLevel ChinaSpecifier::GetSpecificCountryLevel(Region const & region) const
{
  AdminLevel adminLevel = region.GetAdminLevel();
  switch (adminLevel)
  {
  case AdminLevel::Two: return PlaceLevel::Country;      // Hong Kong, Macau
  case AdminLevel::Four: return PlaceLevel::Region;      // Provinces
  case AdminLevel::Six: return PlaceLevel::Subregion;    // County
  case AdminLevel::Eight: return PlaceLevel::Subregion;  // Township / Town / Subdistrict
  case AdminLevel::Ten: return PlaceLevel::Locality;     // 	Village
  default: break;
  }

  return PlaceLevel::Unknown;
}
}  // namespace specs
}  // namespace regions
}  // namespace generator
