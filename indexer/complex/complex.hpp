#pragma once

#include "indexer/complex/hierarchy_entry.hpp"
#include "indexer/complex/tree_node.hpp"

#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace indexer
{
bool IsComplex(tree_node::types::Ptr<indexer::HierarchyEntry> const & tree);

std::string GetCountry(tree_node::types::Ptr<indexer::HierarchyEntry> const & tree);

class ComplexLoader
{
public:
  explicit ComplexLoader(std::string const & filename);

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

ComplexLoader const & GetOrCreateComplexLoader(std::string const & filename);
}  // namespace indexer
