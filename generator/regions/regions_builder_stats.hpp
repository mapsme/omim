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
    Counter count;
    std::map<PlaceType, Counter> placeCounts;
  };

  std::map<ObjectLevel, Counter> objectLevelCounts;
  std::map<PlaceType, Counter> placeCounts;
  std::map<PlaceType, Counter> placePointsCounts;
  std::map<PlaceType, Counter> unboudedPlacePointsCounts;
  std::map<AdminLevel, AdminLevelStats> adminLevels;

  void Update(Node::PtrList const & countryOuters);
  void Update(PlacePointsMap const & placePoints, PlacePointsMap const & unboudedPlacePoints);

private:
  void UpdateByNode(Node::Ptr const & node);
  void UpdateAdminStatsByNode(AdminLevelStats & adminLevelStats, Node::Ptr const & node);
};
}  // namespace regions
}  // namespace generator
