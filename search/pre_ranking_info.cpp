#include "search/ranking_info.hpp"

#include "std/sstream.hpp"

namespace search
{
string DebugPrint(PreRankingInfo const & info)
{
  ostringstream os;
  os << "PreRankingInfo [";
  os << "m_distanceToPivot:" << info.m_distanceToPivot << ",";
  os << "m_tokenRange:" << DebugPrint(info.m_tokenRange) << ",";
  os << "m_rank:" << info.m_rank << ",";
  os << "m_searchType:" << info.m_searchType;
  os << "]";
  return os.str();
}

}  // namespace search
