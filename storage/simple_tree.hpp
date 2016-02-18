#pragma once

#include "base/assert.hpp"

#include "std/algorithm.hpp"
#include "std/vector.hpp"

template <class T>
class SimpleTree
{
  T m_value;
  // @TODO(bykoianko) Remove on unnecessary methods of SimpleTree if any.

  /// \brief m_children contains all first generation descendants of the node.
  vector<SimpleTree<T>> m_children;

  static bool IsEqual(T const & v1, T const & v2)
  {
    return !(v1 < v2) && !(v2 < v1);
  }

public:
  SimpleTree(T const & value = T()) : m_value(value)
  {
  }

  /// @return reference is valid only up to the next tree structure modification
  T const & Value() const
  {
    return m_value;
  }

  /// @return reference is valid only up to the next tree structure modification
  T & Value()
  {
    return m_value;
  }

  /// @return reference is valid only up to the next tree structure modification
  T & AddAtDepth(int level, T const & value)
  {
    SimpleTree<T> * node = this;
    while (level-- > 0 && !node->m_children.empty())
      node = &node->m_children.back();
    return node->Add(value);
  }

  /// @return reference is valid only up to the next tree structure modification
  T & Add(T const & value)
  {
    m_children.emplace_back(SimpleTree(value));
    return m_children.back().Value();
  }

  /// Deletes all children and makes tree empty
  void Clear()
  {
    m_children.clear();
  }

  bool operator<(SimpleTree<T> const & other) const
  {
    return Value() < other.Value();
  }

  /// sorts children independently on each level by default
  void Sort()
  {
    sort(m_children.begin(), m_children.end());
    for (auto & child : m_children)
      child.Sort();
  }

  /// \brief Checks all nodes in tree to find an equal one. If there're several equal nodes
  /// returns the first found.
  /// \returns a poiter item in the tree if found and nullptr otherwise.
  /// @TODO(bykoianko) The complexity of the method is O(n). But the structure (tree) is built on the start of the program
  /// and then actively used on run time. This method (and class) should be redesigned to make the function work faster.
  /// A hash table is being planned to use.
  SimpleTree<T> const * const Find(T const & value) const
  {
    if (IsEqual(m_value, value))
      return this;

    for (auto const & child : m_children)
    {
      SimpleTree<T> const * const found = child.Find(value);
      if (found != nullptr)
        return found;
    }
    return nullptr;
  }

  /// \brief Find only leaves.
  /// \note It's a termprary fucntion for compatablity with old countries.txt.
  /// When new countries.txt with unique ids will be added FindLeaf will be removed
  /// and Find will be used intead.
  /// @TODO(bykoianko) Remove this method on countries.txt update.
  SimpleTree<T> const * const FindLeaf(T const & value) const
  {
    if (IsEqual(m_value, value) && m_children.empty())
      return this; // It's a leaf.

    for (auto const & child : m_children)
    {
      SimpleTree<T> const * const found = child.FindLeaf(value);
      if (found != nullptr)
        return found;
    }
    return nullptr;
  }

  SimpleTree<T> const & Child(size_t index) const
  {
    return m_children.at(index);
  }

  size_t ChildrenCount() const
  {
    return m_children.size();
  }

  /// \brief Calls functor f for each first generation descendant of the node.
  template <class TFunctor>
  void ForEachChild(TFunctor && f)
  {
    for (auto const & child : m_children)
      f(child);
  }

  template <class TFunctor>
  void ForEachChild(TFunctor && f) const
  {
    for (auto const & child : m_children)
      f(child);
  }

  /// \brief Calls functor f for all nodes (add descendant) in the tree.
  template <class TFunctor>
  void ForEachDescendant(TFunctor && f)
  {
    for (auto & child : m_children)
    {
      f(child);
      child.ForEachDescendant(f);
    }
  }

  template <class TFunctor>
  void ForEachDescendant(TFunctor && f) const
  {
    for (auto const & child: m_children)
    {
      f(child);
      child.ForEachDescendant(f);
    }
  }

  template <class TFunctor>
  void ForEachInSubtree(TFunctor && f)
  {
    f(*this);
    for (auto const & child: m_children)
      child.ForEachInSubtree(f);
  }

  template <class TFunctor>
  void ForEachInSubtree(TFunctor && f) const
  {
    f(*this);
    for (auto const & child: m_children)
      child.ForEachInSubtree(f);
  }
};
