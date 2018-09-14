#pragma once

#include "search/common.hpp"
#include "search/feature_offset_match.hpp"
#include "search/token_slice.hpp"

#include "indexer/categories_holder.hpp"
#include "indexer/feature_decl.hpp"
#include "indexer/mwm_set.hpp"
#include "indexer/search_delimiters.hpp"
#include "indexer/search_string_utils.hpp"
#include "indexer/trie.hpp"

#include "geometry/rect2d.hpp"

#include "base/levenshtein_dfa.hpp"
#include "base/stl_helpers.hpp"
#include "base/string_utils.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

class DataSource;
class MwmInfo;

namespace search
{
size_t GetMaxErrorsForToken(strings::UniString const & token);

strings::LevenshteinDFA BuildLevenshteinDFA(strings::UniString const & s);

template <typename ToDo>
void ForEachCategoryType(StringSliceBase const & slice, Locales const & locales,
                         CategoriesHolder const & categories, ToDo && todo)
{
  for (size_t i = 0; i < slice.Size(); ++i)
  {
    auto const & token = slice.Get(i);
    for (int8_t const locale : locales)
      categories.ForEachTypeByName(locale, token, std::bind<void>(todo, i, std::placeholders::_1));

    // Special case processing of 2 codepoints emoji (e.g. black guy on a bike).
    // Only emoji synonyms can have one codepoint.
    if (token.size() > 1)
    {
      categories.ForEachTypeByName(CategoriesHolder::kEnglishCode, strings::UniString(1, token[0]),
                                   std::bind<void>(todo, i, std::placeholders::_1));
    }
  }
}

// Unlike ForEachCategoryType which extracts types by a token
// from |slice| by exactly matching it to a token in the name
// of a category, in the worst case this function has to loop through the tokens
// in all category synonyms in all |locales| in order to find a token
// whose edit distance is close enough to the required token from |slice|.
template <typename ToDo>
void ForEachCategoryTypeFuzzy(StringSliceBase const & slice, Locales const & locales,
                              CategoriesHolder const & categories, ToDo && todo)
{
  using Iterator = trie::MemTrieIterator<strings::UniString, base::VectorValues<uint32_t>>;

  auto const & trie = categories.GetNameToTypesTrie();
  Iterator const iterator(trie.GetRootIterator());

  for (size_t i = 0; i < slice.Size(); ++i)
  {
    // todo(@m, @y). We build dfa twice for each token: here and in geocoder.cpp.
    // A possible optimization is to build each dfa once and save it. Note that
    // dfas for the prefix tokens differ, i.e. we ignore slice.IsPrefix(i) here.
    SearchTrieRequest<strings::LevenshteinDFA> request;
    request.m_names.push_back(BuildLevenshteinDFA(slice.Get(i)));
    request.SetLangs(locales);

    MatchFeaturesInTrie(request, iterator, [&](uint32_t /* type */) { return true; } /* filter */,
                        std::bind<void>(todo, i, std::placeholders::_1));
  }
}

// Returns |true| and fills |types| if request specified by |slice| is categorial
// in any of the |locales| and |false| otherwise. We expect that categorial requests should
// mostly arise from clicking on a category button in the UI.
// It is assumed that typing a word that matches a category's name
// and a space after it means that no errors were made.
template <typename T>
bool FillCategories(QuerySliceOnRawStrings<T> const & slice, Locales const & locales,
                    CategoriesHolder const & catHolder, std::vector<uint32_t> & types)
{
  types.clear();
  if (slice.HasPrefixToken())
    return false;

  catHolder.ForEachNameAndType(
      [&](CategoriesHolder::Category::Name const & categorySynonym, uint32_t type) {
        if (!locales.Contains(static_cast<uint64_t>(categorySynonym.m_locale)))
          return;

        std::vector<QueryParams::String> categoryTokens;
        SplitUniString(search::NormalizeAndSimplifyString(categorySynonym.m_name),
                       base::MakeBackInsertFunctor(categoryTokens), search::Delimiters());

        if (slice.Size() != categoryTokens.size())
          return;

        for (size_t i = 0; i < slice.Size(); ++i)
        {
          if (slice.Get(i) != categoryTokens[i])
            return;
        }

        types.push_back(type);
      });

  return !types.empty();
}

// Returns classificator types for category with |name| and |locale|. For metacategories
// like "Hotel" returns all subcategories types.
std::vector<uint32_t> GetCategoryTypes(std::string const & name, std::string const & locale,
                                       CategoriesHolder const & categories);

MwmSet::MwmHandle FindWorld(DataSource const & dataSource,
                            std::vector<std::shared_ptr<MwmInfo>> const & infos);
MwmSet::MwmHandle FindWorld(DataSource const & dataSource);

using FeatureIndexCallback = std::function<void(FeatureID const &)>;
// Applies |fn| to each feature index of type from |types| in |rect|.
void ForEachOfTypesInRect(DataSource const & dataSource, std::vector<uint32_t> const & types,
                          m2::RectD const & rect, FeatureIndexCallback const & fn);
}  // namespace search
