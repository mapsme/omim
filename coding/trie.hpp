#pragma once

#include "base/assert.hpp"
#include "base/base.hpp"
#include "base/buffer_vector.hpp"

#include "std/unique_ptr.hpp"

namespace trie
{

typedef uint32_t TrieChar;

// 95 is a good value for the default baseChar, since both small and capital latin letters
// are less than +/- 32 from it and thus can fit into supershort edge.
// However 0 is used because the first byte is actually language id.
static uint32_t const DEFAULT_CHAR = 0;

template <typename TValue>
class Iterator
{
  //dbg::ObjectTracker m_tracker;

public:
  struct Edge
  {
    typedef buffer_vector<TrieChar, 8> EdgeStrT;
    EdgeStrT m_str;
  };

  buffer_vector<Edge, 8> m_edge;
  buffer_vector<TValue, 2> m_value;

  virtual ~Iterator() {}

  virtual unique_ptr<Iterator<TValue>> Clone() const = 0;
  virtual unique_ptr<Iterator<TValue>> GoToEdge(size_t i) const = 0;
};

struct EmptyValueReader
{
  typedef unsigned char ValueType;

  EmptyValueReader() = default;

  template <typename SourceT>
  void operator() (SourceT &, ValueType & value) const
  {
    value = 0;
  }
};

template <unsigned int N>
struct FixedSizeValueReader
{
  struct ValueType
  {
    unsigned char m_data[N];
  };

  template <typename SourceT>
  void operator() (SourceT & src, ValueType & value) const
  {
    src.Read(&value.m_data[0], N);
  }
};

template <typename TValue, typename F, typename TString>
void ForEachRef(Iterator<TValue> const & iter, F & f, TString const & s)
{
  for (size_t i = 0; i < iter.m_value.size(); ++i)
    f(s, iter.m_value[i]);
  for (size_t i = 0; i < iter.m_edge.size(); ++i)
  {
    TString s1(s);
    s1.insert(s1.end(), iter.m_edge[i].m_str.begin(), iter.m_edge[i].m_str.end());
    auto it = iter.GoToEdge(i);
    ForEachRef(*it, f, s1);
  }
}

}  // namespace Trie
