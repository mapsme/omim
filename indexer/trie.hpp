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

template <typename TValueList>
class Iterator
{
  //dbg::ObjectTracker m_tracker;

public:
  using TValue = typename TValueList::TValue;

  struct Edge
  {
    using TEdgeLabel = buffer_vector<TrieChar, 8>;
    TEdgeLabel m_label;
  };

  buffer_vector<Edge, 8> m_edge;
  TValueList m_valueList;

  virtual ~Iterator() {}

  virtual unique_ptr<Iterator<TValueList>> Clone() const = 0;
  virtual unique_ptr<Iterator<TValueList>> GoToEdge(size_t i) const = 0;
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

template <typename TValueList, typename TF, typename TString>
void ForEachRef(Iterator<TValueList> const & it, TF && f, TString const & s)
{
  it.m_valueList.ForEach([&f, &s](typename TValueList::TValue const & value)
                         {
                           f(s, value);
                         });
  for (size_t i = 0; i < it.m_edge.size(); ++i)
  {
    TString s1(s);
    s1.insert(s1.end(), it.m_edge[i].m_label.begin(), it.m_edge[i].m_label.end());
    auto nextIt = it.GoToEdge(i);
    ForEachRef(*nextIt, f, s1);
  }
}
}  // namespace trie
