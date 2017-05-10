#include "generator/dumper.hpp"

#include "search/search_index_values.hpp"
#include "search/search_trie.hpp"

#include "indexer/classificator.hpp"
#include "indexer/feature_processor.hpp"
#include "indexer/trie_reader.hpp"
#include "indexer/search_delimiters.hpp"
#include "indexer/search_string_utils.hpp"

#include "coding/multilang_utf8_string.hpp"

#include "base/logging.hpp"

#include <algorithm>
#include <functional>
#include <functional>
#include <iostream>
#include <map>
#include <vector>

namespace
{
template <typename TValue>
struct SearchTokensCollector
{
  SearchTokensCollector() : m_currentS(), m_currentCount(0) {}

  void operator()(strings::UniString const & s, TValue const & /* value */)
  {
    if (m_currentS != s)
    {
      if (m_currentCount > 0)
        m_tokens.emplace_back(m_currentCount, m_currentS);
      m_currentS = s;
      m_currentCount = 0;
    }
    ++m_currentCount;
  }

  void Finish()
  {
    if (m_currentCount > 0)
      m_tokens.emplace_back(m_currentCount, m_currentS);
    std::sort(m_tokens.begin(), m_tokens.end(), std::greater<pair<uint32_t, strings::UniString>>());
  }

  std::vector<pair<uint32_t, strings::UniString>> m_tokens;
  strings::UniString m_currentS;
  uint32_t m_currentCount;
};
}  // namespace

namespace feature
{
  class TypesCollector
  {
    std::vector<uint32_t> m_currFeatureTypes;

  public:
    typedef std::map<std::vector<uint32_t>, size_t> value_type;
    value_type m_stats;
    size_t m_namesCount;
    size_t m_totalCount;

    TypesCollector() : m_namesCount(0), m_totalCount(0) {}

    void operator()(FeatureType & f, uint32_t)
    {
      ++m_totalCount;
      std::string s1, s2;
      f.GetPreferredNames(s1, s2);
      if (!s1.empty())
        ++m_namesCount;

      m_currFeatureTypes.clear();
      f.ForEachType([this](uint32_t type)
      {
        m_currFeatureTypes.push_back(type);
      });
      CHECK(!m_currFeatureTypes.empty(), ("Feature without any type???"));

      auto found = m_stats.insert(make_pair(m_currFeatureTypes, 1));
      if (!found.second)
        found.first->second++;
    }
  };

  template <class T>
  static bool SortFunc(T const & first, T const & second)
  {
    return first.second > second.second;
  }

  void DumpTypes(std::string const & fPath)
  {
    TypesCollector doClass;
    feature::ForEachFromDat(fPath, doClass);

    typedef pair<std::vector<uint32_t>, size_t> stats_elem_type;
    typedef std::vector<stats_elem_type> vec_to_sort;
    vec_to_sort vecToSort(doClass.m_stats.begin(), doClass.m_stats.end());
    std::sort(vecToSort.begin(), vecToSort.end(), &SortFunc<stats_elem_type>);

    for (vec_to_sort::iterator it = vecToSort.begin(); it != vecToSort.end(); ++it)
    {
      std::cout << it->second << " ";
      for (size_t i = 0; i < it->first.size(); ++i)
        std::cout << classif().GetFullObjectName(it->first[i]) << " ";
      std::cout << endl;
    }
    std::cout << "Total features: " << doClass.m_totalCount << endl;
    std::cout << "Features with names: " << doClass.m_namesCount << endl;
  }

  ///////////////////////////////////////////////////////////////////

  typedef std::map<int8_t, std::map<strings::UniString, pair<unsigned int, std::string> > > TokensContainerT;
  class PrefixesCollector
  {
  public:
    TokensContainerT m_stats;

