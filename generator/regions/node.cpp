#include "generator/regions/node.hpp"

#include "geometry/mercator.hpp"

#include <algorithm>
#include <bitset>
#include <iomanip>
#include <numeric>

namespace generator
{
namespace regions
{
namespace
{
using MergeFunc = std::function<Node::Ptr(Node::Ptr const &, Node::Ptr const &)>;

bool LessNodePtrById(Node::Ptr const & l, Node::Ptr const & r)
{
  auto const & lPlace = l->GetData();
  auto const & rPlace = r->GetData();
  return lPlace.GetId() < rPlace.GetId();
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
  auto const pred = [](Node::Ptr const & l, Node::Ptr const & r)
  {
    auto const & lPlace = l->GetData();
    auto const & rPlace = r->GetData();
    return lPlace.GetId() == rPlace.GetId();
  };

  if (std::adjacent_find(std::begin(children), std::end(children), pred) == std::end(children))
    return children;

  Node::PtrList uniqueChildren;
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

Node::Ptr MergeHelper(Node::Ptr const & l, Node::Ptr const & r, MergeFunc mergeTree)
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

size_t TreeSize(Node::Ptr const & node)
{
  if (node == nullptr)
    return 0;

  size_t size = 1;
  for (auto const & n : node->GetChildren())
    size += TreeSize(n);

  return size;
}

size_t MaxDepth(Node::Ptr const & node)
{
  if (node == nullptr)
    return 0;

  size_t depth = 1;
  for (auto const & n : node->GetChildren())
    depth = std::max(MaxDepth(n), depth);

  return depth;
}

void PrintTree(Node::Ptr const & node, std::ostream & stream = std::cout, std::string prefix = "",
               bool isTail = true)
{
  auto const & place = node->GetData();
  if (auto label = GetLabel(place.GetLevel()))
  {
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

    auto const point = place.GetCenter();
    auto const center = MercatorBounds::ToLatLon({point.get<0>(), point.get<1>()});
    stream << place.GetName() << "<" << place.GetEnglishOrTransliteratedName() << "> ("
           << DebugPrint(place.GetId())
           << ";" << label
           << ";" << static_cast<int>(place.GetLevel())
           << ";[" << std::fixed << std::setprecision(7) << center.lat << "," << center.lon << "])"
           << std::endl;
  }

  auto const & children = node->GetChildren();
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

Node::Ptr MergeTree(Node::Ptr const & l, Node::Ptr const & r)
{
  if (l == nullptr)
    return r;

  if (r == nullptr)
    return l;

  auto const & lRegion = l->GetData().GetRegion();
  auto const & rRegion = r->GetData().GetRegion();

  if (lRegion.GetArea() > rRegion.GetArea())
    return MergeHelper(l, r, MergeTree);
  else
    return MergeHelper(r, l, MergeTree);
}

void NormalizeTree(Node::Ptr & tree)
{
  if (tree == nullptr)
    return;

  auto & children = tree->GetChildren();
  std::sort(std::begin(children), std::end(children), LessNodePtrById);
  auto newChildren = NormalizeChildren(children, MergeTree);
  tree->SetChildren(std::move(newChildren));
  for (auto & ch : tree->GetChildren())
    NormalizeTree(ch);
}

std::vector<Node::Ptr> MakeLevelPath(Node::Ptr const & node)
{
  CHECK(node->GetData().GetLevel() != ObjectLevel::Unknown, ());

  std::bitset<static_cast<std::size_t>(ObjectLevel::Sublocality) + 1> skipLevels;
  std::vector<Node::Ptr> path{node};
  for (auto p = node->GetParent(); p; p = p->GetParent())
  {
    auto const level = p->GetData().GetLevel();
    if (ObjectLevel::Unknown == level)
      continue;

    if (skipLevels.test(static_cast<std::size_t>(level)))
      continue;

    skipLevels.set(static_cast<std::size_t>(level), true);
    if (ObjectLevel::Locality == level)
    {
      // To ignore covered locality.
      skipLevels.set(static_cast<std::size_t>(ObjectLevel::Suburb), true);
      skipLevels.set(static_cast<std::size_t>(ObjectLevel::Sublocality), true);
    }

    path.push_back(p);
  }

  std::reverse(path.begin(), path.end());
  std::sort(path.begin(), path.end(), [] (Node::Ptr const & l, Node::Ptr const & r) {
    return l->GetData().GetLevel() < r->GetData().GetLevel();
  });
  return path;
}
}  // namespace regions
}  // namespace generator
