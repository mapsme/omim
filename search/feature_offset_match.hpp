#pragma once
#include "search/common.hpp"
#include "search/query_params.hpp"
#include "search/search_index_values.hpp"
#include "search/search_trie.hpp"
#include "search/token_slice.hpp"

#include "indexer/trie.hpp"

#include "base/dfa_helpers.hpp"
#include "base/levenshtein_dfa.hpp"
#include "base/mutex.hpp"
#include "base/scope_guard.hpp"
#include "base/stl_add.hpp"
#include "base/string_utils.hpp"
#include "base/uni_string_dfa.hpp"

#include "std/algorithm.hpp"
#include "std/queue.hpp"
#include "std/target_os.hpp"
#include "std/unique_ptr.hpp"
#include "std/unordered_set.hpp"
#include "std/utility.hpp"
#include "std/vector.hpp"

namespace search
{
namespace impl
{
namespace
{
template <typename Value>
bool FindLangIndex(trie::Iterator<ValueList<Value>> const & trieRoot, uint8_t lang, uint32_t & langIx)
{
  ASSERT_LESS(trieRoot.m_edge.size(), numeric_limits<uint32_t>::max(), ());

  uint32_t const numLangs = static_cast<uint32_t>(trieRoot.m_edge.size());
  for (uint32_t i = 0; i < numLangs; ++i)
  {
    auto const & edge = trieRoot.m_edge[i].m_label;
    ASSERT_GREATER_OR_EQUAL(edge.size(), 1, ());
    if (edge[0] == lang)
    {
      langIx = i;
      return true;
    }
  }
  return false;
}
}  // namespace

template <typename Value, typename DFA, typename ToDo>
bool MatchInTrie(trie::Iterator<ValueList<Value>> const & trieRoot,
                 strings::UniChar const * rootPrefix, size_t rootPrefixSize, DFA const & dfa,
                 ToDo && toDo)
{
  using TrieDFAIt = shared_ptr<trie::Iterator<ValueList<Value>>>;
  using DFAIt = typename DFA::Iterator;
  using State = pair<TrieDFAIt, DFAIt>;

  queue<State> q;

  {
    auto it = dfa.Begin();
    DFAMove(it, rootPrefix, rootPrefix + rootPrefixSize);
    if (it.Rejects())
      return false;
    q.emplace(trieRoot.Clone(), it);
  }

  bool found = false;

  while (!q.empty())
  {
    auto const p = q.front();
    q.pop();

    auto const & trieIt = p.first;
    auto const & dfaIt = p.second;

    if (dfaIt.Accepts())
    {
      trieIt->m_valueList.ForEach(toDo);
      found = true;
    }

    size_t const numEdges = trieIt->m_edge.size();
    for (size_t i = 0; i < numEdges; ++i)
    {
      auto const & edge = trieIt->m_edge[i];

      auto curIt = dfaIt;
      strings::DFAMove(curIt, edge.m_label.begin(), edge.m_label.end());
      if (!curIt.Rejects())
        q.emplace(trieIt->GoToEdge(i), curIt);
    }
  }

  return found;
}

template <typename Filter, typename Value>
class OffsetIntersector
{
  struct Hash
  {
    size_t operator()(Value const & v) const { return v.m_featureId; }
  };

  struct Equal
  {
    bool operator()(Value const & v1, Value const & v2) const
    {
      return v1.m_featureId == v2.m_featureId;
    }
  };

  using Set = unordered_set<Value, Hash, Equal>;

  Filter const & m_filter;
  unique_ptr<Set> m_prevSet;
  unique_ptr<Set> m_set;

public:
  explicit OffsetIntersector(Filter const & filter) : m_filter(filter), m_set(make_unique<Set>()) {}

  void operator()(Value const & v)
  {
    if (m_prevSet && !m_prevSet->count(v))
      return;

    if (!m_filter(v.m_featureId))
      return;

    m_set->insert(v);
  }

  void NextStep()
  {
    if (!m_prevSet)
      m_prevSet = make_unique<Set>();

    m_prevSet.swap(m_set);
    m_set->clear();
  }

  template <class ToDo>
  void ForEachResult(ToDo && toDo) const
  {
    if (!m_prevSet)
      return;
    for (auto const & value : *m_prevSet)
      toDo(value);
  }
};
}  // namespace impl

template <typename Value>
struct TrieRootPrefix
{
  using Iterator = trie::Iterator<ValueList<Value>>;

  Iterator const & m_root;
  strings::UniChar const * m_prefix;
  size_t m_prefixSize;

  TrieRootPrefix(Iterator const & root, typename Iterator::Edge::TEdgeLabel const & edge)
    : m_root(root)
  {
    if (edge.size() == 1)
    {
      m_prefix = 0;
      m_prefixSize = 0;
    }
    else
    {
      m_prefix = &edge[1];
      m_prefixSize = edge.size() - 1;
    }
  }
};

template <typename Filter, typename Value>
class TrieValuesHolder
{
public:
  TrieValuesHolder(Filter const & filter) : m_filter(filter) {}

  void operator()(Value const & v)
  {
    if (m_filter(v.m_featureId))
      m_values.push_back(v);
  }

  template <class ToDo>
  void ForEachValue(ToDo && toDo) const
  {
    for (auto const & value : m_values)
      toDo(value);
  }

private:
  vector<Value> m_values;
  Filter const & m_filter;
};

template <typename DFA>
struct SearchTrieRequest
{
  inline bool IsLangExist(int8_t lang) const { return m_langs.Contains(lang); }

