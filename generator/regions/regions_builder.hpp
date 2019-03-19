#pragma once

#include "generator/regions/node.hpp"
#include "generator/regions/place_point.hpp"
#include "generator/regions/region.hpp"
#include "generator/regions/regions_builder_stats.hpp"
#include "generator/regions/to_string_policy.hpp"

#include "base/thread_pool_computational.hpp"

#include <functional>
#include <memory>
#include <string>
#include <utility>
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
  using StringsList = std::vector<std::string>;
  using CountryFn = std::function<void(std::string const &, Node::PtrList const &,
                                       CountryRegionsBuilderStats const &)>;

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
  static bool IsAreaLess(Region const & lhs, Region const & rhs);
  static void ReviseSublocalityDisposition(Node::Ptr & tree);

  PlacePointsMap FindCountryPlacePoints(RegionPlaceLot const & countryOuters);
  std::vector<std::future<PlacePointsMap>> PushCountryPlacePointsFinders(RegionPlaceLot const & countryOuter);
  static PlacePointsMap FindCountryPlacePointsForRange(RegionPlaceLot const & countryOuters,
                                                       PlacePointsMap::iterator begin,
                                                       PlacePointsMap::iterator end);

  static constexpr double k_areaRelativeErrorPercent = 0.1;

  RegionPlaceLot m_countriesOuters;
  RegionPlaceLot m_regionPlaceOrder; // in descending order by area
  PlacePointsMap m_placePointsMap;
  size_t m_threadsCount;
  base::thread_pool::computational::ThreadPool m_threadPool;
};
}  // namespace regions
}  // namespace generator
