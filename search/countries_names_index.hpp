#pragma once

#include "search/base/mem_search_index.hpp"
#include "search/feature_offset_match.hpp"

#include "storage/storage_defines.hpp"

#include "indexer/search_string_utils.hpp"

#include "base/string_utils.hpp"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace search
{
class CountriesNamesIndex
{
public:
  struct Doc
  {
    template <typename Fn>
    void ForEachToken(Fn && fn) const
    {
      for (auto const & s : m_translations)
        fn(StringUtf8Multilang::kDefaultCode, NormalizeAndSimplifyString(s));
    }

    std::vector<std::string> m_translations;
  };

  CountriesNamesIndex();

  void CollectMatchingCountries(std::string const & query,
                                std::vector<storage::CountryId> & results);

private:
  struct Country
  {
    storage::CountryId m_countryId;
    Doc m_doc;
  };

  // todo(@m) Almost the same as in bookmarks/processor.hpp.
  template <typename DFA, typename Fn>
  void Retrieve(strings::UniString const & s, Fn && fn) const
  {
    SearchTrieRequest<DFA> request;
    request.m_names.emplace_back(BuildLevenshteinDFA(s));
    request.m_langs.insert(StringUtf8Multilang::kDefaultCode);

    MatchFeaturesInTrie(
        request, m_index.GetRootIterator(), [](size_t id) { return true; } /* filter */,
        std::forward<Fn>(fn));
  }

  void ReadCountryNamesFromFile(std::vector<Country> & countries);
  void BuildIndexFromTranslations();

  std::vector<Country> m_countries;
  search_base::MemSearchIndex<size_t> m_index;
};
}  // namespace search