  inline void Clear()
  {
    m_names.clear();
    m_categories.clear();
    m_langs.Clear();
  }

  vector<DFA> m_names;
  vector<strings::UniStringDFA> m_categories;
  QueryParams::Langs m_langs;
};

// Calls |toDo| for each feature accepted by at least one DFA.
//
// *NOTE* |toDo| may be called several times for the same feature.
template <typename DFA, typename Value, typename ToDo>
void MatchInTrie(vector<DFA> const & dfas, TrieRootPrefix<Value> const & trieRoot, ToDo && toDo)
{
  for (auto const & dfa : dfas)
    impl::MatchInTrie(trieRoot.m_root, trieRoot.m_prefix, trieRoot.m_prefixSize, dfa, toDo);
}

// Calls |toDo| for each feature in categories branch matching to |request|.
//
// *NOTE* |toDo| may be called several times for the same feature.
template <typename DFA, typename Value, typename ToDo>
bool MatchCategoriesInTrie(SearchTrieRequest<DFA> const & request,
                           trie::Iterator<ValueList<Value>> const & trieRoot, ToDo && toDo)
{
  uint32_t langIx = 0;
  if (!impl::FindLangIndex(trieRoot, search::kCategoriesLang, langIx))
    return false;

  auto const & edge = trieRoot.m_edge[langIx].m_label;
  ASSERT_GREATER_OR_EQUAL(edge.size(), 1, ());

  auto const catRoot = trieRoot.GoToEdge(langIx);
  MatchInTrie(request.m_categories, TrieRootPrefix<Value>(*catRoot, edge), toDo);

  return true;
}

// Calls |toDo| with trie root prefix and language code on each
// language allowed by |request|.
template <typename DFA, typename Value, typename ToDo>
void ForEachLangPrefix(SearchTrieRequest<DFA> const & request,
                       trie::Iterator<ValueList<Value>> const & trieRoot, ToDo && toDo)
{
  ASSERT_LESS(trieRoot.m_edge.size(), numeric_limits<uint32_t>::max(), ());

  uint32_t const numLangs = static_cast<uint32_t>(trieRoot.m_edge.size());
  for (uint32_t langIx = 0; langIx < numLangs; ++langIx)
  {
    auto const & edge = trieRoot.m_edge[langIx].m_label;
    ASSERT_GREATER_OR_EQUAL(edge.size(), 1, ());
    int8_t const lang = static_cast<int8_t>(edge[0]);
    if (edge[0] < search::kCategoriesLang && request.IsLangExist(lang))
    {
      auto const langRoot = trieRoot.GoToEdge(langIx);
      TrieRootPrefix<Value> langPrefix(*langRoot, edge);
      toDo(langPrefix, lang);
    }
  }
}

// Calls |toDo| for each feature whose description matches to
// |request|.  Each feature will be passed to |toDo| only once.
template <typename DFA, typename Value, typename Filter, typename ToDo>
void MatchFeaturesInTrie(SearchTrieRequest<DFA> const & request,
                         trie::Iterator<ValueList<Value>> const & trieRoot, Filter const & filter,
                         ToDo && toDo)
{
  TrieValuesHolder<Filter, Value> categoriesHolder(filter);
  bool const categoriesMatched = MatchCategoriesInTrie(request, trieRoot, categoriesHolder);

  impl::OffsetIntersector<Filter, Value> intersector(filter);

  ForEachLangPrefix(request, trieRoot,
                    [&request, &intersector](TrieRootPrefix<Value> & langRoot, int8_t /* lang */) {
                      MatchInTrie(request.m_names, langRoot, intersector);
                    });

  if (categoriesMatched)
    categoriesHolder.ForEachValue(intersector);

  intersector.NextStep();
  intersector.ForEachResult(forward<ToDo>(toDo));
}

template <typename Value, typename Filter, typename ToDo>
void MatchPostcodesInTrie(TokenSlice const & slice,
                          trie::Iterator<ValueList<Value>> const & trieRoot,
                          Filter const & filter, ToDo && toDo)
{
  using namespace strings;

  uint32_t langIx = 0;
  if (!impl::FindLangIndex(trieRoot, search::kPostcodesLang, langIx))
    return;

  auto const & edge = trieRoot.m_edge[langIx].m_label;
  auto const postcodesRoot = trieRoot.GoToEdge(langIx);

  impl::OffsetIntersector<Filter, Value> intersector(filter);
  for (size_t i = 0; i < slice.Size(); ++i)
  {
    if (slice.IsPrefix(i))
    {
      vector<PrefixDFAModifier<UniStringDFA>> dfas;
      slice.Get(i).ForEach([&dfas](UniString const & s) { dfas.emplace_back(UniStringDFA(s)); });
      MatchInTrie(dfas, TrieRootPrefix<Value>(*postcodesRoot, edge), intersector);
    }
    else
    {
      vector<UniStringDFA> dfas;
      slice.Get(i).ForEach([&dfas](UniString const & s) { dfas.emplace_back(s); });
      MatchInTrie(dfas, TrieRootPrefix<Value>(*postcodesRoot, edge), intersector);
    }

    intersector.NextStep();
  }

  intersector.ForEachResult(forward<ToDo>(toDo));
}
}  // namespace search
