#pragma once

#include "base/geo_object_id.hpp"

#include <vector>

namespace geocoder
{
// todo(@m) Probably make it a template and move to base/. Also add tests.
//          Template the Less too?
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

    Entry(Key const & key, Value value) : m_key(key), m_value(value) {}

    bool operator<(Entry const & rhs) const { return m_value > rhs.m_value; }
  };

  explicit Beam(size_t capacity) : m_capacity(capacity) { m_entries.reserve(m_capacity); }

  // O(log(n) + n) for |n| entries.
  // O(|m_capacity|) in the worst case.
  void Add(Key const & key, Value value)
  {
    if (m_capacity == 0)
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

  std::vector<Entry> const & GetEntries() const { return m_entries; }

private:
  size_t m_capacity;
  std::vector<Entry> m_entries;
};
}  // namespace geocoder
