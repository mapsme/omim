#include "testing/testing.hpp"

#include "search/cbv.hpp"
#include "search/geocoder_context.hpp"
#include "search/locality_scorer.hpp"

#include "indexer/search_delimiters.hpp"
#include "indexer/search_string_utils.hpp"

#include "coding/compressed_bit_vector.hpp"

#include "base/assert.hpp"
#include "base/mem_trie.hpp"
#include "base/stl_helpers.hpp"
#include "base/string_utils.hpp"

#include <algorithm>
#include <set>
#include <unordered_map>
#include <vector>

using namespace search;
using namespace std;
using namespace strings;

namespace
{
class LocalityScorerTest : public LocalityScorer::Delegate
{
public:
  using Ids = vector<uint32_t>;

  LocalityScorerTest() : m_scorer(m_params, static_cast<LocalityScorer::Delegate &>(*this)) {}

  void InitParams(string const & query, bool lastTokenIsPrefix)
  {
    m_params.Clear();

    vector<UniString> tokens;
    Delimiters delims;
    SplitUniString(NormalizeAndSimplifyString(query), base::MakeBackInsertFunctor(tokens), delims);

    if (lastTokenIsPrefix)
    {
      CHECK(!tokens.empty(), ());
      m_params.InitWithPrefix(tokens.begin(), tokens.end() - 1, tokens.back());
    }
    else
    {
      m_params.InitNoPrefix(tokens.begin(), tokens.end());
    }
  }

  void AddLocality(string const & name, uint32_t featureId, uint8_t rank = 0)
  {
    set<UniString> tokens;
    Delimiters delims;
    SplitUniString(NormalizeAndSimplifyString(name), base::MakeInsertFunctor(tokens), delims);

    for (auto const & token : tokens)
      m_searchIndex.Add(token, featureId);

    m_names[featureId].push_back(name);
    m_ranks[featureId] = rank;
  }

  Ids GetTopLocalities(size_t limit)
  {
    BaseContext ctx;
    ctx.m_tokens.assign(m_params.GetNumTokens(), BaseContext::TOKEN_TYPE_COUNT);
    ctx.m_numTokens = m_params.GetNumTokens();

    for (size_t i = 0; i < m_params.GetNumTokens(); ++i)
    {
      auto const & token = m_params.GetToken(i);
      bool const isPrefixToken = m_params.IsPrefixToken(i);

      vector<uint64_t> ids;
      token.ForEach([&](UniString const & name) {
        if (isPrefixToken)
        {
          m_searchIndex.ForEachInSubtree(name,
                                         [&](UniString const & /* prefix */, uint32_t featureId) {
                                           ids.push_back(featureId);
                                         });
        }
        else
        {
          m_searchIndex.ForEachInNode(name, [&](uint32_t featureId) { ids.push_back(featureId); });
        }
      });

      base::SortUnique(ids);
      ctx.m_features.emplace_back(coding::CompressedBitVectorBuilder::FromBitPositions(ids));
    }

    CBV filter;
    filter.SetFull();

    vector<Locality> localities;
    m_scorer.GetTopLocalities(MwmSet::MwmId(), ctx, filter, limit, localities);
    sort(localities.begin(), localities.end(), base::LessBy(&Locality::m_featureId));

    Ids ids;
    for (auto const & locality : localities)
      ids.push_back(locality.m_featureId);
    return ids;
  }

  // LocalityScorer::Delegate overrides:
  void GetNames(uint32_t featureId, vector<string> & names) const override
  {
    auto it = m_names.find(featureId);
    if (it != m_names.end())
      names.insert(names.end(), it->second.begin(), it->second.end());
  }

  uint8_t GetRank(uint32_t featureId) const override
  {
    auto it = m_ranks.find(featureId);
    return it == m_ranks.end() ? 0 : it->second;
  }

  CBV GetMatchedFeatures(strings::UniString const & token, bool isPrefix) const override
  {
    vector<uint64_t> ids;

    if (isPrefix)
    {
      m_searchIndex.ForEachInSubtree(token, [&ids](strings::UniString const & /* prefix */,
                                                   uint32_t id) { ids.push_back(id); });
    }
    else
    {
      m_searchIndex.ForEachInNode(token, [&ids](uint32_t id) { ids.push_back(id); });
    }

    base::SortUnique(ids);
    return CBV{coding::CompressedBitVectorBuilder::FromBitPositions(move(ids))};
  }

protected:
  QueryParams m_params;
  unordered_map<uint32_t, vector<string>> m_names;
  unordered_map<uint32_t, uint8_t> m_ranks;
  LocalityScorer m_scorer;

