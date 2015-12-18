#pragma once
#include "std/string.hpp"


namespace storage
{
  struct TIndex
  {
    string m_idx;
    static string const INVALID;
    TIndex(string const & idx) : m_idx(idx) {}
    TIndex() : m_idx(INVALID) {}
    bool operator==(TIndex const & other) const { return m_idx == other.m_idx; }
    bool IsValid() const { return m_idx != INVALID; }
     
    bool operator!=(TIndex const & other) const
    {
      return !(*this == other);
    }

    bool operator<(TIndex const & other) const
    {
      return m_idx.compare(other.m_idx) > 0;
    }
  };

  string DebugPrint(TIndex const & r);
}
