#pragma once

#include <algorithm>
#include <memory>
#include <sstream>
#include <utility>
#include <vector>

#include "base/control_flow.hpp"
#include "base/stl_helpers.hpp"

namespace tree_node
{
template <typename Data>
class TreeNode;

namespace types
{
template <typename Data>
using Ptr = std::shared_ptr<TreeNode<Data>>;

template <typename Data>
using WeakPtr = std::weak_ptr<TreeNode<Data>>;

template <typename Data>
using PtrList = std::vector<Ptr<Data>>;
}  // namespace types

template <typename Data>
class TreeNode
{
public:
  using Ptr = types::Ptr<Data>;
  using WeakPtr = types::WeakPtr<Data>;
  using PtrList = types::PtrList<Data>;

  explicit TreeNode(Data && data) : m_data(std::move(data)) {}

  bool HasChildren() const { return !m_children.empty(); }
  PtrList const & GetChildren() const { return m_children; }
  void AddChild(Ptr const & child) { m_children.push_back(child); }
  void AddChildren(PtrList && children)
  {
    std::move(std::begin(children), std::end(children), std::end(m_children));
  }

  void SetChildren(PtrList && children) { m_children = std::move(children); }
  void RemoveChildren() { m_children.clear(); }

  bool HasParent() const { return m_parent.lock() != nullptr; }
  Ptr GetParent() const { return m_parent.lock(); }
  void SetParent(Ptr const & parent) { m_parent = parent; }

  Data & GetData() { return m_data; }
  Data const & GetData() const { return m_data; }

private:
  Data m_data;
  PtrList m_children;
  WeakPtr m_parent;
};

template <typename Data>
decltype(auto) MakeTreeNode(Data && data)
{
  return std::make_shared<TreeNode<Data>>(std::forward<Data>(data));
}

template <typename Data>
class Forest
{
public:
  void Append(types::Ptr<Data> const & tree) { m_trees.emplace_back(tree); }

  size_t Size() const { return m_trees.size(); }

  template <typename Fn>
  void ForEachTree(Fn && fn) const
  {
    std::for_each(std::cbegin(m_trees), std::cend(m_trees), std::forward<Fn>(fn));
  }

private:
  types::PtrList<Data> m_trees;
};

template <typename Data>
void Link(types::Ptr<Data> const & node, types::Ptr<Data> const & parent)
{
  parent->AddChild(node);
  node->SetParent(parent);
}

template <typename Data>
size_t GetDepth(types::Ptr<Data> node)
{
  size_t depth = 0;
  while (node)
  {
    node = node->GetParent();
    ++depth;
  }
  return depth;
}

template <typename Data, typename Fn>
void PreOrderVisit(types::Ptr<Data> const & node, Fn && fn)
{
  base::ControlFlowWrapper<Fn> wrapper(std::forward<Fn>(fn));
  std::function<void(types::Ptr<Data> const &)> preOrderVisitDetail;
  preOrderVisitDetail = [&](auto const & node) {
    if (wrapper(node) == base::ControlFlow::Break)
      return;

    for (auto const & ch : node->GetChildren())
      preOrderVisitDetail(ch);
  };
  preOrderVisitDetail(node);
}

template <typename Data, typename Fn>
void ForEach(types::Ptr<Data> const & node, Fn && fn)
{
  PreOrderVisit(node, [&](auto const & node) {
    fn(node->GetData());
  });
}

template <typename Data, typename Fn>
size_t CountIf(types::Ptr<Data> const & node, Fn && fn)
{
  size_t count = 0;
  PreOrderVisit(node, [&](auto const & node) {
    if (fn(node->GetData()))
      ++count;
  });
  return count;
}

template <typename Data, typename Fn>
decltype(auto) FindIf(types::Ptr<Data> const & node, Fn && fn)
{
  types::Ptr<Data> res;
  PreOrderVisit(node, [&](auto const & node) {
    if (fn(node->GetData()))
    {
      res = node;
      return base::ControlFlow::Break;
    }
    return base::ControlFlow::Continue;
  });
  return res;
}

template <typename Data>
size_t Size(types::Ptr<Data> const & node)
{
  size_t size = 0;
  PreOrderVisit(node, [&](auto const &) { ++size; });
  return size;
}

template <typename Data>
void Print(types::Ptr<Data> const & node, std::ostream & stream,
           std::string prefix = "", bool isTail = true)
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
  auto str = DebugPrint(node->GetData());
  std::replace(std::begin(str), std::end(str), '\n', '|');
  stream << str << '\n';
  auto const & children = node->GetChildren();
  size_t size = children.size();
  for (size_t i = 0; i < size; ++i)
    Print(children[i], stream, prefix, i == size - 1 /* isTail */);
}

template <typename Data>
std::string DebugPrint(types::Ptr<Data> const & node)
{
  std::stringstream stream;
  Print(node, stream);
  return stream.str();
}
}  // namespace tree_node
