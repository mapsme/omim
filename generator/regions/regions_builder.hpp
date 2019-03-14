#pragma once

#include "base/thread_pool_computational.hpp"

#include "generator/regions/node.hpp"
#include "generator/regions/place_point.hpp"
#include "generator/regions/region.hpp"
#include "generator/regions/to_string_policy.hpp"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace generator
{
namespace regions
{

class CountrySpecifier;

// This class is needed to build a hierarchy of regions. We can have several nodes for a region
// with the same name, represented by a multi-polygon (several polygons).
class RegionsBuilder
{
public:
  using Regions = std::vector<Region>;
  using RegionPlaceLot = std::vector<RegionPlace>;
  using PlacePointsMap = std::unordered_map<base::GeoObjectId, PlacePoint>;
  using StringsList = std::vector<std::string>;
  struct CountryStats;
  using CountryFn = std::function<void(std::string const &, Node::PtrList const &, CountryStats const &)>;

  RegionsBuilder(Regions && regions, PlacePointsMap && placePointsMap, size_t threadsCount = 1);

  RegionPlaceLot const & GetCountriesOuters() const;
  StringsList GetCountryNames() const;
  void ForEachCountry(CountryFn const & fn);
private:
  using ParentChildPairs = std::vector<std::pair<Node::Ptr, Node::Ptr>>;

  RegionPlaceLot FormRegionPlaceOrder(Regions && regions, PlacePointsMap const & placePointsMap);
  void EraseLabelPlacePoints(PlacePointsMap & placePointsMap, RegionPlaceLot const & placeOrder);
  RegionPlaceLot ExtractCountriesOuters(RegionPlaceLot & placeOrder);

  std::vector<Node::Ptr> BuildCountryRegionTrees(RegionPlaceLot const & countryOuters,
                                                 CountrySpecifier const & countrySpecifier);
  std::vector<Node::Ptr> MakeCountryNodeOrder(RegionPlace const & countryOuter,
                                              CountrySpecifier const & countrySpecifier);
  static ParentChildPairs FindAllParentChildPairs(std::vector<Node::Ptr> const & nodeOrder,
                                                  std::size_t startOffset, std::size_t step,
                                                  CountrySpecifier const & countrySpecifier);
  static Node::Ptr FindParent(std::vector<Node::Ptr> const & nodeOrder,
                              std::vector<Node::Ptr>::const_iterator forItem,
                              CountrySpecifier const & countrySpecifier);
  static std::vector<Node::Ptr>::const_reverse_iterator FindLowerAreaBound(
      std::vector<Node::Ptr> const & nodeOrder, std::vector<Node::Ptr>::const_iterator forItem);
  void BindNodeChildren(ParentChildPairs const & parentChildPairs,
                        CountrySpecifier const & countrySpecifier);
  // Return: 0 - no relation, 1 - |l| contains |r|, -1 - |r| contains |l|.
  static int Compare(LevelPlace const & l, LevelPlace const & r, CountrySpecifier const & countrySpecifier);
  std::unique_ptr<CountrySpecifier> GetCountrySpecifier(std::string const & countryName);
  static void ReviseSublocalityDisposition(Node::Ptr & tree);

  PlacePointsMap FindCountryPlacePoints(RegionPlaceLot const & countryOuters);
  std::vector<std::future<PlacePointsMap>> PushCountryPlacePointsFinders(RegionPlaceLot const & countryOuter);
  static PlacePointsMap FindCountryPlacePointsForRange(RegionPlaceLot const & countryOuters,
                                                       PlacePointsMap::iterator begin,
                                                       PlacePointsMap::iterator end);

  void UpdateStats(CountryStats & stats, Node::Ptr const & node);
  void UpdateStats(CountryStats & stats, AdminLevel adminLevel, Node::Ptr const & node);
  void UpdateStats(CountryStats & stats, PlacePointsMap const & placePoints,
                   PlacePointsMap const & unboudedPlacePoints);

  RegionPlaceLot m_countriesOuters;
  RegionPlaceLot m_regionPlaceOrder; // in descending order by area
  PlacePointsMap m_placePointsMap;
  size_t m_threadsCount;
  base::thread_pool::computational::ThreadPool m_threadPool;
};

struct RegionsBuilder::CountryStats
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
};
}  // namespace regions
}  // namespace generator
