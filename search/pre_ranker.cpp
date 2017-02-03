#include "search/pre_ranker.hpp"

#include "search/dummy_rank_table.hpp"
#include "search/lazy_centers_table.hpp"
#include "search/nearby_points_sweeper.hpp"
#include "search/pre_ranking_info.hpp"

#include "indexer/mwm_set.hpp"
#include "indexer/rank_table.hpp"
#include "indexer/scales.hpp"

#include "base/stl_helpers.hpp"

#include "std/iterator.hpp"

namespace search
{
namespace
{
struct LessFeatureID
{
  using TValue = PreResult1;

  inline bool operator()(TValue const & lhs, TValue const & rhs) const
  {
    return lhs.GetId() < rhs.GetId();
  }
};

// Orders PreResult1 by following criterion:
// 1. Feature Id (increasing), if same...
// 2. Number of matched tokens from the query (decreasing), if same...
// 3. Index of the first matched token from the query (increasing).
struct ComparePreResult1
{
  bool operator()(PreResult1 const & lhs, PreResult1 const & rhs) const
  {
    if (lhs.GetId() != rhs.GetId())
      return lhs.GetId() < rhs.GetId();

    auto const & linfo = lhs.GetInfo();
    auto const & rinfo = rhs.GetInfo();
    if (linfo.GetNumTokens() != rinfo.GetNumTokens())
      return linfo.GetNumTokens() > rinfo.GetNumTokens();
    return linfo.m_tokenRange.m_begin < rinfo.m_tokenRange.m_begin;
  }
};

// Selects a fair random subset of size min(|n|, |k|) from [0, 1, 2, ..., n - 1].
vector<size_t> RandomSample(size_t n, size_t k, minstd_rand & rng)
{
  vector<size_t> result(std::min(k, n));
  iota(result.begin(), result.end(), 0);

  for (size_t i = k; i < n; ++i)
  {
    size_t const j = rng() % (i + 1);
    if (j < k)
      result[j] = i;
  }

  return result;
}

void SweepNearbyResults(double eps, vector<PreResult1> & results)
{
  NearbyPointsSweeper sweeper(eps);
  for (size_t i = 0; i < results.size(); ++i)
  {
    auto const & p = results[i].GetInfo().m_center;
    sweeper.Add(p.x, p.y, i);
  }

  vector<PreResult1> filtered;
  sweeper.Sweep([&filtered, &results](size_t i)
                {
                  filtered.push_back(results[i]);
                });

  results.swap(filtered);
}
}  // namespace

PreRanker::PreRanker(Index const & index, Ranker & ranker, size_t limit)
  : m_index(index), m_ranker(ranker), m_limit(limit), m_pivotFeatures(index)
{
}

void PreRanker::Init(Params const & params)
{
  m_numSentResults = 0;
  m_results.clear();
  m_params = params;
  m_currEmit.clear();
}

void PreRanker::FillMissingFieldsInPreResults()
{
  MwmSet::MwmId mwmId;
  MwmSet::MwmHandle mwmHandle;
  unique_ptr<RankTable> ranks = make_unique<DummyRankTable>();
  unique_ptr<LazyCentersTable> centers;

  bool const fillCenters = (Size() > BatchSize());

  if (fillCenters)
    m_pivotFeatures.SetPosition(m_params.m_accuratePivotCenter, m_params.m_scale);

  ForEach([&](PreResult1 & r) {
    FeatureID const & id = r.GetId();
    PreRankingInfo & info = r.GetInfo();
    if (id.m_mwmId != mwmId)
    {
      mwmId = id.m_mwmId;
      mwmHandle = m_index.GetMwmHandleById(mwmId);
      ranks.reset();
      centers.reset();
      if (mwmHandle.IsAlive())
      {
        ranks = RankTable::Load(mwmHandle.GetValue<MwmValue>()->m_cont);
        centers = make_unique<LazyCentersTable>(*mwmHandle.GetValue<MwmValue>());
      }
      if (!ranks)
        ranks = make_unique<DummyRankTable>();
    }

    info.m_rank = ranks->Get(id.m_index);

    if (fillCenters)
    {
      m2::PointD center;
      if (centers && centers->Get(id.m_index, center))
      {
        info.m_distanceToPivot =
            MercatorBounds::DistanceOnEarth(m_params.m_accuratePivotCenter, center);
        info.m_center = center;
        info.m_centerLoaded = true;
      }
      else
      {
        info.m_distanceToPivot = m_pivotFeatures.GetDistanceToFeatureMeters(id);
      }
    }
  });
}

void PreRanker::Filter(bool viewportSearch)
{
  using TSet = set<PreResult1, LessFeatureID>;
  TSet filtered;

  sort(m_results.begin(), m_results.end(), ComparePreResult1());
  m_results.erase(unique(m_results.begin(), m_results.end(), my::EqualsBy(&PreResult1::GetId)),
                  m_results.end());

  if (m_results.size() > BatchSize())
  {
    bool const centersLoaded =
        all_of(m_results.begin(), m_results.end(),
               [](PreResult1 const & result) { return result.GetInfo().m_centerLoaded; });
    if (viewportSearch && centersLoaded)
    {
      FilterForViewportSearch();
      ASSERT_LESS_OR_EQUAL(m_results.size(), BatchSize(), ());
      for (auto const & result : m_results)
        m_currEmit.insert(result.GetId());
    }
    else
    {
      sort(m_results.begin(), m_results.end(), &PreResult1::LessDistance);

      // Priority is some kind of distance from the viewport or
      // position, therefore if we have a bunch of results with the same
      // priority, we have no idea here which results are relevant.  To
      // prevent bias from previous search routines (like sorting by
      // feature id) this code randomly selects tail of the
      // sorted-by-priority list of pre-results.

      double const last = m_results[BatchSize()].GetDistance();

      auto b = m_results.begin() + BatchSize();
      for (; b != m_results.begin() && b->GetDistance() == last; --b)
        ;
      if (b->GetDistance() != last)
        ++b;

      auto e = m_results.begin() + BatchSize();
      for (; e != m_results.end() && e->GetDistance() == last; ++e)
        ;

      // The main reason of shuffling here is to select a random subset
      // from the low-priority results. We're using a linear
      // congruential method with default seed because it is fast,
      // simple and doesn't need an external entropy source.
      //
      // TODO (@y, @m, @vng): consider to take some kind of hash from
      // features and then select a subset with smallest values of this
      // hash.  In this case this subset of results will be persistent
      // to small changes in the original set.
      shuffle(b, e, m_rng);
    }
  }
  filtered.insert(m_results.begin(), m_results.begin() + min(m_results.size(), BatchSize()));

  if (!viewportSearch)
  {
    size_t n = min(m_results.size(), BatchSize());
    nth_element(m_results.begin(), m_results.begin() + n, m_results.end(), &PreResult1::LessRank);
    filtered.insert(m_results.begin(), m_results.begin() + n);
  }

  m_results.assign(filtered.begin(), filtered.end());
}

void PreRanker::UpdateResults(bool lastUpdate)
{
  FillMissingFieldsInPreResults();
  Filter(m_viewportSearch);
  m_numSentResults += m_results.size();
  m_ranker.SetPreResults1(move(m_results));
  m_results.clear();
  m_ranker.UpdateResults(lastUpdate);

  if (lastUpdate)
    m_currEmit.swap(m_prevEmit);
}

void PreRanker::ClearCaches()
{
  m_pivotFeatures.Clear();
  m_prevEmit.clear();
  m_currEmit.clear();
}

void PreRanker::FilterForViewportSearch()
{
  auto const & viewport = m_params.m_viewport;

  my::EraseIf(m_results, [&viewport](PreResult1 const & result) {
    auto const & info = result.GetInfo();
    return !viewport.IsPointInside(info.m_center);
  });

  SweepNearbyResults(m_params.m_minDistanceOnMapBetweenResults, m_results);

  size_t const n = m_results.size();

  if (n <= BatchSize())
    return;

  size_t const kNumXSlots = 5;
  size_t const kNumYSlots = 5;
  size_t const kNumBuckets = kNumXSlots * kNumYSlots;
  vector<size_t> buckets[kNumBuckets];

  double const sizeX = viewport.SizeX();
  double const sizeY = viewport.SizeY();

  for (size_t i = 0; i < m_results.size(); ++i)
  {
    auto const & p = m_results[i].GetInfo().m_center;
    int dx = static_cast<int>((p.x - viewport.minX()) / sizeX * kNumXSlots);
    dx = my::clamp(dx, 0, static_cast<int>(kNumXSlots) - 1);

    int dy = static_cast<int>((p.y - viewport.minY()) / sizeY * kNumYSlots);
    dy = my::clamp(dy, 0, static_cast<int>(kNumYSlots) - 1);

    buckets[dx * kNumYSlots + dy].push_back(i);
  }

  vector<PreResult1> results;
  double const density = static_cast<double>(BatchSize()) / static_cast<double>(n);
  for (auto & bucket : buckets)
  {
    size_t const m = std::min(static_cast<size_t>(ceil(density * bucket.size())), bucket.size());

    size_t const old =
        partition(bucket.begin(), bucket.end(),
                  [this](size_t i) { return m_prevEmit.count(m_results[i].GetId()) != 0; }) -
        bucket.begin();

    if (m <= old)
    {
      for (size_t i : RandomSample(old, m, m_rng))
        results.push_back(m_results[bucket[i]]);
    }
    else
    {
      for (size_t i = 0; i < old; ++i)
        results.push_back(m_results[bucket[i]]);

      for (size_t i : RandomSample(bucket.size() - old, m - old, m_rng))
        results.push_back(m_results[bucket[old + i]]);
    }
  }

  if (results.size() <= BatchSize())
  {
    m_results.swap(results);
  }
  else
  {
    m_results.clear();
    for (size_t i : RandomSample(results.size(), BatchSize(), m_rng))
      m_results.push_back(results[i]);
  }
}
}  // namespace search
