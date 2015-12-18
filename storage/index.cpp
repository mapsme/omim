#include "storage/index.hpp"

#include "std/sstream.hpp"


namespace storage
{
  const string TIndex::INVALID = string("");
  string DebugPrint(TIndex const & r)
  {
    return r.m_idx;
  }
}
