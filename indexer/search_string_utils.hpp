#pragma once
#include "base/stl_add.hpp"
#include "base/string_utils.hpp"

#include <algorithm>
#include <functional>
#include <string>
#include <utility>

namespace search
{

// This function should be used for all search strings normalization.
// It does some magic text transformation which greatly helps us to improve our search.
strings::UniString NormalizeAndSimplifyString(std::string const & s);

template <class DelimsT, typename F>
void SplitUniString(strings::UniString const & uniS, F f, DelimsT const & delims)
{
  for (strings::TokenizeIterator<DelimsT> iter(uniS, delims); iter; ++iter)
    f(iter.GetUniString());
}

template <typename TCont, typename TDelims>
void NormalizeAndTokenizeString(std::string const & s, TCont & tokens, TDelims const & delims)
{
  SplitUniString(NormalizeAndSimplifyString(s), MakeBackInsertFunctor(tokens), delims);
}

strings::UniString FeatureTypeToString(uint32_t type);

template <class ContainerT, class DelimsT>
bool TokenizeStringAndCheckIfLastTokenIsPrefix(strings::UniString const & s,
                                               ContainerT & tokens,
                                               DelimsT const & delimiter)
{
  SplitUniString(s, MakeBackInsertFunctor(tokens), delimiter);
  return !s.empty() && !delimiter(s.back());
}


template <class ContainerT, class DelimsT>
bool TokenizeStringAndCheckIfLastTokenIsPrefix(std::string const & s,
                                               ContainerT & tokens,
                                               DelimsT const & delimiter)
{
  return TokenizeStringAndCheckIfLastTokenIsPrefix(NormalizeAndSimplifyString(s),
                                                   tokens,
                                                   delimiter);
}

strings::UniString GetStreetNameAsKey(std::string const & name);

// *NOTE* The argument string must be normalized and simplified.
bool IsStreetSynonym(strings::UniString const & s);
bool IsStreetSynonymPrefix(strings::UniString const & s);

/// Normalizes both str and substr, and then returns true if substr is found in str.
/// Used in native platform code for search in localized strings (cuisines, categories, strings etc.).
bool ContainsNormalized(std::string const & str, std::string const & substr);

// This class can be used as a filter for street tokens.  As there can
// be street synonyms in the street name, single street synonym is
// skipped, but multiple synonyms are left as is. For example, when
// applied to ["улица", "ленина"] the filter must emit only
// ["ленина"], but when applied to ["улица", "набережная"] the filter
// must emit both tokens as is, i.e. ["улица", "набережная"].
class StreetTokensFilter
{
public:
  using TCallback = std::function<void(strings::UniString const & token, size_t tag)>;

  template <typename TC>
  StreetTokensFilter(TC && callback)
    : m_callback(std::forward<TC>(callback))
  {
  }

  // Puts token to the filter. Filter checks following cases:
  // * when |token| is the first street synonym met so far, it's delayed
  // * when |token| is the second street synonym met so far,
  //   callback is called for the |token| and for the previously delayed token
  // * otherwise, callback is called for the |token|
  void Put(strings::UniString const & token, bool isPrefix, size_t tag);

private:
  using TCell = std::pair<strings::UniString, size_t>;

  inline void EmitToken(strings::UniString const & token, size_t tag) { m_callback(token, tag); }

  strings::UniString m_delayedToken;
  size_t m_delayedTag = 0;
  size_t m_numSynonyms = 0;

  TCallback m_callback;
};
}  // namespace search
