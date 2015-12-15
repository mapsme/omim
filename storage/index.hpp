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

//    static int const INVALID;

//    int m_group;
//    int m_country;
//    int m_region;

//    TIndex(int group = INVALID, int country = INVALID, int region = INVALID)
//      : m_group(group), m_country(country), m_region(region) {}

//    bool IsValid() const { return (m_group != INVALID && m_country != INVALID); }

//    bool operator==(TIndex const & other) const
//    {
//      return (m_group == other.m_group &&
//              m_country == other.m_country &&
//              m_region == other.m_region);
//    }
     
    bool operator!=(TIndex const & other) const
    {
      return !(*this == other);
    }

    bool operator<(TIndex const & other) const
    {
      return m_idx.compare(other.m_idx) > 0;
    }

//    bool operator<(TIndex const & other) const
//    {
//      if (m_group != other.m_group)
//        return m_group < other.m_group;
//      else if (m_country != other.m_country)
//        return m_country < other.m_country;
//      return m_region < other.m_region;
//    }
  };

  string DebugPrint(TIndex const & r);
}
