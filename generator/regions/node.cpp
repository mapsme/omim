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

NodePath MakeLevelPath(Node::Ptr const & node)
{
  CHECK(node->GetData().GetLevel() != ObjectLevel::Unknown, ());

  std::bitset<static_cast<std::size_t>(ObjectLevel::Sublocality) + 1> skipLevels;
  NodePath path{node};
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
