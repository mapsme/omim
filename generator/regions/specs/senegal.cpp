#include "generator/regions/specs/senegal.hpp"

#include "generator/regions/country_specifier_builder.hpp"

namespace generator
{
namespace regions
{
namespace specs
{
REGISTER_COUNTRY_SPECIFIER(SenegalSpecifier);

PlaceLevel SenegalSpecifier::GetSpecificCountryLevel(Region const & region) const
{
  AdminLevel adminLevel = region.GetAdminLevel();
  switch (adminLevel)
  {
  case AdminLevel::Three:
    return PlaceLevel::Region;  // boundary of the "Regions" like Saint-Louis, Matam...
  case AdminLevel::Six:
    return PlaceLevel::Subregion;  // boundaries of the "Départements" see a list
  default: break;
  }

  return PlaceLevel::Unknown;
}
}  // namespace specs
}  // namespace regions
}  // namespace generator
