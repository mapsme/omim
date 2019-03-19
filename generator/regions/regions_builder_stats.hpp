#pragma once

#include "generator/regions/node.hpp"
#include "generator/regions/place_point.hpp"

#include <map>

namespace generator
{
namespace regions
{
struct CountryRegionsBuilderStats
{
  using Counter = unsigned long long;

  struct AdminLevelStats
  {
    Counter m_count;
    std::map<PlaceType, Counter> m_placeCounts;
  };

  std::map<ObjectLevel, Counter> m_objectLevelCounts;
  std::map<PlaceType, Counter> m_placeCounts;
  std::map<PlaceType, Counter> m_placePointsCounts;
  std::map<PlaceType, Counter> m_unboudedPlacePointsCounts;
  std::map<AdminLevel, AdminLevelStats> m_adminLevels;

  void Update(Node::PtrList const & countryOuters);
  void Update(PlacePointsMap const & placePoints, PlacePointsMap const & unboudedPlacePoints);

private:
  void UpdateByNode(Node::Ptr const & node);
  void UpdateAdminStatsByNode(AdminLevelStats & adminLevelStats, Node::Ptr const & node);
};
}  // namespace regions
}  // namespace generator
