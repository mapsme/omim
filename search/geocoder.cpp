#include "search/geocoder.hpp"

#include "search/cbv.hpp"
#include "search/dummy_rank_table.hpp"
#include "search/features_filter.hpp"
#include "search/features_layer_matcher.hpp"
#include "search/house_numbers_matcher.hpp"
#include "search/locality_scorer.hpp"
#include "search/pre_ranker.hpp"
#include "search/processor.hpp"
#include "search/retrieval.hpp"
#include "search/token_slice.hpp"
#include "search/tracer.hpp"
#include "search/utils.hpp"

#include "indexer/classificator.hpp"
#include "indexer/data_source.hpp"
#include "indexer/feature_decl.hpp"
#include "indexer/feature_impl.hpp"
#include "indexer/ftypes_matcher.hpp"
#include "indexer/postcodes_matcher.hpp"
#include "indexer/rank_table.hpp"
#include "indexer/search_delimiters.hpp"
#include "indexer/search_string_utils.hpp"
#include "indexer/utils.hpp"

#include "storage/country_info_getter.hpp"

#include "coding/string_utf8_multilang.hpp"

#include "platform/preferred_languages.hpp"

#include "geometry/mercator.hpp"

#include "base/assert.hpp"
#include "base/checked_cast.hpp"
#include "base/control_flow.hpp"
#include "base/logging.hpp"
#include "base/macros.hpp"
#include "base/pprof.hpp"
#include "base/random.hpp"
#include "base/scope_guard.hpp"
#include "base/stl_helpers.hpp"

#include <algorithm>
#include <functional>
#include <iterator>
#include <random>
#include <sstream>

#include "defines.hpp"

#if defined(DEBUG)
#include "base/timer.hpp"
#endif

using namespace std;
using namespace strings;

namespace search
{
namespace
{
size_t constexpr kMaxNumCities = 10;
size_t constexpr kMaxNumStates = 10;
size_t constexpr kMaxNumVillages = 5;
size_t constexpr kMaxNumCountries = 10;
double constexpr kMaxViewportRadiusM = 50.0 * 1000;
double constexpr kMaxPostcodeRadiusM = 1000;
double constexpr kMaxSuburbRadiusM = 2000;
double constexpr kMaxNeighbourhoodRadiusM = 500;
double constexpr kMaxResidentialRadiusM = 500;

size_t constexpr kPivotRectsCacheSize = 10;
size_t constexpr kPostcodesRectsCacheSize = 10;
size_t constexpr kSuburbsRectsCacheSize = 10;
size_t constexpr kLocalityRectsCacheSize = 10;

UniString const kUniSpace(MakeUniString(" "));

struct ScopedMarkTokens
{
  static BaseContext::TokenType constexpr kUnused = BaseContext::TOKEN_TYPE_COUNT;

  ScopedMarkTokens(vector<BaseContext::TokenType> & tokens, BaseContext::TokenType type,
                   TokenRange const & range)
    : m_tokens(tokens), m_type(type), m_range(range)
  {
    ASSERT(m_range.IsValid(), ());
    ASSERT_LESS_OR_EQUAL(m_range.End(), m_tokens.size(), ());
#if defined(DEBUG)
    for (size_t i : m_range)
      ASSERT_EQUAL(m_tokens[i], kUnused, (i));
#endif
    fill(m_tokens.begin() + m_range.Begin(), m_tokens.begin() + m_range.End(), m_type);
  }

  ~ScopedMarkTokens()
  {
#if defined(DEBUG)
    for (size_t i : m_range)
      ASSERT_EQUAL(m_tokens[i], m_type, (i));
#endif
    fill(m_tokens.begin() + m_range.Begin(), m_tokens.begin() + m_range.End(), kUnused);
  }

  vector<search::BaseContext::TokenType> & m_tokens;
  search::BaseContext::TokenType const m_type;
  TokenRange const m_range;
};

// static
BaseContext::TokenType constexpr ScopedMarkTokens::kUnused;

class LazyRankTable : public RankTable
{
public:
  explicit LazyRankTable(MwmValue const & value) : m_value(value) {}

  uint8_t Get(uint64_t i) const override
  {
    EnsureTableLoaded();
    return m_table->Get(i);
  }

  uint64_t Size() const override
  {
    EnsureTableLoaded();
    return m_table->Size();
  }

  RankTable::Version GetVersion() const override
  {
    EnsureTableLoaded();
    return m_table->GetVersion();
  }

  void Serialize(Writer & writer, bool preserveHostEndiannes) override
  {
    EnsureTableLoaded();
    m_table->Serialize(writer, preserveHostEndiannes);
  }

private:
  void EnsureTableLoaded() const
  {
    if (m_table)
      return;
    m_table = search::RankTable::Load(m_value.m_cont, SEARCH_RANKS_FILE_TAG);
    if (!m_table)
      m_table = make_unique<search::DummyRankTable>();
  }

  MwmValue const & m_value;
  mutable unique_ptr<search::RankTable> m_table;
};

class LocalityScorerDelegate : public LocalityScorer::Delegate
{
public:
  LocalityScorerDelegate(MwmContext & context, Geocoder::Params const & params,
                         function<bool(m2::PointD const &)> const & belongsToMatchedRegionFn,
                         base::Cancellable const & cancellable)
    : m_context(context)
    , m_params(params)
    , m_cancellable(cancellable)
    , m_belongsToMatchedRegionFn(belongsToMatchedRegionFn)
    , m_retrieval(m_context, m_cancellable)
    , m_ranks(m_context.m_value)
  {
    ASSERT(m_belongsToMatchedRegionFn, ());
  }

  // LocalityScorer::Delegate overrides:
  void GetNames(uint32_t featureId, vector<string> & names) const override
  {
    auto ft = m_context.GetFeature(featureId);
    if (!ft)
      return;
    for (auto const lang : m_params.GetLangs())
    {
      string name;
      if (ft->GetName(lang, name))
        names.push_back(name);
    }
  }

  uint8_t GetRank(uint32_t featureId) const override { return m_ranks.Get(featureId); }

  optional<m2::PointD> GetCenter(uint32_t featureId) override
  {
    m2::PointD center;
    // m_context->GetCenter is faster but may not work for editor created features.
    if (!m_context.GetCenter(featureId, center))
    {
      auto ft = m_context.GetFeature(featureId);
      if (!ft)
        return {};

      center = feature::GetCenter(*ft);
    }

    return center;
  }

  bool BelongsToMatchedRegion(m2::PointD const & p) const override
  {
    ASSERT(m_belongsToMatchedRegionFn, ());
    return m_belongsToMatchedRegionFn(p);
  }

private:
  MwmContext & m_context;
  Geocoder::Params const & m_params;
  base::Cancellable const & m_cancellable;
  function<bool(m2::PointD const &)> m_belongsToMatchedRegionFn;

  Retrieval m_retrieval;

