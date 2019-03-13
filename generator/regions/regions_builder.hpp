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
  using CountryTrees = std::multimap<std::string, Node::Ptr>;
  using NormalizedCountryFn = std::function<void(std::string const &, Node::Ptr const &)>;

  RegionsBuilder(Regions && regions, PlacePointsMap && placePointsMap, size_t threadsCount = 1);

  RegionPlaceLot const & GetCountriesOuters() const;
  StringsList GetCountryNames() const;
  void ForEachNormalizedCountry(NormalizedCountryFn const & fn);
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
  void BindNodeChildren(ParentChildPairs const & parentChildPairs,
                        CountrySpecifier const & countrySpecifier);
  // Return: 0 - no relation, 1 - |l| contains |r|, -1 - |r| contains |l|.
  static int Compare(LevelPlace const & l, LevelPlace const & r, CountrySpecifier const & countrySpecifier);
  std::unique_ptr<CountrySpecifier> GetCountrySpecifier(std::string const & countryName);
  void ReviseSublocalityDisposition(Node::Ptr & tree) const;

  RegionPlaceLot m_countriesOuters;
  RegionPlaceLot m_regionPlaceOrder; // in descending order by area
  PlacePointsMap m_placePointsMap;
  size_t m_threadsCount;
  base::thread_pool::computational::ThreadPool m_threadPool;
};
}  // namespace regions
}  // namespace generator