  base::MemTrie<UniString, base::VectorValues<uint32_t>> m_searchIndex;
};
}  // namespace

UNIT_CLASS_TEST(LocalityScorerTest, Smoke)
{
  enum
  {
    ID_NEW_ORLEANS,
    ID_YORK,
    ID_NEW_YORK,
  };

  InitParams("New York Time Square", false /* lastTokenIsPrefix */);

  AddLocality("New Orleans", ID_NEW_ORLEANS);
  AddLocality("York", ID_YORK);
  AddLocality("New York", ID_NEW_YORK);

  TEST_EQUAL(GetTopLocalities(100 /* limit */), Ids({ID_NEW_ORLEANS, ID_YORK, ID_NEW_YORK}), ());
  TEST_EQUAL(GetTopLocalities(2 /* limit */), Ids({ID_YORK, ID_NEW_YORK}), ());
  TEST_EQUAL(GetTopLocalities(1 /* limit */), Ids({ID_YORK}), ());
}

UNIT_CLASS_TEST(LocalityScorerTest, NumbersMatch)
{
  enum
  {
    ID_MARCH,
    ID_APRIL,
    ID_MAY,
    ID_TVER
  };

  InitParams("тверь советская 1", false /* lastTokenIsPrefix */);

  AddLocality("поселок 1 марта", ID_MARCH);
  AddLocality("поселок 1 апреля", ID_APRIL);
  AddLocality("поселок 1 мая", ID_MAY);
  AddLocality("тверь", ID_TVER);

  // Tver is the only matched locality as other localities were
  // matched only by number.
  TEST_EQUAL(GetTopLocalities(100 /* limit */), Ids({ID_TVER}), ());
  TEST_EQUAL(GetTopLocalities(1 /* limit */), Ids({ID_TVER}), ());
}

UNIT_CLASS_TEST(LocalityScorerTest, NumbersComplexMatch)
{
  enum
  {
    ID_MAY,
    ID_SAINT_PETERSBURG
  };

  InitParams("saint petersburg may 1", false /* lastTokenIsPrefix */);

  AddLocality("may 1", ID_MAY);
  AddLocality("saint petersburg", ID_SAINT_PETERSBURG);

  TEST_EQUAL(GetTopLocalities(2 /* limit */), Ids({ID_MAY, ID_SAINT_PETERSBURG}), ());
  TEST_EQUAL(GetTopLocalities(1 /* limit */), Ids({ID_MAY}), ());
}

UNIT_CLASS_TEST(LocalityScorerTest, PrefixMatch)
{
  enum
  {
    ID_SAN_ANTONIO,
    ID_NEW_YORK,
    ID_YORK,
    ID_MOSCOW
  };

  InitParams("New York San Anto", true /* lastTokenIsPrefix */);

  AddLocality("San Antonio", ID_SAN_ANTONIO);
  AddLocality("New York", ID_NEW_YORK);
  AddLocality("York", ID_YORK);
  AddLocality("Moscow", ID_MOSCOW);

  // All localities except Moscow match to the search query.
  TEST_EQUAL(GetTopLocalities(100 /* limit */), Ids({ID_SAN_ANTONIO, ID_NEW_YORK, ID_YORK}), ());
  TEST_EQUAL(GetTopLocalities(2 /* limit */), Ids({ID_SAN_ANTONIO, ID_NEW_YORK}), ());
  TEST_EQUAL(GetTopLocalities(1 /* limit */), Ids({ID_SAN_ANTONIO}), ());
}

UNIT_CLASS_TEST(LocalityScorerTest, Ranks)
{
  enum
  {
    ID_SAN_MARINO,
    ID_SAN_ANTONIO,
    ID_SAN_FRANCISCO
  };

  AddLocality("San Marino", ID_SAN_MARINO, 10 /* rank */);
  AddLocality("Citta di San Antonio", ID_SAN_ANTONIO, 20 /* rank */);
  AddLocality("San Francisco", ID_SAN_FRANCISCO, 30 /* rank */);

  InitParams("San", false /* lastTokenIsPrefix */);
  TEST_EQUAL(GetTopLocalities(100 /* limit */),
             Ids({ID_SAN_MARINO, ID_SAN_ANTONIO, ID_SAN_FRANCISCO}), ());
  TEST_EQUAL(GetTopLocalities(2 /* limit */), Ids({ID_SAN_MARINO, ID_SAN_FRANCISCO}), ());
  TEST_EQUAL(GetTopLocalities(1 /* limit */), Ids({ID_SAN_FRANCISCO}), ());
}
