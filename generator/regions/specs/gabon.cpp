#include "generator/regions/specs/gabon.hpp"

namespace generator
{
namespace regions
{
namespace specs
{
PlaceLevel GabonSpecifier::GetSpecificCountryLevel(Region const & region) const
{
  return PlaceLevel::Unknown;
}
}  // namespace specs
}  // namespace regions
}  // namespace generator
