#include "generator/regions/relative_nesting_specifier.hpp"

#include "generator/regions/admin_suburbs_marker.hpp"
#include "generator/regions/locality_admin_regionizer.hpp"

#include "base/logging.hpp"

namespace generator
{
namespace regions
{
void RelativeNestingSpecifier::AddPlaces(Node::PtrList & outers,
                                         RegionsBuilder::PlacePointsMap & placePointsMap)
{
  AddLocalities(outers, placePointsMap);

  for (auto & tree : outers)
  {
    AdminSuburbsMarker suburbsMarker{};
    suburbsMarker.MarkSuburbs(tree);
  }
}

void RelativeNestingSpecifier::AddLocalities(Node::PtrList & outers,
                                             RegionsBuilder::PlacePointsMap & placePointsMap)
{
  auto localityPlaceTypes = {PlaceType::City, PlaceType::Town};
  for (auto placeType : localityPlaceTypes)
  {
    auto placePoint = begin(placePointsMap);
    while (placePoint != end(placePointsMap))
    {
      auto const & place = placePoint->second;
      if (placeType == place.GetPlaceType() && AddPlacePoint(outers, place))
      {
        placePoint = placePointsMap.erase(placePoint);
        continue;
      }

      ++placePoint;
    }
  }
}

bool RelativeNestingSpecifier::AddPlacePoint(Node::PtrList & outers, PlacePoint const & placePoint)
{
  for (auto & tree : outers)
  {
    auto & country = tree->GetData();
    auto const & countryRegion = country.GetRegion();
    if (!countryRegion.Contains(placePoint))
      continue;

    LocalityAdminRegionizer regionizer{placePoint};
    if (!regionizer.ApplyTo(tree))
    {
      LOG(LDEBUG, ("Can't to define boundary for the",
                   StringifyPlaceType(placePoint.GetPlaceType()), "place",
                   placePoint.GetId(), "(", GetPlaceNotation(placePoint), ")",
                   "in", GetPlaceNotation(country)));
      return false;
    }

    return true;
  }

  return false;
}
}  // namespace regions
}  // namespace generator
