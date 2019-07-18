#include "generator/regions/node.hpp"

#include "geometry/mercator.hpp"

#include <array>
#include <algorithm>
#include <iomanip>
#include <numeric>

namespace generator
{
namespace regions
{
namespace
{
using MergeFunc = std::function<Node::Ptr(Node::Ptr, Node::Ptr)>;

bool LessNodePtrById(Node::Ptr l, Node::Ptr r)
{
  auto const & lRegion = l->GetData();
  auto const & rRegion = r->GetData();
  return lRegion.GetId() < rRegion.GetId();
}

Node::PtrList MergeChildren(Node::PtrList const & l, Node::PtrList const & r, Node::Ptr newParent)
{
  Node::PtrList result(l);
  std::copy(std::begin(r), std::end(r), std::back_inserter(result));
  for (auto & p : result)
    p->SetParent(newParent);

  std::sort(std::begin(result), std::end(result), LessNodePtrById);
  return result;
}

Node::PtrList NormalizeChildren(Node::PtrList const & children, MergeFunc mergeTree)
{
  Node::PtrList uniqueChildren;
  auto const pred = [](Node::Ptr l, Node::Ptr r)
  {
    auto const & lRegion = l->GetData();
    auto const & rRegion = r->GetData();
    return lRegion.GetId() == rRegion.GetId();
  };
  std::unique_copy(std::begin(children), std::end(children),
                   std::back_inserter(uniqueChildren), pred);
  Node::PtrList result;
  for (auto const & ch : uniqueChildren)
  {
    auto const bounds = std::equal_range(std::begin(children), std::end(children), ch,
                                         LessNodePtrById);
    auto merged = std::accumulate(bounds.first, bounds.second, Node::Ptr(), mergeTree);
    result.emplace_back(std::move(merged));
  }

  return result;
}

Node::Ptr MergeHelper(Node::Ptr l, Node::Ptr r, MergeFunc mergeTree)
{
  auto const & lChildren = l->GetChildren();
  auto const & rChildren = r->GetChildren();
  auto const children = MergeChildren(lChildren, rChildren, l);
  if (children.empty())
    return l;

  auto resultChildren = NormalizeChildren(children, mergeTree);
  l->SetChildren(std::move(resultChildren));
  l->ShrinkToFitChildren();
  r->RemoveChildren();
  r->ShrinkToFitChildren();
  return l;
}
}  // nmespace

size_t TreeSize(Node::Ptr node)
{
  if (node == nullptr)
    return 0;

  size_t size = 1;
  for (auto const & n : node->GetChildren())
    size += TreeSize(n);

  return size;
}

size_t MaxDepth(Node::Ptr node)
{
  if (node == nullptr)
    return 0;

  size_t depth = 1;
  for (auto const & n : node->GetChildren())
    depth = std::max(MaxDepth(n), depth);

  return depth;
}

NodePath MakeLevelPath(Node::Ptr const & node)
{
  CHECK(node->GetData().GetLevel() != PlaceLevel::Unknown, ());

  std::array<bool, static_cast<std::size_t>(PlaceLevel::Count)> skipLevels{};
  NodePath path{node};
  for (auto p = node->GetParent(); p; p = p->GetParent())
  {
    auto const level = p->GetData().GetLevel();
    if (PlaceLevel::Unknown == level)
      continue;

    auto levelIndex = static_cast<std::size_t>(level);
    if (skipLevels.at(levelIndex))
      continue;

    skipLevels[levelIndex] = true;
    if (PlaceLevel::Locality == level)
    {
      // To ignore covered locality.
      skipLevels[static_cast<std::size_t>(PlaceLevel::Suburb)] = true;
      skipLevels[static_cast<std::size_t>(PlaceLevel::Sublocality)] = true;
    }

    path.push_back(p);
  }
  std::reverse(path.begin(), path.end());

  // Sort by level in case that megapolis (PlaceLevel::Locality) contains subregions
  // (PlaceLevel::Subregions).
  std::sort(path.begin(), path.end(), [] (Node::Ptr const & l, Node::Ptr const & r) {
    return l->GetData().GetLevel() < r->GetData().GetLevel();
  });
  return path;
}

void PrintTree(Node::Ptr node, std::ostream & stream = std::cout, std::string prefix = "",
               bool isTail = true)
{
  auto const & children = node->GetChildren();
  stream << prefix;
  if (isTail)
  {
    stream << "└───";
    prefix += "    ";
  }
  else
  {
    stream << "├───";
    prefix += "│   ";
  }

  auto const & d = node->GetData();
  auto const point = d.GetCenter();
  auto const center = MercatorBounds::ToLatLon({point.get<0>(), point.get<1>()});
  auto const label = GetLabel(d.GetLevel());
  stream << d.GetName() << "<" << d.GetEnglishOrTransliteratedName() << "> ("
         << DebugPrint(d.GetId())
         << ";" << (label ? label : "-")
         << ";" << static_cast<size_t>(d.GetRank())
         << ";[" << std::fixed << std::setprecision(7) << center.m_lat << "," << center.m_lon << "])"
         << std::endl;
  for (size_t i = 0, size = children.size(); i < size; ++i)
    PrintTree(children[i], stream, prefix, i == size - 1);
}

void DebugPrintTree(Node::Ptr const & tree, std::ostream & stream)
{
  stream << "ROOT NAME: " << tree->GetData().GetName() << std::endl;
  stream << "MAX DEPTH: " <<  MaxDepth(tree) << std::endl;
  stream << "TREE SIZE: " <<  TreeSize(tree) << std::endl;
  PrintTree(tree, stream);
  stream << std::endl;
}

Node::Ptr MergeTree(Node::Ptr l, Node::Ptr r)
{
  if (l == nullptr)
    return r;

  if (r == nullptr)
    return l;

  auto const & lRegion = l->GetData();
  auto const & rRegion = r->GetData();
  if (lRegion.GetId() != rRegion.GetId())
    return nullptr;

  if (lRegion.GetArea() > rRegion.GetArea())
    return MergeHelper(l, r, MergeTree);
  else
    return MergeHelper(r, l, MergeTree);
}

void NormalizeTree(Node::Ptr tree)
{
  if (tree == nullptr)
    return;

  auto & children = tree->GetChildren();
  std::sort(std::begin(children), std::end(children), LessNodePtrById);
  auto newChildren = NormalizeChildren(children, MergeTree);
  tree->SetChildren(std::move(newChildren));
  for (auto const & ch : tree->GetChildren())
    NormalizeTree(ch);
}
}  // namespace regions
}  // namespace generator
