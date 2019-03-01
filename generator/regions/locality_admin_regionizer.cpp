#include "generator/regions/locality_admin_regionizer.hpp"

#include "generator/regions/region.hpp"

namespace generator
{
namespace regions
{
LocalityAdminRegionizer::LocalityAdminRegionizer(PlaceCenter const & placeCenter)
  : m_placeCenter(placeCenter)
{ }

bool LocalityAdminRegionizer::ApplyTo(Node::Ptr & tree)
{
  auto const & place = tree->GetData();

  auto & placeLabel = place.GetPlaceLabel();
  if (placeLabel && placeLabel->GetId() == m_placeCenter.GetId())
    return true;

  if (!place.GetRegion().HasAdminLevel())
    return ContainsSameLocality(tree);

  auto level = place.GetLevel();
  if (ObjectLevel::Locality == level)
    return ContainsSameLocality(tree);

  for (auto & subtree : tree->GetChildren())
  {
    auto & subregion = subtree->GetData().GetRegion();
    if (!subregion.Contains(m_placeCenter))
      continue;

    if (ApplyTo(subtree))
      return true;
    break;
  }

  if (IsFitNode(tree))
  {
    InsertInto(tree);
    return true;
  }

  return false;
}

bool LocalityAdminRegionizer::ContainsSameLocality(Node::Ptr const & tree)
{
  auto const & place = tree->GetData();
  auto level = place.GetLevel();
  if (ObjectLevel::Locality == level && place.GetName() == m_placeCenter.GetName())
    return true;

  for (auto & subtree : tree->GetChildren())
  {
    auto & subregion = subtree->GetData().GetRegion();
    if (subregion.Contains(m_placeCenter))
      return ContainsSameLocality(subtree);
  }

  return false;
}

bool LocalityAdminRegionizer::IsFitNode(Node::Ptr & node)
{
  auto const & place = node->GetData();

  if (place.GetRegion().GetAdminLevel() <= AdminLevel::Four)
    return false;

  return place.GetName() == m_placeCenter.GetName();
}

void LocalityAdminRegionizer::InsertInto(Node::Ptr & node)
{
  auto & parentPlace = node->GetData();
  auto logLevel = m_placeCenter.GetPlaceType() <= PlaceType::Town ? LINFO : LDEBUG;
  LOG(logLevel, ("Place", StringifyPlaceType(m_placeCenter.GetPlaceType()), "object", m_placeCenter.GetId(),
                 "(", GetPlaceNotation(m_placeCenter), ")",
                 "has bounded by region", parentPlace.GetId(),
                 "(", GetPlaceNotation(parentPlace), ")"));

  Region placeRegion{m_placeCenter.GetMultilangName(), m_placeCenter.GetRegionData(),
                     parentPlace.GetRegion().GetPolygon()};
  auto placeNode = std::make_shared<Node>(RegionPlace{ObjectLevel::Locality, std::move(placeRegion),
                                                      m_placeCenter});
  placeNode->SetParent(node);

  placeNode->SetChildren(std::move(node->GetChildren()));
  for (auto & child : placeNode->GetChildren())
    child->SetParent(placeNode);

  node->AddChild(std::move(placeNode));
}
}  // namespace regions
}  // namespace generator
