#include "search/tracer.hpp"

#include "base/stl_helpers.hpp"

#include <cstddef>
#include <iomanip>
#include <sstream>

using namespace std;

namespace search
{
// Tracer::Parse -----------------------------------------------------------------------------------
Tracer::Parse::Parse(vector<TokenType> const & types, bool category) : m_category(category)
{
  size_t i = 0;
  while (i != types.size())
  {
    auto const type = types[i];
    auto j = i + 1;
    while (j != types.size() && types[j] == type)
      ++j;
    m_ranges[type] = TokenRange(i, j);
    i = j;
  }
}

Tracer::Parse::Parse(vector<pair<TokenType, TokenRange>> const & ranges, bool category)
  : m_category(category)
{
  for (auto const & kv : ranges)
    m_ranges[kv.first] = kv.second;
}

string DebugPrint(Tracer::Parse const & parse)
{
  using TokenType = Tracer::Parse::TokenType;

  ostringstream os;
  os << "Parse [";

  bool first = true;
  for (size_t i = 0; i < TokenType::TOKEN_TYPE_COUNT; ++i)
  {
    auto const & range = parse.m_ranges[i];
    if (range.Begin() == range.End())
      continue;

    if (!first)
      os << ", ";

    os << DebugPrint(static_cast<TokenType>(i)) << ": " << DebugPrint(range);
    first = false;
  }

  os << ", category: " << boolalpha << parse.m_category;
  os << "]";

  return os.str();
}

// Tracer ------------------------------------------------------------------------------------------
vector<Tracer::Parse> Tracer::GetUniqueParses() const
{
  auto parses = m_parses;
  base::SortUnique(parses);
  return parses;
}
}  // namespace search
