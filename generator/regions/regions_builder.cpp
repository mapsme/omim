#include "generator/regions/regions_builder.hpp"

#include "base/assert.hpp"
#include "base/thread_pool_computational.hpp"
#include "base/stl_helpers.hpp"
#include "base/thread_pool_computational.hpp"

#include "generator/regions/country_specifier.hpp"
#include "generator/regions/relative_nesting_specifier.hpp"
#include "generator/regions/ru_specifier.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <functional>
#include <numeric>
#include <queue>
#include <thread>
#include <unordered_set>

namespace generator
{
namespace regions
{
RegionsBuilder::RegionsBuilder(Regions && regions, PlaceCentersMap && placeCentersMap, size_t threadsCount)
  : m_regions(std::move(regions))
  , m_placeCentersMap(std::move(placeCentersMap))
  , m_threadsCount(threadsCount)
{
  ASSERT(m_threadsCount != 0, ());

  auto const isCountry = [](Region const & r) { return r.IsCountry(); };
  std::copy_if(std::begin(m_regions), std::end(m_regions), std::back_inserter(m_countries), isCountry);
  base::EraseIf(m_regions, isCountry);
  auto const cmp = [](Region const & l, Region const & r) { return l.GetArea() > r.GetArea(); };
  std::sort(std::begin(m_countries), std::end(m_countries), cmp);
}

RegionsBuilder::Regions const & RegionsBuilder::GetCountries() const
{
  return m_countries;
}

RegionsBuilder::StringsList RegionsBuilder::GetCountryNames() const
{
  StringsList result;
  std::unordered_set<std::string> set;
  for (auto const & c : GetCountries())
  {
    auto name = c.GetName();
    if (set.insert(name).second)
      result.emplace_back(std::move(name));
  }

  return result;
}

void RegionsBuilder::ForEachNormalizedCountry(NormalizedCountryFn const & fn)
{
  for (auto const & countryName : GetCountryNames())
  {
    auto countrySpecifier = GetCountrySpecifier(countryName);

    RegionsBuilder::Regions country;
    auto const & countries = GetCountries();
    auto const pred = [&](const Region & r) { return countryName == r.GetName(); };
    std::copy_if(std::begin(countries), std::end(countries), std::back_inserter(country), pred);
    auto const countryTrees = BuildCountryRegionTrees(country, *countrySpecifier);
    auto tree = std::accumulate(std::begin(countryTrees), std::end(countryTrees),
                                      Node::Ptr(), MergeTree);
    if (!tree)
      return;
    NormalizeTree(tree);

    countrySpecifier->AddPlaces(tree, m_placeCentersMap);
    countrySpecifier->AdjustRegionsLevel(tree, m_placeCentersMap);

    ReviseSublocalityDisposition(tree);

    fn(countryName, tree);
  }
}

std::vector<Node::Ptr> RegionsBuilder::BuildCountryRegionTrees(RegionsBuilder::Regions const & countries,
                                                               CountrySpecifier const & countrySpecifier)
{
  std::vector<std::future<Node::Ptr>> tmp;
  {
    base::thread_pool::computational::ThreadPool threadPool(m_threadsCount);
    for (auto const & country : countries)
    {
      auto result = threadPool.Submit(&RegionsBuilder::BuildCountryRegionTree,
                                      std::cref(country), std::cref(m_regions), std::cref(m_placeCentersMap),
                                      std::cref(countrySpecifier));
      tmp.emplace_back(std::move(result));
    }
  }
  std::vector<Node::Ptr> res;
  res.reserve(tmp.size());
  std::transform(std::begin(tmp), std::end(tmp),
                 std::back_inserter(res), [](auto & f) { return f.get(); });
  return res;
}

Node::Ptr RegionsBuilder::BuildCountryRegionTree(Region const & country, Regions const & allRegions,
                                                 PlaceCentersMap const & placeCentersMap,
                                                 CountrySpecifier const & countrySpecifier)
{
  Node::Ptr tree = std::make_shared<Node>(RegionPlace{ObjectLevel::Country, country});

  auto addRegion = [&] (Region const & region) {
    boost::optional<PlaceCenter> placeLabel;
    if (auto levelOsmId = region.GetLabelOsmIdOptional())
    {
      auto label = placeCentersMap.find(*levelOsmId);
      if (label != placeCentersMap.end())
        placeLabel = label->second;
    }
    auto level = countrySpecifier.GetLevel(region, placeLabel);
    auto node = std::make_shared<Node>(RegionPlace{level, region, std::move(placeLabel)});

    Insert(tree, std::move(node), countrySpecifier);
  };

  for (auto const region : allRegions)
  {
    if (country.Contains(region))
      addRegion(region);
  }

  if (tree->GetChildren().empty())
    return {};

  return tree;
}

void RegionsBuilder::Insert(Node::Ptr & tree, Node::Ptr && node, CountrySpecifier const & countrySpecifier)
{
  auto & nodePlace = node->GetData();
  auto & children = tree->GetChildren();
  auto && subtree = children.begin();
  while (subtree != children.end())
  {
    auto c = Compare(nodePlace, (*subtree)->GetData(), countrySpecifier);
    if (c < 0)
    {
      Insert(*subtree, std::move(node), countrySpecifier);
      return;
    }

    if (c > 0)
    {
      AddChild(node, std::move(*subtree));
      *subtree = std::move(children.back());
      children.pop_back();
      continue;
    }

    ++subtree;
  }

  CHECK(!node->GetParent(), ());
  AddChild(tree, std::move(node));
}

void RegionsBuilder::AddChild(Node::Ptr & tree, Node::Ptr && node)
{
  node->SetParent(tree);
  auto greaterArea = [&] (Node::Ptr const & other) {
    return node->GetData().GetRegion().GetArea() > other->GetData().GetRegion().GetArea();
  };
  auto & children = tree->GetChildren();
  auto pos = std::find_if(children.begin(), children.end(), greaterArea);
  children.insert(pos, std::move(node));
}

int RegionsBuilder::Compare(RegionPlace const & l, RegionPlace const & r,
                            CountrySpecifier const & countrySpecifier)
{
  auto const & lRegion = l.GetRegion();
  auto const & rRegion = r.GetRegion();
  if (lRegion.Contains(rRegion))
    return 1;
  if (rRegion.Contains(lRegion))
    return -1;

  if ((lRegion.Contains(r.GetCenter()) || rRegion.Contains(l.GetCenter())) &&
      lRegion.CalculateOverlapPercentage(rRegion) > 50.0)
  {
    return countrySpecifier.CompareWeight(l, r);
  }

  return 0;
}

std::unique_ptr<CountrySpecifier> RegionsBuilder::GetCountrySpecifier(std::string const & countryName)
{
  if (u8"Россия" == countryName)
    return std::make_unique<RuSpecifier>();

  return std::make_unique<RelativeNestingSpecifier>();
}

void RegionsBuilder::ReviseSublocalityDisposition(Node::Ptr & tree) const
{
  auto & place = tree->GetData();
  auto const level = place.GetLevel();
  if (ObjectLevel::Locality == level)
    return;

  if (ObjectLevel::Suburb == level || ObjectLevel::Sublocality == level)
  {
    LOG(LINFO, ("The", GetLabel(level), "object", place.GetId(),
                "(", GetPlaceNotation(place), ") are skipped: outside locality"));
    place.SetLevel(ObjectLevel::Unknown);
  }

  for (auto & subtree : tree->GetChildren())
    ReviseSublocalityDisposition(subtree);
}
}  // namespace regions
}  // namespace generator
