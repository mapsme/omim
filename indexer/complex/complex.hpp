#pragma once

#include "indexer/complex/hierarchy_entry.hpp"
#include "indexer/complex/tree_node.hpp"

#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace indexer
{
namespace complex
{
using Id = uint32_t;
}  // namespace complex

class SourceComplexesLoader
{
public:
  explicit SourceComplexesLoader(std::string const & filename);

  tree_node::Forest<indexer::HierarchyEntry> const & GetForest(std::string const & country) const;

  template <typename Fn>
  void ForEach(Fn && fn) const
  {
    for (auto const & pair : m_forests)
      fn(pair.first, pair.second);
  }

  std::unordered_set<indexer::CompositeId> GetIdsSet() const;

private:
  std::unordered_map<std::string, tree_node::Forest<indexer::HierarchyEntry>> m_forests;
};

bool IsComplex(tree_node::types::Ptr<indexer::HierarchyEntry> const & tree);

std::string GetCountry(tree_node::types::Ptr<indexer::HierarchyEntry> const & tree);

SourceComplexesLoader const & GetOrCreateSourceComplexesLoader(std::string const & filename);

template <typename Fn>
tree_node::Forest<complex::Id> TraformToComplexForest(
    tree_node::Forest<HierarchyEntry> const & forest, Fn && fn)
{
  tree_node::Forest<complex::Id> res;
  forest.ForEachTree([&](auto const & tree) {
    res.Append(tree_node::TransformToTree(tree, std::forward<Fn>(fn)));
  });
  return res;
}
}  // namespace indexer
