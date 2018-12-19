#pragma once

#include "base/geo_object_id.hpp"
#include "base/macros.hpp"

#include <algorithm>
#include <utility>
#include <vector>

namespace base
{
// A data structure to perform the beam search with.
// Maintains a list of (Key, Value) pairs sorted in the decreasing
// order of Values.
template <typename TKey, typename TValue>
class Beam
{
public:
  using Key = TKey;
  using Value = TValue;

  struct Entry
  {
    Key m_key;
    Value m_value;

    Entry(Key const & key, Value const & value) : m_key(key), m_value(value) {}

    bool operator<(Entry const & rhs) const { return m_value > rhs.m_value; }
  };

  explicit Beam(size_t capacity) : m_capacity(capacity) { m_entries.reserve(m_capacity); }

  // Tries to add a key-value pair to the beam. Evicts the entry with the lowest
  // value if the insertion would result in an overspill.
  //
  // Complexity: O(log(m_capacity)) in the best case (a typical application has a considerable
  //                                fraction of calls that do not result in an eviction).
  //             O(m_capacity) in the worst case.
  void Add(Key const & key, Value const & value)
  {
    if (PREDICT_FALSE(m_capacity == 0))
      return;

    Entry const e(key, value);
    auto it = std::lower_bound(m_entries.begin(), m_entries.end(), e);

    if (it == m_entries.end())
    {
      if (m_entries.size() < m_capacity)
        m_entries.emplace_back(e);
      return;
    }

    if (m_entries.size() == m_capacity)
      m_entries.pop_back();

    m_entries.insert(it, e);
  }

  // template <typename Fn>
  // void ForEachEntry(Fn && fn) const
  // {
  //   for (Entry const & e : m_entries)
  //     fn(e.m_key, e.m_value);
  // }

  // todo(@m) Specify whether the entries are supposed to be sorted.
  std::vector<Entry> const & GetEntries() const { return m_entries; }

private:
  size_t m_capacity;
  std::vector<Entry> m_entries;
};

template <typename TKey, typename TValue>
class HeapBeam
{
public:
  using Key = TKey;
  using Value = TValue;

  struct Entry
  {
    Key m_key;
    Value m_value;

    Entry(Key const & key, Value const & value) : m_key(key), m_value(value) {}

    bool operator<(Entry const & rhs) const { return m_value > rhs.m_value; }
  };

  explicit HeapBeam(size_t capacity) : m_capacity(capacity), m_size(0)
  {
    m_entries.reserve(m_capacity);
  }

  // Tries to add a key-value pair to the beam. Evicts the entry with the lowest
  // value if the insertion would result in an overspill.
  //
  // Complexity: O(log(m_capacity)).
  void Add(Key const & key, Value const & value)
  {
    if (PREDICT_FALSE(m_capacity == 0))
      return;

    Entry const e(key, value);
    if (PREDICT_FALSE(m_size < m_capacity))
    {
      ++m_size;
      m_entries.emplace_back(e);
      std::push_heap(m_entries.begin(), m_entries.begin() + m_size);
      return;
    }

    ASSERT_GREATER(m_size, 0, ());
    if (m_entries.front() < e)
      return;

    std::pop_heap(m_entries.begin(), m_entries.begin() + m_size);
    m_entries[m_size - 1] = std::move(e);
    std::push_heap(m_entries.begin(), m_entries.begin() + m_size);
  }

  // template <typename Fn>
  // void ForEachEntry(Fn && fn) const
  // {
  //   for (Entry const & e : m_entries)
  //     fn(e.m_key, e.m_value);
  // }

  std::vector<Entry> const & GetEntries() const { return m_entries; }

private:
  size_t m_capacity;
  size_t m_size;
  std::vector<Entry> m_entries;
};
}  // namespace base
