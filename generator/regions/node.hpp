#pragma once

#include "generator/place_node.hpp"
#include "generator/regions/region_place.hpp"

#include <iostream>

namespace generator
{
namespace regions
{
using Node = PlaceNode<RegionPlace>;
using NodePath = std::vector<Node::Ptr>;

size_t TreeSize(Node::Ptr const & node);

size_t MaxDepth(Node::Ptr const & node);

void DebugPrintTree(Node::Ptr const & tree, std::ostream & stream = std::cout);

// This function merges two trees if the roots have the same ids.
Node::Ptr MergeTree(Node::Ptr const & l, Node::Ptr const & r);

// This function corrects the tree. It traverses the whole node and unites children with
// the same ids.
void NormalizeTree(Node::Ptr & tree);

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
