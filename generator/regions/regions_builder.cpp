#include "generator/regions/regions_builder.hpp"

#include "base/assert.hpp"
#include "base/stl_helpers.hpp"

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
RegionsBuilder::RegionsBuilder(Regions && regions, PlacePointsMap && placePointsMap, size_t threadsCount)
  : m_threadsCount(threadsCount)
  , m_threadPool(threadsCount)
{
  m_regionPlaceOrder = FormRegionPlaceOrder(std::move(regions), placePointsMap);

  m_placePointsMap = std::move(placePointsMap);
  EraseLabelPlacePoints(m_placePointsMap, m_regionPlaceOrder);

  m_countriesOuters = ExtractCountriesOuters(m_regionPlaceOrder);
}

RegionsBuilder::RegionPlaceLot RegionsBuilder::FormRegionPlaceOrder(
    Regions && regions, PlacePointsMap const & placePointsMap)
{
  RegionPlaceLot placeOrder;

  auto const cmp = [](Region const & l, Region const & r) { return l.GetArea() > r.GetArea(); };
  std::sort(std::begin(regions), std::end(regions), cmp);

  placeOrder.reserve(regions.size());
  for (auto & region : regions)
  {
    boost::optional<PlacePoint> placeLabel;
    if (auto labelOsmId = region.GetLabelOsmIdOptional())
    {
      auto label = placePointsMap.find(*labelOsmId);
      if (label != placePointsMap.end())
        placeLabel = label->second;
    }
    placeOrder.emplace_back(std::move(region), std::move(placeLabel));
  }

  return placeOrder;
}

void RegionsBuilder::EraseLabelPlacePoints(PlacePointsMap & placePointsMap,
                                           RegionPlaceLot const & placeOrder)
{
  for (auto const & place : placeOrder)
  {
    if (auto levelOsmId = place.GetRegion().GetLabelOsmIdOptional())
      placePointsMap.erase(*levelOsmId);
  }
}

RegionsBuilder::RegionPlaceLot RegionsBuilder::ExtractCountriesOuters(RegionPlaceLot & placeOrder)
{
  RegionPlaceLot countriesOuters;

  auto const isCountry = [](RegionPlace const & place) {
    return PlaceType::Country == place.GetPlaceType() || AdminLevel::Two == place.GetAdminLevel();
  };
  std::copy_if(std::begin(placeOrder), std::end(placeOrder), std::back_inserter(countriesOuters),
               isCountry);
  base::EraseIf(placeOrder, isCountry);

  return countriesOuters;
}

RegionsBuilder::RegionPlaceLot const & RegionsBuilder::GetCountriesOuters() const
{
  return m_countriesOuters;
}

RegionsBuilder::StringsList RegionsBuilder::GetCountryNames() const
{
  StringsList result;
  std::unordered_set<std::string> set;
  for (auto const & c : GetCountriesOuters())
  {
    auto name = c.GetName();
    if (set.insert(name).second)
      result.emplace_back(std::move(name));
  }

  return result;
}

void RegionsBuilder::ForEachCountry(CountryFn const & fn)
{
  for (auto const & countryName : GetCountryNames())
  {
    auto countrySpecifier = GetCountrySpecifier(countryName);

    RegionPlaceLot country;
    auto const & countries = GetCountriesOuters();
    auto const pred = [&](const RegionPlace & p) { return countryName == p.GetName(); };
    std::copy_if(std::begin(countries), std::end(countries), std::back_inserter(country), pred);
    auto countryTrees = BuildCountryRegionTrees(country, *countrySpecifier);
    auto countryPlacePoints = FindCountryPlacePoints(country);
    auto unboundedPlacePoints = countryPlacePoints;

    countrySpecifier->AddPlaces(countryTrees, unboundedPlacePoints);
    countrySpecifier->AdjustRegionsLevel(countryTrees, unboundedPlacePoints);

    std::for_each(begin(countryTrees), end(countryTrees), ReviseSublocalityDisposition);

    CountryStats stats;
    for (auto const & tree : countryTrees)
      Visit(tree, [&stats, this] (Node::Ptr const & node) { UpdateStats(stats, node); } );
    UpdateStats(stats, countryPlacePoints, unboundedPlacePoints);

    fn(countryName, countryTrees, stats);
  }
}

