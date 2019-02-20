#include "generator/regions/relative_nesting_specifier.hpp"

#include "generator/regions/admin_suburbs_marker.hpp"
#include "generator/regions/locality_admin_regionizer.hpp"

#include "base/logging.hpp"

namespace generator
{
namespace regions
{
void RelativeNestingSpecifier::AddPlaces(Node::Ptr & tree,
                                         RegionsBuilder::PlaceCentersMap const & placeCentersMap)
{
  if (!tree)
    return;

  AddLocalities(tree, placeCentersMap);

  AdminSuburbsMarker suburbsMarker{placeCentersMap};
  suburbsMarker.MarkSuburbs(tree);
}

void RelativeNestingSpecifier::AddLocalities(Node::Ptr & tree,
                                             RegionsBuilder::PlaceCentersMap const & placeCentersMap)
{
  auto & country = tree->GetData();
  auto const & countryRegion = country.GetRegion();
  auto localityPlaceTypes = {PlaceType::City, PlaceType::Town, PlaceType::Village, PlaceType::Hamlet};
  for (auto placeType : localityPlaceTypes)
  {
    for (auto const & placeItem : placeCentersMap)
    {
      if (placeType == placeItem.second.GetPlaceType() && countryRegion.Contains(placeItem.second))
      {
        auto const & place = placeItem.second;
        LocalityAdminRegionizer regionizer{place};
        if (!regionizer.ApplyTo(tree))
        {
          LOG(LINFO, ("Can't to define boundary for the city place", place.GetId(), "(", place.GetName(), ")",
                      "in", country.GetName()));
        }
      }
    }
  }
}
}  // namespace regions
}  // namespace generator
