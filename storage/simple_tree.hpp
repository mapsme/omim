#pragma once

#include "base/assert.hpp"

#include "std/algorithm.hpp"
#include "std/utility.hpp"
#include "std/vector.hpp"

template <class T>
class SimpleTree
{
  typedef std::vector<SimpleTree<T> > internal_container_type;

  T m_value;
  // @TODO(bykoianko) According to the logic the member and methods should be called m_children. Rename it.
  // @TODO(bykoianko) Remove on unnecessary methods of SimpleTree if any.
  internal_container_type m_siblings;

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
    while (level-- > 0 && !node->m_siblings.empty())
      node = &node->m_siblings.back();
    return node->Add(value);
  }

  /// @return reference is valid only up to the next tree structure modification
  T & Add(T const & value)
  {
    m_siblings.push_back(SimpleTree(value));
    return m_siblings.back().Value();
  }

  /// Deletes all children and makes tree empty
  void Clear()
  {
    m_siblings.clear();
  }

  bool operator<(SimpleTree<T> const & other) const
  {
    return Value() < other.Value();
  }

  /// sorts siblings independently on each level by default
  void Sort(bool onlySiblings = false)
  {
    std::sort(m_siblings.begin(), m_siblings.end());
    if (!onlySiblings)
      for (typename internal_container_type::iterator it = m_siblings.begin(); it != m_siblings.end(); ++it)
        it->Sort(false);
  }

  /// \brief Checks all nodes in tree to find an equal one. If there're several equal nodes
  /// returns the first found.
  /// \returns a poiter item in the tree if found and nullptr otherwise.
  /// @TODO(bykoianko) The complexity of the method is O(n). But the structure (tree) is built on the start of the program
  /// and then actively used on run time. This method (and class) should be redesigned to make the function work faster.
  /// A hash table is being planned to use.
  SimpleTree<T> const * const Find(T const & value) const
  {
    if (!(m_value < value) && !(value < m_value))
      return this;

    for (auto const & child : m_siblings)
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
    if (!(m_value < value) && !(value < m_value) && m_siblings.empty())
      return this; // It's a leaf.

    for (auto const & child : m_siblings)
    {
      SimpleTree<T> const * const found = child.FindLeaf(value);
      if (found != nullptr)
        return found;
    }
    return nullptr;
  }

  SimpleTree<T> const & At(size_t index) const
  {
    return m_siblings.at(index);
  }

  size_t SiblingsCount() const
  {
    return m_siblings.size();
  }

  template <class TFunctor>
  void ForEachSibling(TFunctor & f)
  {
    for (typename internal_container_type::iterator it = m_siblings.begin(); it != m_siblings.end(); ++it)
      f(*it);
  }

  template <class TFunctor>
  void ForEachSibling(TFunctor & f) const
  {
    for (typename internal_container_type::const_iterator it = m_siblings.begin(); it != m_siblings.end(); ++it)
      f(*it);
  }

  template <class TFunctor>
  void ForEachChildren(TFunctor & f)
  {
    for (typename internal_container_type::iterator it = m_siblings.begin(); it != m_siblings.end(); ++it)
    {
      f(*it);
      it->ForEachChildren(f);
    }
  }

  template <class TFunctor>
  void ForEachChildren(TFunctor & f) const
  {
    for (typename internal_container_type::const_iterator it = m_siblings.begin(); it != m_siblings.end(); ++it)
    {
      f(*it);
      it->ForEachChildren(f);
    }
  }
};
