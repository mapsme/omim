#pragma once

#include "base/assert.hpp"

#include <cstddef>
#include <functional>
#include <queue>
#include <unordered_map>

template <typename Key, typename Value>
class FifoCache
{
  template <typename K, typename V> friend class FifoCacheTest;
public:
  using Loader = std::function<void(Key const & key, Value & value)>;

  FifoCache(size_t maxCacheSize, Loader const & loader)
  : m_maxCacheSize(maxCacheSize), m_loader(loader)
  {
    CHECK_GREATER(maxCacheSize, 0, ());
  }

  Value const & GetValue(Key const & key)
  {
    auto const it = m_cache.find(key);
    if (it != m_cache.cend())
      return it->second;

    ASSERT_EQUAL(m_fifoQueue.size(), m_cache.size(), ());
    if (m_cache.size() >= m_maxCacheSize)
    {
      ASSERT(!m_fifoQueue.empty(), ());
      m_cache.erase(m_fifoQueue.back());
      m_fifoQueue.pop_back();
    }

    m_fifoQueue.push_front(key);
    Value & value = m_cache[key];
    m_loader(key, value);
    return value;
  }

  /// \brief Checks for coherence class params.
  /// \note It's a time consumption method and should be called for tests only.
  bool IsValid() const
  {
    if (m_cache.size() != m_fifoQueue.size())
      return false;

    if (m_cache.size() > m_maxCacheSize)
      return false;

    for (auto const & k : m_fifoQueue)
    {
      if (m_cache.find(k) == m_cache.cend())
        return false;
    }
    return true;
  }

private:
  size_t const m_maxCacheSize;
  Loader const m_loader;
  std::unordered_map<Key, Value> m_cache;
  std::deque<Key> m_fifoQueue;
};