std::vector<Node::Ptr> RegionsBuilder::BuildCountryRegionTrees(RegionPlaceLot const & countryOuters,
                                                               CountrySpecifier const & countrySpecifier)
{
  std::vector<Node::Ptr> res;
  res.reserve(countryOuters.size());

  for (auto const & outer : countryOuters)
  {
    std::vector<std::future<ParentChildPairs>> tasks;
    auto nodes = MakeCountryNodeOrder(outer, countrySpecifier);
    for (std::size_t t = 0; t < m_threadsCount; ++t)
    {
      tasks.push_back(m_threadPool.Submit(FindAllParentChildPairs,
                                          std::cref(nodes), t, m_threadsCount, std::cref(countrySpecifier)));
    }

    for (auto & t : tasks)
      BindNodeChildren(t.get(), countrySpecifier);

    auto tree = nodes.front();
    if (!tree->GetChildren().empty())
      res.push_back(tree);
  }

  return res;
}

std::vector<Node::Ptr> RegionsBuilder::MakeCountryNodeOrder(RegionPlace const & countryOuter,
                                                            CountrySpecifier const & countrySpecifier)
{
  auto const & countryOuterRegion = countryOuter.GetRegion();
  std::vector<Node::Ptr> nodes{std::make_shared<Node>(LevelPlace{ObjectLevel::Country, countryOuter})};
  for (auto const & place : m_regionPlaceOrder)
  {
    auto const & placeRegion = place.GetRegion();
    if (countryOuterRegion.ContainsRect(placeRegion))
    {
      auto level = countrySpecifier.GetLevel(place);
      auto node = std::make_shared<Node>(LevelPlace{level, place});
      nodes.emplace_back(std::move(node));
    }
  }

  return nodes;
}

RegionsBuilder::ParentChildPairs RegionsBuilder::FindAllParentChildPairs(
    std::vector<Node::Ptr> const & nodeOrder, std::size_t startOffset, std::size_t step,
    CountrySpecifier const & countrySpecifier)
{
  ParentChildPairs res;

  auto size = nodeOrder.size();
  for (auto i = (std::ptrdiff_t(size) - 1) - std::ptrdiff_t(startOffset); i > 0; i -= step)
  {
    auto const & place = nodeOrder[i]->GetData();
    auto const & placeRegion = place.GetRegion();
    for (auto j = i - 1; j >= 0; --j)
    {
      auto const & parentCandidate = nodeOrder[j];
      auto const & candidatePlace = parentCandidate->GetData();
      auto const & candidateRegion = candidatePlace.GetRegion();
      if (!candidateRegion.ContainsRect(placeRegion))
        continue;

      auto c = Compare(candidatePlace, place, countrySpecifier);
      if (c == 1)
      {
        res.emplace_back(parentCandidate, nodeOrder[i]);
        break;
      }

      if (c == -1)
        res.emplace_back(nodeOrder[i], parentCandidate);
    }
  }

  return res;
}

void RegionsBuilder::BindNodeChildren(ParentChildPairs const & parentChildPairs,
                                      CountrySpecifier const & countrySpecifier)
{
  for (auto & pair : parentChildPairs)
  {
    auto & parent = pair.first;
    auto & child = pair.second;

    if (auto childCurrParent = child->GetParent())
    {
      auto c = Compare(childCurrParent->GetData(), parent->GetData(), countrySpecifier);
      if (c <= 0)
        continue;

      auto & parentClildren = childCurrParent->GetChildren();
      parentClildren.erase(std::find(begin(parentClildren), end(parentClildren), child));
    }

    child->SetParent(parent);
    parent->AddChild(std::move(child));
  }
}

int RegionsBuilder::Compare(LevelPlace const & l, LevelPlace const & r,
                            CountrySpecifier const & countrySpecifier)
{
  auto const & lRegion = l.GetRegion();
  auto const & rRegion = r.GetRegion();
  auto lArea = lRegion.GetArea();
  auto rArea = rRegion.GetArea();

  if (lArea > 1.001 * rArea && lRegion.Contains(rRegion))
    return 1;
  if (rArea > 1.001 * lArea && rRegion.Contains(lRegion))
    return -1;

  if (lRegion.CalculateOverlapPercentage(rRegion) < 50.0)
    return 0;

  if (0.5 * lArea >= rArea)
    return 1;
  if (0.5 * rArea >= lArea)
    return -1;

  return countrySpecifier.CompareWeight(l, r);
}