  LazyRankTable m_ranks;
};

void JoinQueryTokens(QueryParams const & params, TokenRange const & range, UniString const & sep,
                     UniString & res)
{
  ASSERT(range.IsValid(), (range));
  for (size_t i : range)
  {
    res.append(params.GetToken(i).GetOriginal());
    if (i + 1 != range.End())
      res.append(sep);
  }
}

WARN_UNUSED_RESULT bool GetAffiliationName(FeatureType & ft, string & affiliation)
{
  affiliation.clear();

  if (ft.GetName(StringUtf8Multilang::kDefaultCode, affiliation) && !affiliation.empty())
    return true;

  // As a best effort, we try to read an english name if default name is absent.
  if (ft.GetName(StringUtf8Multilang::kEnglishCode, affiliation) && !affiliation.empty())
    return true;
  return false;
}

double Area(m2::RectD const & rect) { return rect.IsValid() ? rect.SizeX() * rect.SizeY() : 0; }

// Computes the average similarity between |rect| and |pivot|. By
// similarity between two rects we mean a fraction of the area of
// rects intersection to the area of the smallest rect.
double GetSimilarity(m2::RectD const & pivot, m2::RectD const & rect)
{
  double const area = min(Area(pivot), Area(rect));
  if (area == 0.0)
    return 0.0;
  m2::RectD p = pivot;
  if (!p.Intersect(rect))
    return 0.0;
  return Area(p) / area;
}

// Returns shortest distance from the |pivot| to the |rect|.
//
// *NOTE* calculations below are incorrect, because shortest distance
// on the Mercator's plane is not the same as shortest distance on the
// Earth. But we assume that it is not an issue here.
double GetDistanceMeters(m2::PointD const & pivot, m2::RectD const & rect)
{
  if (rect.IsPointInside(pivot))
    return 0.0;

  double distance = numeric_limits<double>::max();

  rect.ForEachSide([&](m2::PointD const & a, m2::PointD const & b) {
    m2::ParametrizedSegment<m2::PointD> segment(a, b);
    distance = min(distance, mercator::DistanceOnEarth(pivot, segment.ClosestPointTo(pivot)));
  });

  return distance;
}

unique_ptr<MwmContext> GetWorldContext(DataSource const & dataSource)
{
  vector<shared_ptr<MwmInfo>> infos;
  dataSource.GetMwmsInfo(infos);
  MwmSet::MwmHandle handle = indexer::FindWorld(dataSource, infos);
  if (handle.IsAlive())
    return make_unique<MwmContext>(move(handle));
  return {};
}

#define TRACE(branch)                                      \
  m_resultTracer.CallMethod(ResultTracer::Branch::branch); \
  SCOPE_GUARD(tracerGuard, [&] { m_resultTracer.LeaveMethod(ResultTracer::Branch::branch); });
}  // namespace

// Geocoder::ExtendedMwmInfos::ExtendedMwmInfo -----------------------------------------------------
bool Geocoder::ExtendedMwmInfos::ExtendedMwmInfo::operator<(
    Geocoder::ExtendedMwmInfos::ExtendedMwmInfo const & rhs) const
{
  if (m_distance == 0.0 && rhs.m_distance == 0.0)
    return m_similarity > rhs.m_similarity;
  return m_distance < rhs.m_distance;
}

// Geocoder::LocalitiesCaches ----------------------------------------------------------------------
Geocoder::LocalitiesCaches::LocalitiesCaches(base::Cancellable const & cancellable)
  : m_countries(cancellable)
  , m_states(cancellable)
  , m_citiesTownsOrVillages(cancellable)
  , m_villages(cancellable)
{
}

void Geocoder::LocalitiesCaches::Clear()
{
  m_countries.Clear();
  m_states.Clear();
  m_citiesTownsOrVillages.Clear();
  m_villages.Clear();
}

// Geocoder::Geocoder ------------------------------------------------------------------------------
Geocoder::Geocoder(DataSource const & dataSource, storage::CountryInfoGetter const & infoGetter,
                   CategoriesHolder const & categories,
                   CitiesBoundariesTable const & citiesBoundaries, PreRanker & preRanker,
                   LocalitiesCaches & localitiesCaches, base::Cancellable const & cancellable)
  : m_dataSource(dataSource)
  , m_infoGetter(infoGetter)
  , m_categories(categories)
  , m_streetsCache(cancellable)
  , m_suburbsCache(cancellable)
  , m_localitiesCaches(localitiesCaches)
  , m_hotelsCache(cancellable)
  , m_foodCache(cancellable)
  , m_hotelsFilter(m_hotelsCache)
  , m_cuisineFilter(m_foodCache)
  , m_cancellable(cancellable)
  , m_citiesBoundaries(citiesBoundaries)
  , m_pivotRectsCache(kPivotRectsCacheSize, m_cancellable, kMaxViewportRadiusM)
  , m_postcodesRectsCache(kPostcodesRectsCacheSize, m_cancellable, kMaxPostcodeRadiusM)
  , m_suburbsRectsCache(kSuburbsRectsCacheSize, m_cancellable, kMaxSuburbRadiusM)
  , m_localityRectsCache(kLocalityRectsCacheSize, m_cancellable)
  , m_filter(nullptr)
  , m_matcher(nullptr)
  , m_finder(m_cancellable)
  , m_preRanker(preRanker)
{
}

Geocoder::~Geocoder() {}

void Geocoder::SetParams(Params const & params)
{
  if (params.IsCategorialRequest())
  {
    SetParamsForCategorialSearch(params);
    return;
  }

  m_params = params;

  m_tokenRequests.clear();
  m_prefixTokenRequest.Clear();
  for (size_t i = 0; i < m_params.GetNumTokens(); ++i)
  {
    if (!m_params.IsPrefixToken(i))
    {
      m_tokenRequests.emplace_back();
      auto & request = m_tokenRequests.back();
      FillRequestFromToken(m_params.GetToken(i), request);
      for (auto const & index : m_params.GetTypeIndices(i))
        request.m_categories.emplace_back(FeatureTypeToString(index));
      request.SetLangs(m_params.GetLangs());
    }
    else
    {
      auto & request = m_prefixTokenRequest;
      FillRequestFromToken(m_params.GetToken(i), request);
      for (auto const & index : m_params.GetTypeIndices(i))
        request.m_categories.emplace_back(FeatureTypeToString(index));
      request.SetLangs(m_params.GetLangs());
    }
  }

  m_resultTracer.Clear();

  LOG(LDEBUG, (static_cast<QueryParams const &>(m_params)));
}

void Geocoder::GoEverywhere()
{
// TODO (@y): remove following code as soon as Geocoder::Go() will
// work fast for most cases (significantly less than 1 second).
#if defined(DEBUG)
  base::Timer timer;
  SCOPE_GUARD(printDuration, [&timer]() {
    LOG(LINFO, ("Total geocoding time:", timer.ElapsedSeconds(), "seconds"));
  });
#endif

  TRACE(GoEverywhere);

  if (m_params.GetNumTokens() == 0)
    return;

  vector<shared_ptr<MwmInfo>> infos;
  m_dataSource.GetMwmsInfo(infos);

  GoImpl(infos, false /* inViewport */);
}

void Geocoder::GoInViewport()
{
  TRACE(GoInViewport);

  if (m_params.GetNumTokens() == 0)
    return;

  vector<shared_ptr<MwmInfo>> infos;
  m_dataSource.GetMwmsInfo(infos);

  base::EraseIf(infos, [this](shared_ptr<MwmInfo> const & info) {
    return !m_params.m_pivot.IsIntersect(info->m_bordersRect);
  });

  GoImpl(infos, true /* inViewport */);
}

void Geocoder::Finish(bool cancelled)
{
  m_preRanker.Finish(cancelled);
}

void Geocoder::ClearCaches()
{
  m_pivotRectsCache.Clear();
  m_localityRectsCache.Clear();

  m_matchersCache.clear();
  m_streetsCache.Clear();
  m_hotelsCache.Clear();
  m_foodCache.Clear();
  m_hotelsFilter.ClearCaches();
  m_cuisineFilter.ClearCaches();
  m_postcodePointsCache.Clear();
  m_postcodes.Clear();
}

void Geocoder::SetParamsForCategorialSearch(Params const & params)
{
  m_params = params;

  m_tokenRequests.clear();
  m_prefixTokenRequest.Clear();

  LOG(LDEBUG, (static_cast<QueryParams const &>(m_params)));
}

Geocoder::ExtendedMwmInfos::ExtendedMwmInfo Geocoder::GetExtendedMwmInfo(
    shared_ptr<MwmInfo> const & info, bool inViewport,
    function<bool(shared_ptr<MwmInfo> const &)> const & isMwmWithMatchedCity,
    function<bool(shared_ptr<MwmInfo> const &)> const & isMwmWithMatchedState) const
{
  ExtendedMwmInfos::ExtendedMwmInfo extendedInfo;
  extendedInfo.m_info = info;

  auto const & rect = info->m_bordersRect;
  extendedInfo.m_type.m_viewportIntersected = m_params.m_pivot.IsIntersect(rect);
  extendedInfo.m_type.m_containsUserPosition =
      m_params.m_position && rect.IsPointInside(*m_params.m_position);
  extendedInfo.m_type.m_containsMatchedCity = isMwmWithMatchedCity(info);
  extendedInfo.m_type.m_containsMatchedState = isMwmWithMatchedState(info);

  extendedInfo.m_similarity = GetSimilarity(m_params.m_pivot, rect);
  if (!inViewport && extendedInfo.m_type.m_containsUserPosition)
    extendedInfo.m_distance = 0.0;
  else
    extendedInfo.m_distance = GetDistanceMeters(m_params.m_pivot.Center(), rect);
  return extendedInfo;
}

Geocoder::ExtendedMwmInfos Geocoder::OrderCountries(bool inViewport,
                                                    vector<shared_ptr<MwmInfo>> const & infos)
{
  set<storage::CountryId> mwmsWithCities;
  set<storage::CountryId> mwmsWithStates;
  if (!inViewport)
  {
    for (auto const & p : m_cities)
    {
      for (auto const & city : p.second)
        mwmsWithCities.insert(m_infoGetter.GetRegionCountryId(city.m_rect.Center()));
    }
    for (auto const & p : m_regions[Region::TYPE_STATE])
    {
      for (auto const & state : p.second)
      {
        mwmsWithStates.insert(m_infoGetter.GetRegionCountryId(state.m_center));
      }
    }
  }

  ExtendedMwmInfos res;
  res.m_infos.reserve(infos.size());
  auto const hasMatchedCity = [&mwmsWithCities](auto const & i) {
    return mwmsWithCities.count(i->GetCountryName()) != 0;
  };
  auto const hasMatchedState = [&mwmsWithStates](auto const & i) {
    return mwmsWithStates.count(i->GetCountryName()) != 0;
  };
  for (auto const & info : infos)
  {
    res.m_infos.push_back(GetExtendedMwmInfo(info, inViewport, hasMatchedCity, hasMatchedState));
  }
  sort(res.m_infos.begin(), res.m_infos.end());

  auto const firstBatch = [&](auto const & extendedInfo) {
    return extendedInfo.m_type.IsFirstBatchMwm(inViewport);
  };
  auto const sep = stable_partition(res.m_infos.begin(), res.m_infos.end(), firstBatch);
  res.m_firstBatchSize = distance(res.m_infos.begin(), sep);
  return res;
}

void Geocoder::GoImpl(vector<shared_ptr<MwmInfo>> const & infos, bool inViewport)
{
  // base::PProf pprof("/tmp/geocoder.prof");

  // Tries to find world and fill localities table.
  {
    m_cities.clear();
    for (auto & regions : m_regions)
      regions.clear();
    MwmSet::MwmHandle handle = indexer::FindWorld(m_dataSource, infos);
    if (handle.IsAlive())
    {
      auto & value = *handle.GetValue();

      // All MwmIds are unique during the application lifetime, so
      // it's ok to save MwmId.
      m_worldId = handle.GetId();
      m_context = make_unique<MwmContext>(move(handle));

      if (value.HasSearchIndex())
      {
        BaseContext ctx;
        InitBaseContext(ctx);
        FillLocalitiesTable(ctx);
      }

      m_context.reset();
    }
  }

  // Orders countries by distance from viewport center and position.
  // This order is used during MatchAroundPivot() stage - we try to
  // match as many features as possible without trying to match
  // locality (COUNTRY or CITY), and only when there are too many
  // features, viewport and position vicinity filter is used.  To
  // prevent full search in all mwms, we need to limit somehow a set
  // of mwms for MatchAroundPivot(), so, we always call
  // MatchAroundPivot() on maps intersecting with pivot rect, other
  // maps are ordered by distance from pivot, and we stop to call
  // MatchAroundPivot() on them as soon as at least one feature is
  // found.
  auto const infosWithType = OrderCountries(inViewport, infos);

  // MatchAroundPivot() should always be matched in mwms
  // intersecting with position and viewport.
  auto processCountry = [&](unique_ptr<MwmContext> context, bool updatePreranker) {
    ASSERT(context, ());
    m_context = move(context);

    SCOPE_GUARD(cleanup, [&]() {
      LOG(LDEBUG, (m_context->GetName(), "geocoding complete."));
      m_matcher->OnQueryFinished();
      m_matcher = nullptr;
      m_context.reset();
    });

    auto it = m_matchersCache.find(m_context->GetId());
    if (it == m_matchersCache.end())
    {
      it = m_matchersCache
               .insert(make_pair(m_context->GetId(),
                                 std::make_unique<FeaturesLayerMatcher>(m_dataSource, m_cancellable)))
               .first;
    }
    m_matcher = it->second.get();
    m_matcher->SetContext(m_context.get());

    BaseContext ctx;
    InitBaseContext(ctx);

    if (inViewport)
    {
      auto const viewportCBV =
          RetrieveGeometryFeatures(*m_context, m_params.m_pivot, RectId::Pivot);
      for (auto & features : ctx.m_features)
        features = features.Intersect(viewportCBV);
    }

    ctx.m_villages = m_localitiesCaches.m_villages.Get(*m_context);

    auto citiesFromWorld = m_cities;
    FillVillageLocalities(ctx);
    SCOPE_GUARD(remove_villages, [&]() { m_cities = citiesFromWorld; });

    if (m_params.IsCategorialRequest())
    {
      auto const mwmType = m_context->GetType();
      CHECK(mwmType, ());
      MatchCategories(ctx, mwmType->m_viewportIntersected /* aroundPivot */);
    }
    else
    {
      MatchRegions(ctx, Region::TYPE_COUNTRY);

      auto const mwmType = m_context->GetType();
      CHECK(mwmType, ());
      if (mwmType->m_viewportIntersected || mwmType->m_containsUserPosition ||
          !m_preRanker.HaveFullyMatchedResult())
      {
        MatchAroundPivot(ctx);
      }
    }

    if (updatePreranker)
      m_preRanker.UpdateResults(false /* lastUpdate */);

    if (m_preRanker.IsFull())
      return base::ControlFlow::Break;

    return base::ControlFlow::Continue;
  };

  // Iterates through all alive mwms and performs geocoding.
  ForEachCountry(infosWithType, processCountry);
}

void Geocoder::InitBaseContext(BaseContext & ctx)
{
  Retrieval retrieval(*m_context, m_cancellable);

  ctx.m_tokens.assign(m_params.GetNumTokens(), BaseContext::TOKEN_TYPE_COUNT);
  ctx.m_numTokens = m_params.GetNumTokens();
  ctx.m_features.resize(ctx.m_numTokens);
  for (size_t i = 0; i < ctx.m_features.size(); ++i)
  {
    if (m_params.IsCategorialRequest())
    {
      // Implementation-wise, the simplest way to match a feature by
      // its category bypassing the matching by name is by using a CategoriesCache.
      CategoriesCache cache(m_params.m_preferredTypes, m_cancellable);
      ctx.m_features[i] = Retrieval::ExtendedFeatures(cache.Get(*m_context));
    }
    else if (m_params.IsPrefixToken(i))
    {
      ctx.m_features[i] = retrieval.RetrieveAddressFeatures(m_prefixTokenRequest);
    }
    else
    {
      ctx.m_features[i] = retrieval.RetrieveAddressFeatures(m_tokenRequests[i]);
    }
  }

  ctx.m_hotelsFilter = m_hotelsFilter.MakeScopedFilter(*m_context, m_params.m_hotelsFilter);
  ctx.m_cuisineFilter = m_cuisineFilter.MakeScopedFilter(*m_context, m_params.m_cuisineTypes);
}

void Geocoder::InitLayer(Model::Type type, TokenRange const & tokenRange, FeaturesLayer & layer)
{
  layer.Clear();
  layer.m_type = type;
  layer.m_tokenRange = tokenRange;

  JoinQueryTokens(m_params, layer.m_tokenRange, kUniSpace /* sep */, layer.m_subQuery);
  layer.m_lastTokenIsPrefix =
      !layer.m_tokenRange.Empty() && m_params.IsPrefixToken(layer.m_tokenRange.End() - 1);
}

void Geocoder::FillLocalityCandidates(BaseContext const & ctx, CBV const & filter,
                                      size_t const maxNumLocalities,
                                      vector<Locality> & preLocalities)
{
  // todo(@m) "food moscow" should be a valid categorial request.
  if (m_params.IsCategorialRequest())
  {
    preLocalities.clear();
    return;
  }

  storage::CountryInfoGetter::RegionIdVec ids;
  for (auto const & type : m_regions)
  {
    for (auto const & ranges : type)
    {
      for (auto const & regions : ranges.second)
        ids.insert(ids.end(), regions.m_ids.begin(), regions.m_ids.end());
    }
  }
  base::SortUnique(ids);

  auto const belongsToMatchedRegion = [&](m2::PointD const & point) {
    return m_infoGetter.BelongsToAnyRegion(point, ids);
  };

  LocalityScorerDelegate delegate(*m_context, m_params, belongsToMatchedRegion, m_cancellable);
  LocalityScorer scorer(m_params, m_params.m_pivot.Center(), delegate);
  scorer.GetTopLocalities(m_context->GetId(), ctx, filter, maxNumLocalities, preLocalities);
}

void Geocoder::CacheWorldLocalities()
{
  if (auto context = GetWorldContext(m_dataSource))
  {
    // Get only world localities
    UNUSED_VALUE(m_localitiesCaches.m_countries.Get(*context));
    UNUSED_VALUE(m_localitiesCaches.m_states.Get(*context));
    UNUSED_VALUE(m_localitiesCaches.m_citiesTownsOrVillages.Get(*context));
  }
}

void Geocoder::FillLocalitiesTable(BaseContext const & ctx)
{
  auto addRegionMaps = [&](FeatureType & ft, Locality const & l, Region::Type type) {
    if (ft.GetGeomType() != feature::GeomType::Point)
      return;

    string affiliation;
    if (!GetAffiliationName(ft, affiliation))
      return;

    Region region(l, type);
    region.m_center = ft.GetCenter();

    ft.GetName(StringUtf8Multilang::kDefaultCode, region.m_defaultName);
    LOG(LDEBUG, ("Region =", region.m_defaultName));

    m_infoGetter.GetMatchedRegions(affiliation, region.m_ids);
    m_regions[type][l.m_tokenRange].push_back(region);
  };

  vector<Locality> preLocalities;

  CBV filter = m_localitiesCaches.m_countries.Get(*m_context);
  FillLocalityCandidates(ctx, filter, kMaxNumCountries, preLocalities);
  for (auto & l : preLocalities)
  {
    auto ft = m_context->GetFeature(l.m_featureId);
    if (!ft)
    {
      LOG(LWARNING, ("Failed to get country from world", l.m_featureId));
      continue;
    }

    addRegionMaps(*ft, l, Region::TYPE_COUNTRY);
  }

  filter = m_localitiesCaches.m_states.Get(*m_context);
  FillLocalityCandidates(ctx, filter, kMaxNumStates, preLocalities);
  for (auto & l : preLocalities)
  {
    auto ft = m_context->GetFeature(l.m_featureId);
    if (!ft)
    {
      LOG(LWARNING, ("Failed to get state from world", l.m_featureId));
      continue;
    }

    addRegionMaps(*ft, l, Region::TYPE_STATE);
  }

  filter = m_localitiesCaches.m_citiesTownsOrVillages.Get(*m_context);
  FillLocalityCandidates(ctx, filter, kMaxNumCities, preLocalities);
  for (auto & l : preLocalities)
  {
    auto ft = m_context->GetFeature(l.m_featureId);
    if (!ft)
    {
      LOG(LWARNING, ("Failed to get city from world", l.m_featureId));
      continue;
    }

    if (ft->GetGeomType() == feature::GeomType::Point)
    {
      City city(l, Model::TYPE_CITY);

      CitiesBoundariesTable::Boundaries boundaries;
      bool haveBoundary = false;
      if (m_citiesBoundaries.Get(ft->GetID(), boundaries))
      {
        city.m_rect = boundaries.GetLimitRect();
        if (city.m_rect.IsValid())
          haveBoundary = true;
      }

      if (!haveBoundary)
      {
        auto const center = feature::GetCenter(*ft);
        auto const population = ftypes::GetPopulation(*ft);
        auto const radius = ftypes::GetRadiusByPopulation(population);
        city.m_rect = mercator::RectByCenterXYAndSizeInMeters(center, radius);
      }

#if defined(DEBUG)
      ft->GetName(StringUtf8Multilang::kDefaultCode, city.m_defaultName);
      LOG(LINFO,
          ("City =", city.m_defaultName, "rect =", city.m_rect,
           "rect source:", haveBoundary ? "table" : "population",
           "sizeX =", mercator::DistanceOnEarth(city.m_rect.LeftTop(), city.m_rect.RightTop()),
           "sizeY =", mercator::DistanceOnEarth(city.m_rect.LeftTop(), city.m_rect.LeftBottom())));
#endif

      m_cities[city.m_tokenRange].push_back(city);
    }
  }
}

void Geocoder::FillVillageLocalities(BaseContext const & ctx)
{
  vector<Locality> preLocalities;
  FillLocalityCandidates(ctx, ctx.m_villages /* filter */, kMaxNumVillages, preLocalities);

  size_t numVillages = 0;

  for (auto & l : preLocalities)
  {
    auto ft = m_context->GetFeature(l.m_featureId);
    if (!ft)
      continue;

    if (m_model.GetType(*ft) != Model::TYPE_VILLAGE)
      continue;

    m2::PointD center;
    // m_context->GetCenter is faster but may not work for editor created features.
    if (!m_context->GetCenter(l.m_featureId, center))
      center = feature::GetCenter(*ft);

    vector<m2::PointD> pivotPoints = {m_params.m_pivot.Center()};
    if (m_params.m_position)
      pivotPoints.push_back(*m_params.m_position);
    auto const mwmType = m_context->GetType();
    CHECK(mwmType, ());
    if (!mwmType->m_containsMatchedState &&
        all_of(pivotPoints.begin(), pivotPoints.end(), [&](auto const & p) {
          return mercator::DistanceOnEarth(center, p) > m_params.m_villageSearchRadiusM;
        }))
    {
      continue;
    }

    ++numVillages;
    City village(l, Model::TYPE_VILLAGE);

    auto const population = ftypes::GetPopulation(*ft);
    auto const radius = ftypes::GetRadiusByPopulation(population);
    village.m_rect = mercator::RectByCenterXYAndSizeInMeters(center, radius);

#if defined(DEBUG)
    ft->GetName(StringUtf8Multilang::kDefaultCode, village.m_defaultName);
    LOG(LDEBUG, ("Village =", village.m_defaultName, "radius =", radius));
#endif

    m_cities[village.m_tokenRange].push_back(village);
    if (numVillages >= kMaxNumVillages)
      break;
  }
}

bool Geocoder::CityHasPostcode(BaseContext const & ctx) const
{
  if (!ctx.m_city)
    return false;

  auto const isWorld = ctx.m_city->m_countryId.GetInfo()->GetType() == MwmInfo::WORLD;
  return m_postcodes.Has(ctx.m_city->m_featureId, isWorld);
}

template <typename Fn>
void Geocoder::ForEachCountry(ExtendedMwmInfos const & extendedInfos, Fn && fn)
{
  for (size_t i = 0; i < extendedInfos.m_infos.size(); ++i)
  {
    auto const & info = extendedInfos.m_infos[i].m_info;
    if (info->GetType() != MwmInfo::COUNTRY && info->GetType() != MwmInfo::WORLD)
      continue;
    if (info->GetType() == MwmInfo::COUNTRY && m_params.m_mode == Mode::Downloader)
      continue;

    auto handle = m_dataSource.GetMwmHandleById(MwmSet::MwmId(info));
    if (!handle.IsAlive())
      continue;
    auto & value = *handle.GetValue();
    if (!value.HasSearchIndex() || !value.HasGeometryIndex())
      continue;
    bool const updatePreranker = i + 1 >= extendedInfos.m_firstBatchSize;
    auto const & mwmType = extendedInfos.m_infos[i].m_type;
    if (fn(make_unique<MwmContext>(move(handle), mwmType), updatePreranker) ==
        base::ControlFlow::Break)
    {
      break;
    }
  }
}

void Geocoder::MatchCategories(BaseContext & ctx, bool aroundPivot)
{
  TRACE(MatchCategories);

  auto features = ctx.m_features[0];

  if (aroundPivot)
  {
    auto const pivotFeatures =
        RetrieveGeometryFeatures(*m_context, m_params.m_pivot, RectId::Pivot);
    ViewportFilter filter(pivotFeatures, m_preRanker.Limit() /* threshold */);
    features.m_features = filter.Filter(features.m_features);
    features.m_exactMatchingFeatures =
        features.m_exactMatchingFeatures.Intersect(features.m_features);
  }

  auto emit = [&](uint32_t featureId, bool exactMatch) {
    Model::Type type;
    if (!GetTypeInGeocoding(ctx, featureId, type))
      return;

    EmitResult(ctx, m_context->GetId(), featureId, type, TokenRange(0, ctx.m_numTokens),
               nullptr /* geoParts */, true /* allTokensUsed */, exactMatch);
  };

  // Features have been retrieved from the search index
  // using the exact (non-fuzzy) matching and intersected
  // with viewport, if needed. Every such feature is relevant.
  features.ForEach(emit);
}

void Geocoder::MatchRegions(BaseContext & ctx, Region::Type type)
{
  TRACE(MatchRegions);

  switch (type)
  {
  case Region::TYPE_STATE:
    // Tries to skip state matching and go to cities matching.
    // Then, performs states matching.
    MatchCities(ctx);
    break;
  case Region::TYPE_COUNTRY:
    // Tries to skip country matching and go to states matching.
    // Then, performs countries matching.
    MatchRegions(ctx, Region::TYPE_STATE);
    break;
  case Region::TYPE_COUNT: ASSERT(false, ("Invalid region type.")); return;
  }

  auto const & regions = m_regions[type];

  auto const & fileName = m_context->GetName();
  bool const isWorld = m_context->GetInfo()->GetType() == MwmInfo::WORLD;

  // Try to match regions.
  for (auto const & p : regions)
  {
    BailIfCancelled();

    auto const & tokenRange = p.first;
    if (ctx.HasUsedTokensInRange(tokenRange))
      continue;

    for (auto const & region : p.second)
    {
      bool matches = false;

      // On the World.mwm we need to check that CITY - STATE - COUNTRY
      // form a nested sequence.  Otherwise, as mwm borders do not
      // intersect state or country boundaries, it's enough to check
      // mwm that is currently being processed belongs to region.
      if (isWorld)
      {
        matches = ctx.m_regions.empty() ||
                  m_infoGetter.BelongsToAnyRegion(region.m_center, ctx.m_regions.back()->m_ids);
      }
      else
      {
        matches = m_infoGetter.BelongsToAnyRegion(fileName, region.m_ids);
      }

      if (!matches)
        continue;

      ctx.m_regions.push_back(&region);
      SCOPE_GUARD(cleanup, [&ctx]() { ctx.m_regions.pop_back(); });

      ScopedMarkTokens mark(ctx.m_tokens, BaseContext::FromRegionType(type), tokenRange);

      if (ctx.AllTokensUsed())
      {
        bool exactMatch = true;
        for (auto const & region : ctx.m_regions)
        {
          if (!region->m_exactMatch)
            exactMatch = false;
        }

        // Region matches to search query, we need to emit it as is.
        EmitResult(ctx, region, tokenRange, true /* allTokensUsed */, exactMatch);
        continue;
      }

      switch (type)
      {
      case Region::TYPE_STATE: MatchCities(ctx); break;
      case Region::TYPE_COUNTRY: MatchRegions(ctx, Region::TYPE_STATE); break;
      case Region::TYPE_COUNT: ASSERT(false, ("Invalid region type.")); break;
      }
    }
  }
}

void Geocoder::MatchCities(BaseContext & ctx)
{
  TRACE(MatchCities);

  ASSERT(!ctx.m_city, ());

  // Localities are ordered my (m_startToken, m_endToken) pairs.
  for (auto const & p : m_cities)
  {
    auto const & tokenRange = p.first;
    if (ctx.HasUsedTokensInRange(tokenRange))
      continue;

    for (auto const & city : p.second)
    {
      BailIfCancelled();

      if (!ctx.m_regions.empty() &&
          !m_infoGetter.BelongsToAnyRegion(city.m_rect.Center(), ctx.m_regions.back()->m_ids))
      {
        continue;
      }

      ScopedMarkTokens mark(ctx.m_tokens, BaseContext::FromModelType(city.m_type), tokenRange);
      ctx.m_city = &city;
      SCOPE_GUARD(cleanup, [&ctx]() { ctx.m_city = nullptr; });

      if (ctx.AllTokensUsed())
      {
        // City matches to search query, we need to emit it as is.
        EmitResult(ctx, city, tokenRange, true /* allTokensUsed */);
        continue;
      }

      // No need to search features in the World map.
      if (m_context->GetInfo()->GetType() == MwmInfo::WORLD)
        continue;

      auto cityFeatures = RetrieveGeometryFeatures(*m_context, city.m_rect, RectId::Locality);

      if (cityFeatures.IsEmpty())
        continue;

      LocalityFilter filter(cityFeatures);

      size_t const numEmitted = ctx.m_numEmitted;
      LimitedSearch(ctx, filter, {city.m_rect.Center()});
      if (numEmitted == ctx.m_numEmitted)
      {
        TRACE(Relaxed);
        EmitResult(ctx, *ctx.m_city, ctx.m_city->m_tokenRange, false /* allTokensUsed */);
      }
    }
  }
}

void Geocoder::MatchAroundPivot(BaseContext & ctx)
{
  TRACE(MatchAroundPivot);

  ViewportFilter filter(CBV::GetFull(), m_preRanker.Limit() /* threshold */);

  vector<m2::PointD> centers = {m_params.m_pivot.Center()};
  auto const mwmType = m_context->GetType();
  CHECK(mwmType, ());
  if (mwmType->m_containsUserPosition)
  {
    CHECK(m_params.m_position, ());
    if (mwmType->m_viewportIntersected)
      centers.push_back(*m_params.m_position);
    else
      centers = {*m_params.m_position};
  }

  LimitedSearch(ctx, filter, centers);
}

void Geocoder::LimitedSearch(BaseContext & ctx, FeaturesFilter const & filter,
                             vector<m2::PointD> const & centers)
{
  m_filter = &filter;
  SCOPE_GUARD(resetFilter, [&]() { m_filter = nullptr; });

  if (!ctx.m_streets)
    ctx.m_streets = m_streetsCache.Get(*m_context);

  if (!ctx.m_suburbs)
    ctx.m_suburbs = m_suburbsCache.Get(*m_context);

  MatchUnclassified(ctx, 0 /* curToken */);

  auto const search = [this, &ctx, &centers]() {
    GreedilyMatchStreets(ctx, centers);
    MatchPOIsAndBuildings(ctx, 0 /* curToken */, CBV::GetFull());
  };

  WithPostcodes(ctx, search);
  search();
}

template <typename Fn>
void Geocoder::WithPostcodes(BaseContext & ctx, Fn && fn)
{
  TRACE(WithPostcodes);

  size_t const maxPostcodeTokens = GetMaxNumTokensInPostcode();
  auto worldContext = GetWorldContext(m_dataSource);

  for (size_t startToken = 0; startToken != ctx.m_numTokens; ++startToken)
  {
    size_t endToken = startToken;
    for (size_t n = 1; startToken + n <= ctx.m_numTokens && n <= maxPostcodeTokens; ++n)
    {
      if (ctx.IsTokenUsed(startToken + n - 1))
        break;

      TokenSlice slice(m_params, TokenRange(startToken, startToken + n));
      auto const isPrefix = startToken + n == ctx.m_numTokens;
      if (LooksLikePostcode(QuerySlice(slice), isPrefix))
        endToken = startToken + n;
    }
    if (startToken == endToken)
      continue;

    TokenRange const tokenRange(startToken, endToken);

    auto postcodes = RetrievePostcodeFeatures(*m_context, TokenSlice(m_params, tokenRange));
    if (m_context->m_value.m_cont.IsExist(POSTCODE_POINTS_FILE_TAG))
    {
      auto & postcodePoints = m_postcodePointsCache.Get(*m_context);
      UniString postcodeQuery;
      JoinQueryTokens(m_params, tokenRange, kUniSpace /* sep */, postcodeQuery);
      vector<m2::PointD> points;
      postcodePoints.Get(postcodeQuery, points);
      for (auto const & p : points)
      {
        auto const rect = mercator::RectByCenterXYAndOffset(p, postcodePoints.GetRadius());
        postcodes = postcodes.Union(RetrieveGeometryFeatures(*m_context, rect, RectId::Postcode));
      }
    }
    SCOPE_GUARD(cleanup, [&]() { m_postcodes.Clear(); });

    if (!postcodes.IsEmpty())
    {
      ScopedMarkTokens mark(ctx.m_tokens, BaseContext::TOKEN_TYPE_POSTCODE, tokenRange);

      m_postcodes.Clear();

      if (worldContext)
      {
        m_postcodes.m_worldFeatures =
            RetrievePostcodeFeatures(*worldContext, TokenSlice(m_params, tokenRange));
      }

      m_postcodes.m_tokenRange = tokenRange;
      m_postcodes.m_countryFeatures = move(postcodes);

      if (ctx.AllTokensUsed() && CityHasPostcode(ctx))
      {
        EmitResult(ctx, *ctx.m_city, ctx.m_city->m_tokenRange, true /* allTokensUsed */);
        continue;
      }

      fn();
    }
  }
}

void Geocoder::GreedilyMatchStreets(BaseContext & ctx, vector<m2::PointD> const & centers)
{
  TRACE(GreedilyMatchStreets);

  // Match streets without suburbs.
  vector<StreetsMatcher::Prediction> predictions;
  StreetsMatcher::Go(ctx, ctx.m_streets, *m_filter, m_params, predictions);

  for (auto const & prediction : predictions)
    CreateStreetsLayerAndMatchLowerLayers(ctx, prediction, centers);

  GreedilyMatchStreetsWithSuburbs(ctx, centers);
}

void Geocoder::GreedilyMatchStreetsWithSuburbs(BaseContext & ctx,
                                               vector<m2::PointD> const & centers)
{
  TRACE(GreedilyMatchStreetsWithSuburbs);
  vector<StreetsMatcher::Prediction> suburbs;
  StreetsMatcher::Go(ctx, ctx.m_suburbs, *m_filter, m_params, suburbs);

  for (auto const & suburb : suburbs)
  {
    ScopedMarkTokens mark(ctx.m_tokens, BaseContext::TOKEN_TYPE_SUBURB, suburb.m_tokenRange);

    suburb.m_features.ForEach([&](uint64_t suburbId) {
      auto ft = m_context->GetFeature(static_cast<uint32_t>(suburbId));
      if (!ft)
        return;

      auto & layers = ctx.m_layers;
      ASSERT(layers.empty(), ());
      layers.emplace_back();
      SCOPE_GUARD(cleanupGuard, bind(&vector<FeaturesLayer>::pop_back, &layers));

      auto & layer = layers.back();
      InitLayer(Model::TYPE_SUBURB, suburb.m_tokenRange, layer);
      vector<uint32_t> suburbFeatures = {ft->GetID().m_index};
      layer.m_sortedFeatures = &suburbFeatures;

      auto const suburbType = ftypes::IsSuburbChecker::Instance().GetType(*ft);
      double radius = 0.0;
      switch (suburbType)
      {
      case ftypes::SuburbType::Residential: radius = kMaxResidentialRadiusM; break;
      case ftypes::SuburbType::Neighbourhood: radius = kMaxNeighbourhoodRadiusM; break;
      case ftypes::SuburbType::Suburb: radius = kMaxSuburbRadiusM; break;
      default: CHECK(false, ("Bad suburb type:", base::Underlying(suburbType)));
      }

      auto const rect = mercator::RectByCenterXYAndSizeInMeters(feature::GetCenter(*ft), radius);
      auto const suburbCBV = RetrieveGeometryFeatures(*m_context, rect, RectId::Suburb);
      auto const suburbStreets = ctx.m_streets.Intersect(suburbCBV);

      vector<StreetsMatcher::Prediction> predictions;
      StreetsMatcher::Go(ctx, suburbStreets, *m_filter, m_params, predictions);

      for (auto const & prediction : predictions)
        CreateStreetsLayerAndMatchLowerLayers(ctx, prediction, centers);

      MatchPOIsAndBuildings(ctx, 0 /* curToken */, suburbCBV);
    });
  }
}

void Geocoder::CreateStreetsLayerAndMatchLowerLayers(BaseContext & ctx,
                                                     StreetsMatcher::Prediction const & prediction,
                                                     vector<m2::PointD> const & centers)
{
  auto & layers = ctx.m_layers;

  layers.emplace_back();
  SCOPE_GUARD(cleanupGuard, bind(&vector<FeaturesLayer>::pop_back, &layers));

  auto & layer = layers.back();
  InitLayer(Model::TYPE_STREET, prediction.m_tokenRange, layer);

  vector<uint32_t> sortedFeatures;
  sortedFeatures.reserve(base::checked_cast<size_t>(prediction.m_features.PopCount()));
  prediction.m_features.ForEach([&](uint64_t bit) {
    auto const fid = base::asserted_cast<uint32_t>(bit);
    m2::PointD streetCenter;
    if (m_context->GetCenter(fid, streetCenter) &&
        any_of(centers.begin(), centers.end(), [&](auto const & center) {
          return mercator::DistanceOnEarth(center, streetCenter) <= m_params.m_streetSearchRadiusM;
        }))
    {
      sortedFeatures.push_back(base::asserted_cast<uint32_t>(bit));
    }
  });
  layer.m_sortedFeatures = &sortedFeatures;

  ScopedMarkTokens mark(ctx.m_tokens, BaseContext::TOKEN_TYPE_STREET, prediction.m_tokenRange);
  size_t const numEmitted = ctx.m_numEmitted;

  MatchPOIsAndBuildings(ctx, 0 /* curToken */, CBV::GetFull());

  // A relaxed best effort parse: at least show the street if we can find one.
  if (numEmitted == ctx.m_numEmitted && ctx.SkipUsedTokens(0) != ctx.m_numTokens)
  {
    TRACE(Relaxed);
    FindPaths(ctx);
  }
}

void Geocoder::MatchPOIsAndBuildings(BaseContext & ctx, size_t curToken, CBV const & filter)
{
  TRACE(MatchPOIsAndBuildings);

  BailIfCancelled();

  auto & layers = ctx.m_layers;

  curToken = ctx.SkipUsedTokens(curToken);
  if (curToken == ctx.m_numTokens)
  {
    // All tokens were consumed, find paths through layers, emit
    // features.
    if (m_postcodes.IsEmpty() || CityHasPostcode(ctx))
      return FindPaths(ctx);

    // When there are no layers but user entered a postcode, we have
    // to emit all features matching to the postcode.
    if (layers.size() == 0)
    {
      CBV filtered = m_postcodes.m_countryFeatures;
      if (m_filter->NeedToFilter(m_postcodes.m_countryFeatures))
        filtered = m_filter->Filter(m_postcodes.m_countryFeatures);
      filtered.ForEach([&](uint64_t bit) {
        auto const featureId = base::asserted_cast<uint32_t>(bit);
        Model::Type type;
        if (GetTypeInGeocoding(ctx, featureId, type))
        {
          EmitResult(ctx, m_context->GetId(), featureId, type, m_postcodes.m_tokenRange,
                     nullptr /* geoParts */, true /* allTokensUsed */, true /* exactMatch */);
        }
      });
      return;
    }

    if (!(layers.size() == 1 && layers[0].m_type == Model::TYPE_STREET))
      return FindPaths(ctx);

    // If there's only one street layer but user also entered a
    // postcode, we need to emit all features matching to postcode on
    // the given street, including the street itself.

    // Following code emits streets matched by postcodes, because
    // GreedilyMatchStreets() doesn't (and shouldn't) perform
    // postcodes matching.
    {
      for (auto const & id : *layers.back().m_sortedFeatures)
      {
        if (!m_postcodes.Has(id))
          continue;
        EmitResult(ctx, m_context->GetId(), id, Model::TYPE_STREET, layers.back().m_tokenRange,
                   nullptr /* geoParts */, true /* allTokensUsed */, true /* exactMatch */);
      }
    }

    // Following code creates a fake layer with buildings and
    // intersects it with the streets layer.
    layers.emplace_back();
    SCOPE_GUARD(cleanupGuard, bind(&vector<FeaturesLayer>::pop_back, &layers));

    auto & layer = layers.back();
    InitLayer(Model::TYPE_BUILDING, m_postcodes.m_tokenRange, layer);

    vector<uint32_t> features;
    m_postcodes.m_countryFeatures.ForEach([&features](uint64_t bit) {
      features.push_back(base::asserted_cast<uint32_t>(bit));
    });
    layer.m_sortedFeatures = &features;
    return FindPaths(ctx);
  }

  layers.emplace_back();
  SCOPE_GUARD(cleanupGuard, bind(&vector<FeaturesLayer>::pop_back, &layers));

  // Clusters of features by search type. Each cluster is a sorted
  // list of ids.
  size_t const kNumClusters = Model::TYPE_BUILDING + 1;
  vector<uint32_t> clusters[kNumClusters];

  // Appends |featureId| to the end of the corresponding cluster, if
  // any.
  auto const needPostcodes = !m_postcodes.IsEmpty() && !CityHasPostcode(ctx);
  auto clusterize = [&](uint64_t bit)
  {
    auto const featureId = base::asserted_cast<uint32_t>(bit);
    Model::Type type;
    if (!GetTypeInGeocoding(ctx, featureId, type))
      return;

    // All TYPE_CITY features were filtered in MatchCities().  All
    // TYPE_STREET features were filtered in GreedilyMatchStreets().
    if (type < kNumClusters)
    {
      if (!needPostcodes || m_postcodes.Has(featureId))
        clusters[type].push_back(featureId);
    }
  };

  Retrieval::ExtendedFeatures features(filter);

  // Try to consume [curToken, m_numTokens) tokens range.
  for (size_t n = 1; curToken + n <= ctx.m_numTokens && !ctx.IsTokenUsed(curToken + n - 1); ++n)
  {
    // At this point |features| is the intersection of
    // m_addressFeatures[curToken], m_addressFeatures[curToken + 1],
    // ..., m_addressFeatures[curToken + n - 2].

    BailIfCancelled();

    {
      auto & layer = layers.back();
      InitLayer(layer.m_type, TokenRange(curToken, curToken + n), layer);
    }

    features = features.Intersect(ctx.m_features[curToken + n - 1]);

    CBV filtered = features.m_features;
    if (m_filter->NeedToFilter(features.m_features))
      filtered = m_filter->Filter(features.m_features);

    bool const looksLikeHouseNumber = house_numbers::LooksLikeHouseNumber(
        layers.back().m_subQuery, layers.back().m_lastTokenIsPrefix);
    if (filtered.IsEmpty() && !looksLikeHouseNumber)
      break;

    if (n == 1)
    {
      filtered.ForEach(clusterize);
    }
    else
    {
      auto noFeature = [&filtered](uint64_t bit) -> bool
      {
        return !filtered.HasBit(bit);
      };
      for (auto & cluster : clusters)
        base::EraseIf(cluster, noFeature);

      size_t curs[kNumClusters] = {};
      size_t ends[kNumClusters];
      for (size_t i = 0; i < kNumClusters; ++i)
        ends[i] = clusters[i].size();
      filtered.ForEach([&](uint64_t bit)
                       {
                         auto const featureId = base::asserted_cast<uint32_t>(bit);
                         bool found = false;
                         for (size_t i = 0; i < kNumClusters && !found; ++i)
                         {
                           size_t & cur = curs[i];
                           size_t const end = ends[i];
                           while (cur != end && clusters[i][cur] < featureId)
                             ++cur;
                           if (cur != end && clusters[i][cur] == featureId)
                             found = true;
                         }
                         if (!found)
                           clusterize(featureId);
                       });
      for (size_t i = 0; i < kNumClusters; ++i)
        inplace_merge(clusters[i].begin(), clusters[i].begin() + ends[i], clusters[i].end());
    }

    for (size_t i = 0; i < kNumClusters; ++i)
    {
      // ATTENTION: DO NOT USE layer after recursive calls to
      // MatchPOIsAndBuildings().  This may lead to use-after-free.
      auto & layer = layers.back();
      layer.m_sortedFeatures = &clusters[i];

      if (i == Model::TYPE_BUILDING)
      {
        if (layer.m_sortedFeatures->empty() && !looksLikeHouseNumber)
          continue;
      }
      else if (layer.m_sortedFeatures->empty() ||
               house_numbers::LooksLikeHouseNumberStrict(layer.m_subQuery))
      {
        continue;
      }

      layer.m_type = static_cast<Model::Type>(i);
      ScopedMarkTokens mark(ctx.m_tokens, BaseContext::FromModelType(layer.m_type),
                            TokenRange(curToken, curToken + n));
      if (IsLayerSequenceSane(layers))
        MatchPOIsAndBuildings(ctx, curToken + n, filter);
    }
  }
}

bool Geocoder::IsLayerSequenceSane(vector<FeaturesLayer> const & layers) const
{
  ASSERT(!layers.empty(), ());
  static_assert(Model::TYPE_COUNT <= 32, "Select a wider type to represent search types mask.");
  uint32_t mask = 0;
  size_t buildingIndex = layers.size();
  size_t streetIndex = layers.size();
  size_t poiIndex = layers.size();

  // Following loop returns false iff there are two different layers
  // of the same search type.
  for (size_t i = 0; i < layers.size(); ++i)
  {
    auto const & layer = layers[i];
    ASSERT_NOT_EQUAL(layer.m_type, Model::TYPE_COUNT, ());

    // TODO (@y): probably it's worth to check belongs-to-locality here.
    uint32_t bit = 1U << layer.m_type;
    if (mask & bit)
      return false;
    mask |= bit;

    if (layer.m_type == Model::TYPE_BUILDING)
      buildingIndex = i;
    else if (layer.m_type == Model::TYPE_STREET)
      streetIndex = i;
    else if (Model::IsPoi(layer.m_type))
      poiIndex = i;
  }

  bool const hasBuildings = buildingIndex != layers.size();
  bool const hasStreets = streetIndex != layers.size();
  bool const hasPois = poiIndex != layers.size();

  // Checks that building and street layers are neighbours.
  if (hasBuildings && hasStreets)
  {
    auto const & buildings = layers[buildingIndex];
    auto const & streets = layers[streetIndex];
    if (!buildings.m_tokenRange.AdjacentTo(streets.m_tokenRange))
      return false;
  }
  if (hasBuildings && hasPois && !hasStreets)
  {
    return false;
  }

  return true;
}

void Geocoder::FindPaths(BaseContext & ctx)
{
  auto const & layers = ctx.m_layers;

  if (layers.empty())
    return;

  // Layers ordered by search type.
  vector<FeaturesLayer const *> sortedLayers;
  sortedLayers.reserve(layers.size());
  for (auto const & layer : layers)
    sortedLayers.push_back(&layer);
  sort(sortedLayers.begin(), sortedLayers.end(), base::LessBy(&FeaturesLayer::m_type));

  auto const & innermostLayer = *sortedLayers.front();

  if (m_postcodes.IsEmpty() || CityHasPostcode(ctx))
    m_matcher->SetPostcodes(nullptr);
  else
    m_matcher->SetPostcodes(&m_postcodes.m_countryFeatures);

  auto isExactMatch = [](BaseContext const & context, IntersectionResult const & result) {
    bool haveRegion = false;
    for (size_t i = 0; i < context.m_tokens.size(); ++i)
    {
      auto const tokenType = context.m_tokens[i];
      auto id = IntersectionResult::kInvalidId;

      if (tokenType == BaseContext::TokenType::TOKEN_TYPE_SUBPOI)
        id = result.m_subpoi;
      if (tokenType == BaseContext::TokenType::TOKEN_TYPE_COMPLEX_POI)
        id = result.m_complexPoi;
      if (tokenType == BaseContext::TokenType::TOKEN_TYPE_STREET)
        id = result.m_street;

      if (id != IntersectionResult::kInvalidId && context.m_features[i].m_features.HasBit(id) &&
          !context.m_features[i].m_exactMatchingFeatures.HasBit(id))
      {
        return false;
      }

      auto const isCityOrVillage = tokenType == BaseContext::TokenType::TOKEN_TYPE_CITY ||
                                   tokenType == BaseContext::TokenType::TOKEN_TYPE_VILLAGE;
      if (isCityOrVillage && context.m_city && !context.m_city->m_exactMatch)
        return false;

      auto const isRegion = tokenType == BaseContext::TokenType::TOKEN_TYPE_STATE ||
                            tokenType == BaseContext::TokenType::TOKEN_TYPE_COUNTRY;
      if (isRegion)
        haveRegion = true;
    }
    if (haveRegion)
    {
      for (auto const & region : context.m_regions)
      {
        if (!region->m_exactMatch)
          return false;
      }
    }

    return true;
  };

  m_finder.ForEachReachableVertex(*m_matcher, sortedLayers, [&](IntersectionResult const & result) {
    ASSERT(result.IsValid(), ());
    EmitResult(ctx, m_context->GetId(), result.InnermostResult(), innermostLayer.m_type,
               innermostLayer.m_tokenRange, &result, ctx.AllTokensUsed(),
               isExactMatch(ctx, result));
  });
}

void Geocoder::TraceResult(Tracer & tracer, BaseContext const & ctx, MwmSet::MwmId const & mwmId,
                           uint32_t ftId, Model::Type type, TokenRange const & tokenRange)
{
  SCOPE_GUARD(emitParse, [&]() { tracer.EmitParse(ctx.m_tokens); });

  if (!Model::IsPoi(type) && type != Model::TYPE_BUILDING)
    return;

  if (mwmId != m_context->GetId())
    return;

  auto ft = m_context->GetFeature(ftId);
  if (!ft)
    return;

  feature::TypesHolder holder(*ft);
  CategoriesInfo catInfo(holder, TokenSlice(m_params, tokenRange), m_params.m_categoryLocales,
                         m_categories);

  emitParse.release();
  tracer.EmitParse(ctx.m_tokens, catInfo.IsPureCategories());
}

void Geocoder::EmitResult(BaseContext & ctx, MwmSet::MwmId const & mwmId, uint32_t ftId,
                          Model::Type type, TokenRange const & tokenRange,
                          IntersectionResult const * geoParts, bool allTokensUsed, bool exactMatch)
{
  FeatureID id(mwmId, ftId);

  double matchedFraction = 1.0;
  // For categorial requests |allTokensUsed| == true and matchedFraction can not be calculated from |ctx|.
  if (!allTokensUsed)
  {
    size_t length = 0;
    size_t matchedLength = 0;
    TokenSlice slice(m_params, TokenRange(0, ctx.m_numTokens));
    for (size_t tokenIdx = 0; tokenIdx < ctx.m_numTokens; ++tokenIdx)
    {
      auto const tokenLength = slice.Get(tokenIdx).GetOriginal().size();
      length += tokenLength;
      if (ctx.IsTokenUsed(tokenIdx))
        matchedLength += tokenLength;
    }
    CHECK_NOT_EQUAL(length, 0, ());

    matchedFraction = static_cast<double>(matchedLength) / static_cast<double>(length);
  }

  // In our samples the least value for matched fraction for relevant result is 0.116.
  // It is "Горнолыжный комплекс Ключи" feature for "Спортивно стрелковый комплекс Ключи
  // Новосибирск" query. It is relevant not found result (search does not find it, but it's
  // relevant). The least matched fraction for found relevant result is 0.241935, for found vital
  // result is 0.269231.
  if (matchedFraction <= 0.1)
    return;

  if (ctx.m_cuisineFilter && !ctx.m_cuisineFilter->Matches(id))
    return;

  if (m_params.m_tracer)
    TraceResult(*m_params.m_tracer, ctx, mwmId, ftId, type, tokenRange);

  // Distance and rank will be filled at the end, for all results at once.
  //
  // TODO (@y, @m): need to skip zero rank features that are too
  // distant from the pivot when there are enough results close to the
  // pivot.
  PreRankingInfo info(type, tokenRange);

  if (ctx.m_hotelsFilter && !ctx.m_hotelsFilter->Matches(id))
    info.m_refusedByFilter = true;

  for (auto const & layer : ctx.m_layers)
    info.m_tokenRanges[layer.m_type] = layer.m_tokenRange;

  for (auto const * region : ctx.m_regions)
  {
    auto const regionType = Region::ToModelType(region->m_type);
    ASSERT_NOT_EQUAL(regionType, Model::TYPE_COUNT, ());
    info.m_tokenRanges[regionType] = region->m_tokenRange;
  }

  if (ctx.m_city)
  {
    auto const & city = *ctx.m_city;
    info.m_tokenRanges[city.m_type] = city.m_tokenRange;
    info.m_cityId = FeatureID(city.m_countryId, city.m_featureId);
  }

  if (geoParts)
    info.m_geoParts = *geoParts;

  info.m_allTokensUsed = allTokensUsed;
  info.m_exactMatch = exactMatch;

  m_preRanker.Emplace(id, info, m_resultTracer.GetProvenance());

  ++ctx.m_numEmitted;
}

void Geocoder::EmitResult(BaseContext & ctx, Region const & region, TokenRange const & tokenRange,
                          bool allTokensUsed, bool exactMatch)
{
  auto const type = Region::ToModelType(region.m_type);
  EmitResult(ctx, region.m_countryId, region.m_featureId, type, tokenRange, nullptr /* geoParts */,
             allTokensUsed, exactMatch);
}

void Geocoder::EmitResult(BaseContext & ctx, City const & city, TokenRange const & tokenRange,
                          bool allTokensUsed)
{
  EmitResult(ctx, city.m_countryId, city.m_featureId, city.m_type, tokenRange,
             nullptr /* geoParts */, allTokensUsed, city.m_exactMatch);
}

void Geocoder::MatchUnclassified(BaseContext & ctx, size_t curToken)
{
  TRACE(MatchUnclassified);

  ASSERT(ctx.m_layers.empty(), ());

  // We need to match all unused tokens to UNCLASSIFIED features,
  // therefore unused tokens must be adjacent to each other.  For
  // example, as parks are UNCLASSIFIED now, it's ok to match "London
  // Hyde Park", because London will be matched as a city and rest
  // adjacent tokens will be matched to "Hyde Park", whereas it's not
  // ok to match something to "Park London Hyde", because tokens
  // "Park" and "Hyde" are not adjacent.
  if (ctx.NumUnusedTokenGroups() != 1)
    return;

  Retrieval::ExtendedFeatures allFeatures;
  allFeatures.SetFull();

  curToken = ctx.SkipUsedTokens(curToken);
  auto startToken = curToken;
  for (; curToken < ctx.m_numTokens && !ctx.IsTokenUsed(curToken); ++curToken)
  {
    allFeatures = allFeatures.Intersect(ctx.m_features[curToken]);
  }

  if (m_filter->NeedToFilter(allFeatures.m_features))
  {
    allFeatures.m_features = m_filter->Filter(allFeatures.m_features);
    allFeatures.m_exactMatchingFeatures = m_filter->Filter(allFeatures.m_exactMatchingFeatures);
  }

  auto emitUnclassified = [&](uint32_t featureId, bool exactMatch) {
    Model::Type type;
    if (!GetTypeInGeocoding(ctx, featureId, type))
      return;
    if (type == Model::TYPE_UNCLASSIFIED)
    {
      auto const tokenRange = TokenRange(startToken, curToken);
      ScopedMarkTokens mark(ctx.m_tokens, BaseContext::TOKEN_TYPE_UNCLASSIFIED, tokenRange);
      EmitResult(ctx, m_context->GetId(), featureId, type, tokenRange,
                 nullptr /* geoParts */, true /* allTokensUsed */, exactMatch);
    }
  };
  allFeatures.ForEach(emitUnclassified);
}

CBV Geocoder::RetrievePostcodeFeatures(MwmContext const & context, TokenSlice const & slice)
{
  Retrieval retrieval(context, m_cancellable);
  return CBV(retrieval.RetrievePostcodeFeatures(slice));
}

CBV Geocoder::RetrieveGeometryFeatures(MwmContext const & context, m2::RectD const & rect,
                                       RectId id)
{
  switch (id)
  {
  case RectId::Pivot: return m_pivotRectsCache.Get(context, rect, m_params.GetScale());
  case RectId::Postcode: return m_postcodesRectsCache.Get(context, rect, m_params.GetScale());
  case RectId::Locality: return m_localityRectsCache.Get(context, rect, m_params.GetScale());
  case RectId::Suburb: return m_localityRectsCache.Get(context, rect, m_params.GetScale());
  case RectId::Count: ASSERT(false, ("Invalid RectId.")); return CBV();
  }
  UNREACHABLE();
}

bool Geocoder::GetTypeInGeocoding(BaseContext const & ctx, uint32_t featureId, Model::Type & type)
{
  if (ctx.m_streets.HasBit(featureId))
  {
    type = Model::TYPE_STREET;
    return true;
  }
  if (ctx.m_suburbs.HasBit(featureId))
  {
    type = Model::TYPE_SUBURB;
    return true;
  }
  if (ctx.m_villages.HasBit(featureId))
  {
    type = Model::TYPE_VILLAGE;
    return true;
  }

  auto feature = m_context->GetFeature(featureId);
  if (!feature)
    return false;

  type = m_model.GetType(*feature);
  return true;
}
}  // namespace search

#undef TRACE
