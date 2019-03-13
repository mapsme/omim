#pragma once

#include "generator/place_node.hpp"
#include "generator/regions/region_place.hpp"

#include <iostream>

namespace generator
{
namespace regions
{
class LevelPlace : public RegionPlace
{
public:
  LevelPlace(ObjectLevel level, RegionPlace const & regionPlace)
    : RegionPlace(regionPlace), m_level{level}
  { }

  ObjectLevel GetLevel() const noexcept { return m_level; }
  void SetLevel(ObjectLevel level) { m_level = level; }

private:
  ObjectLevel m_level;
};

using Node = PlaceNode<LevelPlace>;
using NodePath = std::vector<Node::Ptr>;

size_t TreeSize(Node::Ptr const & node);

size_t MaxDepth(Node::Ptr const & node);

void DebugPrintTree(Node::Ptr const & tree, std::ostream & stream = std::cout);

NodePath MakeLevelPath(Node::Ptr const & node);

template <typename Func>
void ForEachLevelPath(Node::Ptr const & tree, Func && func)
{
  if (!tree)
    return;

  if (tree->GetData().GetLevel() != ObjectLevel::Unknown)
    func(MakeLevelPath(tree));

  for (auto const & subtree : tree->GetChildren())
    ForEachLevelPath(subtree, std::forward<Func>(func));
}
}  // namespace regions
}  // namespace generator
