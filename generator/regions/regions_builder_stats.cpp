#include "generator/regions/regions_builder_stats.hpp"

namespace generator
{
namespace regions
{
void CountryRegionsBuilderStats::Update(Node::PtrList const & countryOuters)
{
  for (auto const & tree : countryOuters)
    Visit(tree, [this] (Node::Ptr const & node) { UpdateByNode(node); } );
}

void CountryRegionsBuilderStats::UpdateByNode(Node::Ptr const & node)
{
  auto const & place = node->GetData();

  auto const level = place.GetLevel();
  if (level != ObjectLevel::Unknown)
    ++m_objectLevelCounts[level];

  auto const placeType = place.GetPlaceType();
  if (placeType != PlaceType::Unknown)
    ++m_placeCounts[placeType];

  auto const adminLevel = place.GetAdminLevel();
  if (adminLevel != AdminLevel::Unknown)
    UpdateAdminStatsByNode(m_adminLevels[adminLevel], node);
}

void CountryRegionsBuilderStats::UpdateAdminStatsByNode(AdminLevelStats & adminLevelStats,
                                                        Node::Ptr const & node)
{
  auto const & place = node->GetData();

  ++adminLevelStats.m_count;

  auto const placeType = place.GetPlaceType();
  if (placeType != PlaceType::Unknown)
    ++adminLevelStats.m_placeCounts[placeType];

  for (auto const & subnode : node->GetChildren())
  {
    auto const & subplace = subnode->GetData();
    auto const subplaceType = subplace.GetPlaceType();
    if (subplaceType != PlaceType::Unknown)
      ++adminLevelStats.m_placeCounts[subplaceType];
  }
}

void CountryRegionsBuilderStats::Update(PlacePointsMap const & placePoints,
                                        PlacePointsMap const & unboudedPlacePoints)
{
  for (auto const & place : placePoints)
    ++m_placePointsCounts[place.second.GetPlaceType()];

  for (auto const & place : unboudedPlacePoints)
    ++m_unboudedPlacePointsCounts[place.second.GetPlaceType()];
}
}  // namespace regions
}  // namespace generator
