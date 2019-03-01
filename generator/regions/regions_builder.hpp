#pragma once

#include "generator/regions/node.hpp"
#include "generator/regions/place_center.hpp"
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
  using PlaceCentersMap = std::unordered_map<base::GeoObjectId, PlaceCenter>;
  using StringsList = std::vector<std::string>;
  using CountryTrees = std::multimap<std::string, Node::Ptr>;
  using NormalizedCountryFn = std::function<void(std::string const &, Node::Ptr const &)>;

  RegionsBuilder(Regions && regions, PlaceCentersMap && placeCentersMap, size_t threadsCount = 1);

  Regions const & GetCountries() const;
  StringsList GetCountryNames() const;
  void ForEachNormalizedCountry(NormalizedCountryFn const & fn);
private:
  static Node::PtrList MakeSelectedRegionsByCountry(Region const & country,
                                                    Regions const & allRegions,
                                                    CountrySpecifier const & countrySpecifier);
  std::vector<Node::Ptr> BuildCountryRegionTrees(RegionsBuilder::Regions const & countries,
                                                 CountrySpecifier const & countrySpecifier);
  static Node::Ptr BuildCountryRegionTree(Region const & country, Regions const & allRegions,
                                          PlaceCentersMap const & placeCentersMap,
                                          CountrySpecifier const & countrySpecifier);
  static void Insert(Node::Ptr & tree, Node::Ptr && node, CountrySpecifier const & countrySpecifier);
  static void AddChild(Node::Ptr & tree, Node::Ptr && node);
  // Return: 0 - no relation, 1 - |l| contains |r|, -1 - |r| contains |l|.
  static int Compare(RegionPlace const & l, RegionPlace const & r, CountrySpecifier const & countrySpecifier);
  std::unique_ptr<CountrySpecifier> GetCountrySpecifier(std::string const & countryName);
  void ReviseSublocalityDisposition(Node::Ptr & tree) const;

  Regions m_countries;
  Regions m_regions;
  PlaceCentersMap m_placeCentersMap;
  size_t m_threadsCount;
};
}  // namespace regions
}  // namespace generator
