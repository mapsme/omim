#include "indexer/complex/complex.hpp"

#include "indexer/classificator.hpp"

#include "coding/csv_reader.hpp"

#include "base/logging.hpp"
#include "base/stl_helpers.hpp"

#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace
{
size_t const kNumRequiredTypes = 3;

std::unordered_set<uint32_t> GetRequiredComplexTypes()
{
  std::vector<std::vector<std::string>> const kRequiredComplexPaths =
  {
    // Sights
    {"amenity", "place_of_worship"},
    {"historic", "archaeological_site"},
    {"historic", "boundary_stone"},
    {"historic", "castle"},
    {"historic", "fort"},
    {"historic", "memorial"},
    {"historic", "monument"},
    {"historic", "ruins"},
    {"historic", "ship"},
    {"historic", "tomb"},
    {"historic", "wayside_cross"},
    {"historic", "wayside_shrine"},
    {"tourism", "artwork"},
    {"tourism", "attraction"},
    {"tourism", "theme_park"},
    {"tourism", "viewpoint"},
    {"waterway", "waterfall"},

    // Museum
    {"amenity", "arts_centre"},
    {"tourism", "gallery"},
    {"tourism", "museum"},
  };

  std::unordered_set<uint32_t> requiredComplexTypes;
  requiredComplexTypes.reserve(kRequiredComplexPaths.size());
  for (auto const & path : kRequiredComplexPaths)
    requiredComplexTypes.emplace(classif().GetTypeByPath(path));
  return requiredComplexTypes;
}
}  // namespace

namespace indexer
{
bool IsComplex(tree_node::types::Ptr<indexer::HierarchyEntry> const & tree)
{
  static auto const kTypes = GetRequiredComplexTypes();
  return tree_node::CountIf(tree, [&](auto const & e) {
    return kTypes.count(e.m_type) != 0;
  }) >= kNumRequiredTypes;
}

std::string GetCountry(tree_node::types::Ptr<indexer::HierarchyEntry> const & tree)
{
  return tree->GetData().m_countryName;
}

SourceComplexesLoader::SourceComplexesLoader(std::string const & filename)
{
  auto trees = hierarchy::LoadHierachy(filename);
  base::EraseIf(trees, base::NotFn(IsComplex));
  for (auto const & tree : trees)
    m_forests[GetCountry(tree)].Append(tree);
}

tree_node::Forest<indexer::HierarchyEntry> const & SourceComplexesLoader::GetForest(std::string const & country) const
{
  static tree_node::Forest<indexer::HierarchyEntry> const kEmpty;
  auto const it = m_forests.find(country);
  return it == std::cend(m_forests) ? kEmpty : it->second;
}

std::unordered_set<indexer::CompositeId> SourceComplexesLoader::GetIdsSet() const
{
  std::unordered_set<indexer::CompositeId> set;
  ForEach([&](auto const &, auto const & forest) {
    forest.ForEachTree([&] (auto const & tree) {
      tree_node::ForEach(tree, [&](auto const & entry) {
        set.emplace(entry.m_id);
      });
    });
  });
  return set;
}

SourceComplexesLoader const & GetOrCreateSourceComplexesLoader(std::string const & filename)
{
  static std::mutex m;
  static std::unordered_map<std::string, SourceComplexesLoader> complexLoaders;

  std::lock_guard<std::mutex> lock(m);
  auto const it = complexLoaders.find(filename);
  if (it != std::cend(complexLoaders))
    return it->second;

  auto const eIt = complexLoaders.emplace(filename, SourceComplexesLoader(filename));
  return eIt.first->second;
}
}  // namespace indexer
