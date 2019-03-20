#include "generator/regions/locality_admin_regionizer.hpp"

#include "generator/regions/region.hpp"

#include <utility>

namespace generator
{
namespace regions
{
LocalityAdminRegionizer::LocalityAdminRegionizer(PlacePoint const & placePoint)
  : m_placePoint(placePoint)
{ }

bool LocalityAdminRegionizer::ApplyTo(Node::Ptr & tree) const
{
  auto const & place = tree->GetData();

  auto const & placeLabel = place.GetPlaceLabel();
  if (placeLabel && placeLabel->GetId() == m_placePoint.GetId())
    return true;

  auto const level = place.GetLevel();
  if (ObjectLevel::Locality == level)
    return ContainsSameLocality(tree);

  for (auto & subtree : tree->GetChildren())
  {
    auto const & subregion = subtree->GetData().GetRegion();
    if (!subregion.Contains(m_placePoint))
      continue;

    if (ApplyTo(subtree))
      return true;
  }

  if (IsFitNode(tree))
  {
    InsertInto(tree);
    return true;
  }

  return false;
}

bool LocalityAdminRegionizer::ContainsSameLocality(Node::Ptr const & tree) const
{
  auto const & place = tree->GetData();
  auto const level = place.GetLevel();
  if (ObjectLevel::Locality == level && place.GetName() == m_placePoint.GetName())
    return true;

  for (auto & subtree : tree->GetChildren())
  {
    auto const & subregion = subtree->GetData().GetRegion();
    if (subregion.Contains(m_placePoint))
      return ContainsSameLocality(subtree);
  }

  return false;
}

bool LocalityAdminRegionizer::IsFitNode(Node::Ptr & node) const
{
  auto const & place = node->GetData();

  auto const adminLevel = place.GetRegion().GetAdminLevel();
  if (adminLevel == AdminLevel::Unknown || adminLevel <= AdminLevel::Three)
    return false;

  return place.GetName() == m_placePoint.GetName();
}

void LocalityAdminRegionizer::InsertInto(Node::Ptr & node) const
{
  auto const & parentPlace = node->GetData();
  LOG(LDEBUG, ("Place", StringifyPlaceType(m_placePoint.GetPlaceType()), "object", m_placePoint.GetId(),
               "(", GetPlaceNotation(m_placePoint), ")",
               "is bounded by region", parentPlace.GetId(),
               "(", GetPlaceNotation(parentPlace), ")"));

  Region placeRegion{m_placePoint.GetMultilangName(), m_placePoint.GetRegionData(),
                     parentPlace.GetRegion().GetPolygon()};
  auto place = LevelPlace{ObjectLevel::Locality, {std::move(placeRegion), m_placePoint}};
  auto placeNode = std::make_shared<Node>(std::move(place));
  placeNode->SetParent(node);

  placeNode->SetChildren(std::move(node->GetChildren()));
  for (auto & child : placeNode->GetChildren())
    child->SetParent(placeNode);

  node->AddChild(std::move(placeNode));
}
}  // namespace regions
}  // namespace generator
