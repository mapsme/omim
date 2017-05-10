#pragma once
#include "base/assert.hpp"

#include <vector>
#include <map>
#include <iostream>


class IndexAndTypeMapping
{
  std::vector<uint32_t> m_types;

  typedef std::map<uint32_t, uint32_t> MapT;
  MapT m_map;

  void Add(uint32_t ind, uint32_t type);

public:
  void Clear();
  void Load(std::istream & s);

  uint32_t GetType(uint32_t ind) const
  {
    ASSERT_LESS ( ind, m_types.size(), () );
    return m_types[ind];
  }

  uint32_t GetIndex(uint32_t t) const;

  /// For Debug purposes only.
  bool HasIndex(uint32_t t) const { return (m_map.find(t) != m_map.end()); }
};