std::unique_ptr<CountrySpecifier> RegionsBuilder::GetCountrySpecifier(std::string const & countryName)
{
  if (u8"Россия" == countryName)
    return std::make_unique<RuSpecifier>();

  return std::make_unique<RelativeNestingSpecifier>();
}

void RegionsBuilder::ReviseSublocalityDisposition(Node::Ptr & tree)
{
  auto & place = tree->GetData();
  auto const level = place.GetLevel();
  if (ObjectLevel::Locality == level)
    return;

  if (ObjectLevel::Suburb == level || ObjectLevel::Sublocality == level)
  {
    LOG(LDEBUG, ("The", GetLabel(level), "object", place.GetId(),
                 "(", GetPlaceNotation(place), ") are skipped: outside locality"));
    place.SetLevel(ObjectLevel::Unknown);
  }

  for (auto & subtree : tree->GetChildren())
    ReviseSublocalityDisposition(subtree);
}

RegionsBuilder::PlacePointsMap RegionsBuilder::FindCountryPlacePoints(RegionPlaceLot const & countryOuters)
{
  PlacePointsMap countryPoints;

  auto tasks = PushCountryPlacePointsFinders(countryOuters);
  for (auto & t : tasks)
  {
    auto part = t.get();
    countryPoints.insert(begin(part), end(part));
  }

  return countryPoints;
}

std::vector<std::future<RegionsBuilder::PlacePointsMap>> RegionsBuilder::PushCountryPlacePointsFinders(
    RegionPlaceLot const & countryOuter)
{
  std::vector<std::future<PlacePointsMap>> tasks;

  auto i = begin(m_placePointsMap);
  while (i != end(m_placePointsMap))
  {
    auto from = i;
    int n = 0;
    constexpr auto taskBlockSize = 1000;
    while (i != end(m_placePointsMap) && n < taskBlockSize)
    {
      ++i;
      ++n;
    }

    tasks.push_back(m_threadPool.Submit(FindCountryPlacePointsForRange, std::cref(countryOuter), from, i));
  }

  return tasks;
}

RegionsBuilder::PlacePointsMap RegionsBuilder::FindCountryPlacePointsForRange(
    RegionPlaceLot const & countryOuters, PlacePointsMap::iterator begin, PlacePointsMap::iterator end)
{
  PlacePointsMap countryPoints;

  for (auto i = begin; i != end; ++i)
  {
    for (auto const & outer : countryOuters)
    {
      auto const & region = outer.GetRegion();
      if (region.Contains(i->second))
        countryPoints.insert(*i);
    }
  };

  return countryPoints;
}

void RegionsBuilder::UpdateStats(CountryStats & stats, Node::Ptr const & node)
{
  auto const & place = node->GetData();

  auto const level = place.GetLevel();
  if (level != ObjectLevel::Unknown)
    ++stats.objectLevelCounts[level];

  auto const placeType = place.GetPlaceType();
  if (placeType != PlaceType::Unknown)
    ++stats.placeCounts[placeType];

  auto const adminLevel = place.GetAdminLevel();
  if (adminLevel != AdminLevel::Unknown)
    UpdateStats(stats, adminLevel, node);
}

void RegionsBuilder::UpdateStats(CountryStats & stats, AdminLevel adminLevel, Node::Ptr const & node)
{
  auto const & place = node->GetData();

  auto & adminLevelStats = stats.adminLevels[adminLevel];
  ++adminLevelStats.count;

  auto const placeType = place.GetPlaceType();
  if (placeType != PlaceType::Unknown)
    ++adminLevelStats.placeCounts[placeType];

  for (auto const & subnode : node->GetChildren())
  {
    auto const & subplace = subnode->GetData();
    auto const subplaceType = subplace.GetPlaceType();
    if (subplaceType != PlaceType::Unknown)
      ++adminLevelStats.placeCounts[subplaceType];
  }
}

void RegionsBuilder::UpdateStats(CountryStats & stats, PlacePointsMap const & placePoints,
                                 PlacePointsMap const & unboudedPlacePoints)
{
  for (auto const & place : placePoints)
    ++stats.placePointsCounts[place.second.GetPlaceType()];

  for (auto const & place : unboudedPlacePoints)
    ++stats.unboudedPlacePointsCounts[place.second.GetPlaceType()];
}
}  // namespace regions
}  // namespace generator
