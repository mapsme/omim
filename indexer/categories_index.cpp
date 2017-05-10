#include "categories_index.hpp"
#include "search_delimiters.hpp"
#include "search_string_utils.hpp"

#include "base/assert.hpp"
#include "base/stl_add.hpp"
#include "base/stl_helpers.hpp"
#include "base/string_utils.hpp"

#include <algorithm>
#include <set>

namespace
{
void AddAllNonemptySubstrings(my::MemTrie<std::string, my::VectorValues<uint32_t>> & trie,
                              std::string const & s, uint32_t value)
{
  ASSERT(!s.empty(), ());
  for (size_t i = 0; i < s.length(); ++i)
  {
    std::string t;
    for (size_t j = i; j < s.length(); ++j)
    {
      t.push_back(s[j]);
      trie.Add(t, value);
    }
  }
}

template <typename TF>
void ForEachToken(std::string const & s, TF && fn)
{
  std::vector<strings::UniString> tokens;
  SplitUniString(search::NormalizeAndSimplifyString(s), MakeBackInsertFunctor(tokens),
                 search::Delimiters());
  for (auto const & token : tokens)
    fn(strings::ToUtf8(token));
}

void TokenizeAndAddAllSubstrings(my::MemTrie<std::string, my::VectorValues<uint32_t>> & trie,
                                 std::string const & s, uint32_t value)
{
  auto fn = [&](std::string const & token)
  {
    AddAllNonemptySubstrings(trie, token, value);
  };
  ForEachToken(s, fn);
}
}  // namespace

namespace indexer
{
void CategoriesIndex::AddCategoryByTypeAndLang(uint32_t type, int8_t lang)
{
  ASSERT(lang >= 1 && static_cast<size_t>(lang) <= CategoriesHolder::kLocaleMapping.size(),
         ("Invalid lang code:", lang));
  m_catHolder->ForEachNameByType(type, [&](TCategory::Name const & name)
                                 {
                                   if (name.m_locale == lang)
                                     TokenizeAndAddAllSubstrings(m_trie, name.m_name, type);
                                 });
}

void CategoriesIndex::AddCategoryByTypeAllLangs(uint32_t type)
{
  for (size_t i = 1; i <= CategoriesHolder::kLocaleMapping.size(); ++i)
    AddCategoryByTypeAndLang(type, i);
}

void CategoriesIndex::AddAllCategoriesInLang(int8_t lang)
{
  ASSERT(lang >= 1 && static_cast<size_t>(lang) <= CategoriesHolder::kLocaleMapping.size(),
         ("Invalid lang code:", lang));
  m_catHolder->ForEachTypeAndCategory([&](uint32_t type, TCategory const & cat)
                                      {
                                        for (auto const & name : cat.m_synonyms)
                                        {
                                          if (name.m_locale == lang)
                                            TokenizeAndAddAllSubstrings(m_trie, name.m_name, type);
                                        }
                                      });
}

void CategoriesIndex::AddAllCategoriesInAllLangs()
{
  m_catHolder->ForEachTypeAndCategory([this](uint32_t type, TCategory const & cat)
                                      {
                                        for (auto const & name : cat.m_synonyms)
                                          TokenizeAndAddAllSubstrings(m_trie, name.m_name, type);
                                      });
}

void CategoriesIndex::GetCategories(std::string const & query, std::vector<TCategory> & result) const
{
  std::vector<uint32_t> types;
  GetAssociatedTypes(query, types);
  my::SortUnique(types);
  m_catHolder->ForEachTypeAndCategory([&](uint32_t type, TCategory const & cat)
                                      {
                                        if (std::binary_search(types.begin(), types.end(), type))
                                          result.push_back(cat);
                                      });
}

void CategoriesIndex::GetAssociatedTypes(std::string const & query, std::vector<uint32_t> & result) const
{
  bool first = true;
  std::set<uint32_t> intersection;
  auto processToken = [&](std::string const & token)
  {
    std::set<uint32_t> types;
    auto fn = [&](std::string const &, uint32_t type)
    {
      types.insert(type);
    };
    m_trie.ForEachInSubtree(token, fn);

    if (first)
    {
      intersection.swap(types);
    }
    else
    {
      std::set<uint32_t> tmp;
      std::set_intersection(intersection.begin(), intersection.end(), types.begin(), types.end(),
                       inserter(tmp, tmp.begin()));
      intersection.swap(tmp);
    }
    first = false;
  };
  ForEachToken(query, processToken);

  result.insert(result.end(), intersection.begin(), intersection.end());
}
}  // namespace indexer
