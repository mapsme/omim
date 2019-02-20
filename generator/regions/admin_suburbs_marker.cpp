#include "generator/regions/admin_suburbs_marker.hpp"

#include "generator/regions/region.hpp"

namespace generator
{
namespace regions
{
AdminSuburbsMarker::AdminSuburbsMarker(RegionsBuilder::PlaceCentersMap const & placeCentersMap)
  : m_placeCentersMap(placeCentersMap)
{ }

void AdminSuburbsMarker::MarkSuburbs(Node::Ptr & tree)
{
  auto & place = tree->GetData();
  if (ObjectLevel::Locality == place.GetLevel())
  {
    for (auto & subtree : tree->GetChildren())
      MarkSuburbsInCity(subtree);
    return;
  }

  for (auto & subtree : tree->GetChildren())
    MarkSuburbs(subtree);
}

void AdminSuburbsMarker::MarkSuburbsInCity(Node::Ptr & tree)
{
  auto & place = tree->GetData();
  if (ObjectLevel::Locality == place.GetLevel())
    return;

  auto & region = place.GetRegion();
  if (region.HasAdminLevel())
    place.SetLevel(ObjectLevel::Suburb);

  for (auto & subtree : tree->GetChildren())
    MarkUnderLocalityAsSublocalities(subtree);
}

void AdminSuburbsMarker::MarkUnderLocalityAsSublocalities(Node::Ptr & tree)
{
  auto & place = tree->GetData();
  auto const level = place.GetLevel();
  if (ObjectLevel::Locality == level)
    return;

  if (ObjectLevel::Suburb == level)
    place.SetLevel(ObjectLevel::Sublocality);
  else if (ObjectLevel::Unknown == level && place.GetRegion().HasAdminLevel())
    place.SetLevel(ObjectLevel::Sublocality);

  for (auto & subtree : tree->GetChildren())
    MarkUnderLocalityAsSublocalities(subtree);
}
}  // namespace regions
}  // namespace generator