    bool operator()(int8_t langCode, std::string const & name)
    {
      CHECK(!name.empty(), ("Feature name is empty"));

      std::vector<strings::UniString> tokens;
      search::SplitUniString(search::NormalizeAndSimplifyString(name),
                             MakeBackInsertFunctor(tokens), search::Delimiters());

      if (tokens.empty())
        return true;

      for (size_t i = 1; i < tokens.size(); ++i)
      {
        strings::UniString s;
        for (size_t numTokens = 0; numTokens < i; ++numTokens)
        {
          s.append(tokens[numTokens].begin(), tokens[numTokens].end());
          s.push_back(' ');
        }
        pair<TokensContainerT::mapped_type::iterator, bool> found =
            m_stats[langCode].insert(make_pair(s, make_pair(1U, name)));
        if (!found.second)
          found.first->second.first++;
      }
      return true;
    }

    void operator()(FeatureType & f, uint32_t)
    {
      f.ForEachName(*this);
    }
  };

  static size_t const MIN_OCCURRENCE = 3;

  void Print(int8_t langCode, TokensContainerT::mapped_type const & container)
  {
    typedef pair<strings::UniString, pair<unsigned int, std::string> > NameElemT;
    typedef std::vector<NameElemT> VecToSortT;

    VecToSortT v(container.begin(), container.end());
    std::sort(v.begin(), v.end(), &SortFunc<NameElemT>);

    // do not display prefixes with low occurrences
    if (v[0].second.first > MIN_OCCURRENCE)
    {
      std::cout << "Language code: " << StringUtf8Multilang::GetLangByCode(langCode) << endl;

      for (VecToSortT::iterator it = v.begin(); it != v.end(); ++it)
      {
        if (it->second.first <= MIN_OCCURRENCE)
          break;
        std::cout << it->second.first << " " << strings::ToUtf8(it->first);
        std::cout << " \"" << it->second.second << "\"" << endl;
      }
    }
  }

  void DumpPrefixes(std::string const & fPath)
  {
    PrefixesCollector doClass;
    feature::ForEachFromDat(fPath, doClass);
    for (TokensContainerT::iterator it = doClass.m_stats.begin();
         it != doClass.m_stats.end(); ++it)
    {
      Print(it->first, it->second);
    }
  }

  void DumpSearchTokens(std::string const & fPath, size_t maxTokensToShow)
  {
    using TValue = FeatureIndexValue;

    FilesContainerR container(make_unique<FileReader>(fPath));
    feature::DataHeader header(container);
    serial::CodingParams codingParams(trie::GetCodingParams(header.GetDefCodingParams()));

    auto const trieRoot = trie::ReadTrie<ModelReaderPtr, ValueList<TValue>>(
        container.GetReader(SEARCH_INDEX_FILE_TAG), SingleValueSerializer<TValue>(codingParams));

    SearchTokensCollector<TValue> f;
    trie::ForEachRef(*trieRoot, f, strings::UniString());
    f.Finish();

    for (size_t i = 0; i < std::min(maxTokensToShow, f.m_tokens.size()); ++i)
    {
      auto const & s = f.m_tokens[i].second;
      std::cout << f.m_tokens[i].first << " " << strings::ToUtf8(s) << endl;
    }
  }

  void DumpFeatureNames(std::string const & fPath, std::string const & lang)
  {
    int8_t const langIndex = StringUtf8Multilang::GetLangIndex(lang);
    auto printName = [&](int8_t langCode, std::string const & name) -> bool
    {
      CHECK(!name.empty(), ("Feature name is empty"));
      if (langIndex == StringUtf8Multilang::kUnsupportedLanguageCode)
        std::cout << StringUtf8Multilang::GetLangByCode(langCode) << ' ' << name << endl;
      else if (langCode == langIndex)
        std::cout << name << endl;
      return true;
    };

    feature::ForEachFromDat(fPath, [&](FeatureType & f, uint32_t)
                            {
                              f.ForEachName(printName);
                            });
  }

}  // namespace feature
