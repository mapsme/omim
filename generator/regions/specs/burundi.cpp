#include "generator/regions/specs/burundi.hpp"

#include "generator/regions/country_specifier_builder.hpp"

namespace generator
{
namespace regions
{
namespace specs
{
REGISTER_COUNTRY_SPECIFIER(BurundiSpecifier);

PlaceLevel BurundiSpecifier::GetSpecificCountryLevel(Region const & region) const
{
  AdminLevel adminLevel = region.GetAdminLevel();
  switch (adminLevel)
  {
  case AdminLevel::Four: return PlaceLevel::Region;      //  Provinces
  case AdminLevel::Five: return PlaceLevel::Subregion;   // Communes
  case AdminLevel::Eight: return PlaceLevel::Locality;   // Collines
  case AdminLevel::Ten: return PlaceLevel::Sublocality;  // Sous-Collines
  default: break;
  }

  return PlaceLevel::Unknown;
}
}  // namespace specs
}  // namespace regions
}  // namespace generator
