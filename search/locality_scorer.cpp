#include "search/locality_scorer.hpp"

#include "search/cbv.hpp"
#include "search/geocoder_context.hpp"
#include "search/token_slice.hpp"

#include <algorithm>
#include <sstream>
#include <unordered_set>

namespace search
{
// static
size_t const LocalityScorer::kDefaultReadLimit = 100;

namespace
{
bool IsAlmostFullMatch(NameScore score)
{
  return score == NAME_SCORE_FULL_MATCH_PREFIX || score == NAME_SCORE_FULL_MATCH;
}
}  // namespace

// LocalityScorer::ExLocality ----------------------------------------------------------------------
LocalityScorer::ExLocality::ExLocality() : m_numTokens(0), m_rank(0), m_nameScore(NAME_SCORE_ZERO)
{
}

LocalityScorer::ExLocality::ExLocality(Geocoder::Locality const & locality)
  : m_locality(locality)
  , m_numTokens(locality.m_endToken - locality.m_startToken)
  , m_rank(0)
  , m_nameScore(NAME_SCORE_ZERO)
{
}

// LocalityScorer ----------------------------------------------------------------------------------
LocalityScorer::LocalityScorer(QueryParams const & params, Delegate const & delegate)
  : m_params(params), m_delegate(delegate)
{
}

void LocalityScorer::GetTopLocalities(MwmSet::MwmId const & countryId, BaseContext const & ctx,
                                      CBV const & filter, size_t limit,
                                      std::vector<Geocoder::Locality> & localities)
{
  CHECK_EQUAL(ctx.m_numTokens, m_params.GetNumTokens(), ());

  localities.clear();

  for (size_t startToken = 0; startToken < ctx.m_numTokens; ++startToken)
  {
    CBV intersection = filter.Intersect(ctx.m_features[startToken]);
    if (intersection.IsEmpty())
      continue;

    double idf = 1.0 / intersection.PopCount();

    for (size_t endToken = startToken + 1; endToken <= ctx.m_numTokens; ++endToken)
    {
      // Skip locality candidates that match only numbers.
      if (!m_params.IsNumberTokens(startToken, endToken))
      {
        intersection.ForEach([&](uint32_t featureId) {
          Geocoder::Locality l;
          l.m_countryId = countryId;
          l.m_featureId = featureId;
          l.m_startToken = startToken;
          l.m_endToken = endToken;
          l.m_tfidf = idf;
          localities.push_back(l);
        });
      }

      if (endToken < ctx.m_numTokens)
      {
        auto const cur = filter.Intersect(ctx.m_features[endToken]);
        if (cur.IsEmpty())
          break;

        idf += 1.0 / cur.PopCount();

        intersection = intersection.Intersect(cur);
        if (intersection.IsEmpty())
          break;
      }
    }
  }

  LeaveTopLocalities(limit, localities);
}

void LocalityScorer::LeaveTopLocalities(size_t limit,
                                        std::vector<Geocoder::Locality> & localities) const
{
  std::vector<ExLocality> ls;
  ls.reserve(localities.size());
  for (auto const & locality : localities)
    ls.emplace_back(locality);

  LeaveTopByRankAndTfIdf(std::max(limit, kDefaultReadLimit), ls);
  SortByNameAndTfIdf(ls);
  if (ls.size() > limit)
    ls.resize(limit);

  localities.clear();
  localities.reserve(ls.size());
  for (auto const & l : ls)
    localities.push_back(l.m_locality);
}

void LocalityScorer::LeaveTopByRankAndTfIdf(size_t limit, std::vector<ExLocality> & ls) const
{
  for (auto & l : ls)
    l.m_rank = m_delegate.GetRank(l.GetId());

  std::sort(ls.begin(), ls.end(), [](ExLocality const & lhs, ExLocality const & rhs) {
    if (lhs.m_locality.m_tfidf != rhs.m_locality.m_tfidf)
      return lhs.m_locality.m_tfidf > rhs.m_locality.m_tfidf;
    return lhs.m_rank > rhs.m_rank;
  });

  std::vector<ExLocality> rs;
  std::unordered_set<uint32_t> ids;

  for (size_t i = 0; i < ls.size() && rs.size() < limit; ++i)
  {
    bool const inserted = ids.insert(ls[i].GetId()).second;
    if (inserted)
      rs.push_back(ls[i]);
  }

  ASSERT_LESS_OR_EQUAL(rs.size(), limit, ());
  rs.swap(ls);
}

void LocalityScorer::SortByNameAndTfIdf(std::vector<ExLocality> & ls) const
{
  std::vector<std::string> names;
  for (auto & l : ls)
  {
    names.clear();
    m_delegate.GetNames(l.GetId(), names);

    auto score = NAME_SCORE_ZERO;
    for (auto const & name : names)
    {
      score = max(score, GetNameScore(name, TokenSlice(m_params, l.m_locality.m_startToken,
                                                       l.m_locality.m_endToken)));
    }
    l.m_nameScore = score;
  }

  std::sort(ls.begin(), ls.end(), [](ExLocality const & lhs, ExLocality const & rhs) {
    if (IsAlmostFullMatch(lhs.m_nameScore) && IsAlmostFullMatch(rhs.m_nameScore))
    {
      // When both localities match well, e.g. full or full prefix
      // match, the one with larger number of tokens is selected. In
      // case of tie, the one with better score is selected.
      if (lhs.m_locality.m_tfidf != rhs.m_locality.m_tfidf)
        return lhs.m_locality.m_tfidf > rhs.m_locality.m_tfidf;
      if (lhs.m_nameScore != rhs.m_nameScore)
        return lhs.m_nameScore > rhs.m_nameScore;
    }
    else
    {
      // When name scores differ, the one with better name score is
      // selected.  In case of tie, the one with larger number of
      // matched tokens is selected.
      if (lhs.m_nameScore != rhs.m_nameScore)
        return lhs.m_nameScore > rhs.m_nameScore;

      if (lhs.m_locality.m_tfidf != rhs.m_locality.m_tfidf)
        return lhs.m_locality.m_tfidf > rhs.m_locality.m_tfidf;
    }

    if (lhs.m_rank != rhs.m_rank)
      return lhs.m_rank > rhs.m_rank;

    return lhs.m_locality.m_featureId < rhs.m_locality.m_featureId;
  });
}

string DebugPrint(LocalityScorer::ExLocality const & locality)
{
  ostringstream os;
  os << "LocalityScorer::ExLocality [ ";
  os << "m_locality=" << DebugPrint(locality.m_locality) << ", ";
  os << "m_numTokens=" << locality.m_numTokens << ", ";
  os << "m_rank=" << static_cast<uint32_t>(locality.m_rank) << ", ";
  os << "m_nameScore=" << DebugPrint(locality.m_nameScore);
  os << " ]";
  return os.str();
}
}  // namespace search
