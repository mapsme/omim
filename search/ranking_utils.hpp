#pragma once

#include "search/common.hpp"
#include "search/model.hpp"
#include "search/query_params.hpp"

#include "indexer/search_delimiters.hpp"
#include "indexer/search_string_utils.hpp"

#include "base/levenshtein_dfa.hpp"
#include "base/stl_helpers.hpp"
#include "base/string_utils.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <string>
#include <vector>

class CategoriesHolder;

namespace feature
{
class TypesHolder;
}

namespace search
{
class QueryParams;
class TokenSlice;

class CategoriesInfo
{
public:
  CategoriesInfo(feature::TypesHolder const & holder, TokenSlice const & tokens,
                 Locales const & locales, CategoriesHolder const & categories);

  // Returns true when all tokens correspond to categories in
  // |holder|.
  bool IsPureCategories() const { return m_pureCategories; }

  // Returns true when all tokens are categories tokens but none of
  // them correspond to categories in |holder|.
  bool IsFalseCategories() const { return m_falseCategories; }

private:
  bool m_pureCategories = false;
  bool m_falseCategories = false;
};

struct ErrorsMade
{
  static size_t constexpr kInfiniteErrors = std::numeric_limits<size_t>::max();

  ErrorsMade() = default;
  explicit ErrorsMade(size_t errorsMade) : m_errorsMade(errorsMade) {}

  bool IsValid() const { return m_errorsMade != kInfiniteErrors; }

  template <typename Fn>
  static ErrorsMade Combine(ErrorsMade const & lhs, ErrorsMade const & rhs, Fn && fn)
  {
    if (!lhs.IsValid())
      return rhs;
    if (!rhs.IsValid())
      return lhs;
    return ErrorsMade(fn(lhs.m_errorsMade, rhs.m_errorsMade));
  }

  static ErrorsMade Min(ErrorsMade const & lhs, ErrorsMade const & rhs)
  {
    return Combine(lhs, rhs, [](size_t u, size_t v) { return std::min(u, v); });
  }

  static ErrorsMade Max(ErrorsMade const & lhs, ErrorsMade const & rhs)
  {
    return Combine(lhs, rhs, [](size_t u, size_t v) { return std::max(u, v); });
  }

  friend ErrorsMade operator+(ErrorsMade const & lhs, ErrorsMade const & rhs)
  {
    return Combine(lhs, rhs, [](size_t u, size_t v) { return u + v; });
  }

  ErrorsMade & operator+=(ErrorsMade const & rhs)
  {
    *this = *this + rhs;
    return *this;
  }

  bool operator==(ErrorsMade const & rhs) const { return m_errorsMade == rhs.m_errorsMade; }

  size_t m_errorsMade = kInfiniteErrors;
};

std::string DebugPrint(ErrorsMade const & errorsMade);

namespace impl
{
// Returns the minimum number of errors needed to match |text| with
// any of the |tokens|.  If it's not possible in accordance with
// GetMaxErrorsForToken(|text|), returns kInfiniteErrors.
ErrorsMade GetErrorsMade(QueryParams::Token const & token, strings::UniString const & text);
ErrorsMade GetPrefixErrorsMade(QueryParams::Token const & token, strings::UniString const & text);
}  // namespace impl

// The order and numeric values are important here.  Please, check all
// use-cases before changing this enum.
enum NameScore
{
  NAME_SCORE_ZERO = 0,
  NAME_SCORE_SUBSTRING = 1,
  NAME_SCORE_PREFIX = 2,
  NAME_SCORE_FULL_MATCH = 3,

  NAME_SCORE_COUNT
};

struct NameScores
{
  static NameScores BestScores(NameScores const & lhs, NameScores const & rhs);

  NameScore m_nameScore = NAME_SCORE_ZERO;
  ErrorsMade m_errorsMade;
};

// Returns true when |s| is a stop-word and may be removed from a query.
bool IsStopWord(strings::UniString const & s);

// Normalizes, simplifies and splits string, removes stop-words.
void PrepareStringForMatching(std::string const & name, std::vector<strings::UniString> & tokens);

template <typename Slice>
NameScores GetNameScores(std::vector<strings::UniString> const & tokens, Slice const & slice)
{
  if (slice.Empty())
    return {};

  size_t const n = tokens.size();
  size_t const m = slice.Size();

  bool const lastTokenIsPrefix = slice.IsPrefix(m - 1);

  NameScores scores;
  for (size_t offset = 0; offset + m <= n; ++offset)
  {
    ErrorsMade totalErrorsMade;
    bool match = true;
    for (size_t i = 0; i < m - 1 && match; ++i)
    {
      auto errorsMade = impl::GetErrorsMade(slice.Get(i), tokens[offset + i]);
      match = match && errorsMade.IsValid();
      totalErrorsMade += errorsMade;
    }

    if (!match)
      continue;

    auto const prefixErrorsMade =
        impl::GetPrefixErrorsMade(slice.Get(m - 1), tokens[offset + m - 1]);
    auto const fullErrorsMade = impl::GetErrorsMade(slice.Get(m - 1), tokens[offset + m - 1]);
    if (!fullErrorsMade.IsValid() && !(prefixErrorsMade.IsValid() && lastTokenIsPrefix))
      continue;

    if (m == n && fullErrorsMade.IsValid())
    {
      scores.m_nameScore = NAME_SCORE_FULL_MATCH;
      scores.m_errorsMade = totalErrorsMade + fullErrorsMade;
      return scores;
    }

    if (offset == 0)
    {
      scores.m_nameScore = std::max(scores.m_nameScore, NAME_SCORE_PREFIX);
      scores.m_errorsMade = totalErrorsMade + prefixErrorsMade;
    }
    else
    {
      scores.m_nameScore = std::max(scores.m_nameScore, NAME_SCORE_SUBSTRING);
      scores.m_errorsMade = totalErrorsMade + prefixErrorsMade;
    }
  }
  return scores;
}

template <typename Slice>
NameScores GetNameScores(std::string const & name, Slice const & slice)
{
  std::vector<strings::UniString> tokens;
  SplitUniString(NormalizeAndSimplifyString(name), base::MakeBackInsertFunctor(tokens),
                 Delimiters());
  return GetNameScores(tokens, slice);
}

std::string DebugPrint(NameScore score);
}  // namespace search
