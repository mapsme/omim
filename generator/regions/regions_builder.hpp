#pragma once

#include "generator/regions/country_specifier.hpp"
#include "generator/regions/node.hpp"
#include "generator/regions/region.hpp"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace generator
{
namespace regions
{
// This class is needed to build a hierarchy of regions. We can have several nodes for a region
// with the same name, represented by a multi-polygon (several polygons).
class RegionsBuilder
{
public:
  using Regions = std::vector<Region>;
  using StringsList = std::vector<std::string>;
  using CountryFn = std::function<void(std::string const &, Node::PtrList const &)>;

  explicit RegionsBuilder(Regions && regions, PlacePointsMap && placePointsMap,
                          size_t threadsCount = 1);

  Regions const & GetCountriesOuters() const;
  StringsList GetCountryNames() const;
  void ForEachCountry(CountryFn fn);

private:
  static constexpr double kAreaRelativeErrorPercent = 0.1;

  void MoveLabelPlacePoints(PlacePointsMap & placePointsMap, Regions & regions);
  Regions FormRegionsInAreaOrder(Regions && regions);
  Regions ExtractCountriesOuters(Regions & regions);
  Node::PtrList BuildCountryRegionTrees(Regions const & outers,
                                        CountrySpecifier const & countrySpecifier);
  static Node::Ptr BuildCountryRegionTree(Region const & outer, Regions const & regionsInAreaOrder,
                                          CountrySpecifier const & countrySpecifier);
  static std::vector<Node::Ptr> MakeCountryNodesInAreaOrder(
      Region const & countryOuter, Regions const & regionsInAreaOrder,
      CountrySpecifier const & countrySpecifier);
  static Node::Ptr ChooseParent(std::vector<Node::Ptr> const & nodesInAreaOrder,
                                std::vector<Node::Ptr>::const_reverse_iterator forItem,
                                CountrySpecifier const & countrySpecifier);
  static std::vector<Node::Ptr>::const_reverse_iterator FindAreaLowerBoundRely(
      std::vector<Node::Ptr> const & nodesInAreaOrder,
      std::vector<Node::Ptr>::const_reverse_iterator forItem);
  // Return: 0 - no relation, 1 - |l| contains |r|, -1 - |r| contains |l|.
  static int Compare(LevelRegion const & l, LevelRegion const & r,
                     CountrySpecifier const & countrySpecifier);
  static bool IsAreaLessRely(Region const & l, Region const & r);
  std::unique_ptr<CountrySpecifier> GetCountrySpecifier(std::string const & countryName);

  Regions m_countriesOuters;
  Regions m_regionsInAreaOrder;
  size_t m_threadsCount;
};
}  // namespace regions
}  // namespace generator
