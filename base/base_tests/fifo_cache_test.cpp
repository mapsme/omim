#include "testing/testing.hpp"

#include "base/fifo_cache.hpp"

#include <deque>
#include <unordered_map>

using namespace std;

template <typename Key, typename Value>
class FifoCacheTest
{
public:
  FifoCacheTest(size_t maxCacheSize, typename FifoCache<Key, Value>::Loader const & loader)
      : m_cache(maxCacheSize, loader)
  {
  }

  Value const & GetValue(Key const & key) { return m_cache.GetValue(key); }
  bool IsValid() const { return m_cache.IsValid(); }
  unordered_map<Key, Value> const & GetCache() const { return m_cache.m_cache; }
  deque<Key> const & GetFifoQueue() const { return m_cache.m_fifoQueue; }

private:
  FifoCache<Key, Value> m_cache;
};

UNIT_TEST(FifoCacheSmokeTest)
{
  using Key = int;
  using Value = int;
  FifoCache<Key, Value> cache(3 /* maxCacheSize */, [](Key k, Value & v) { v = k; } /* loader */);

  TEST_EQUAL(cache.GetValue(1), 1, ());
  TEST_EQUAL(cache.GetValue(2), 2, ());
  TEST_EQUAL(cache.GetValue(3), 3, ());
  TEST_EQUAL(cache.GetValue(4), 4, ());
  TEST_EQUAL(cache.GetValue(1), 1, ());
  TEST(cache.IsValid(), ());
}

UNIT_TEST(FifoCacheTest)
{
  using Key = int;
  using Value = int;
  FifoCacheTest<Key, Value> cache(3 /* maxCacheSize */, [](Key k, Value & v) { v = k; } /* loader */);

  TEST_EQUAL(cache.GetValue(1), 1, ());
  TEST_EQUAL(cache.GetValue(3), 3, ());
  TEST_EQUAL(cache.GetValue(2), 2, ());
  TEST(cache.IsValid(), ());
  {
    unordered_map<Key, Value> expectedCache({{1 /* key */, 1 /* value */}, {2, 2}, {3, 3}});
    TEST_EQUAL(cache.GetCache(), expectedCache, ());
    deque<Key> fifoQueue({2, 3, 1});
    TEST_EQUAL(cache.GetFifoQueue(), fifoQueue, ());
  }

  TEST_EQUAL(cache.GetValue(7), 7, ());
  TEST(cache.IsValid(), ());
  {
    unordered_map<Key, Value> expectedCache({{7 /* key */, 7 /* value */}, {2, 2}, {3, 3}});
    TEST_EQUAL(cache.GetCache(), expectedCache, ());
    deque<Key> fifoQueue({7, 2, 3});
    TEST_EQUAL(cache.GetFifoQueue(), fifoQueue, ());
  }
}
