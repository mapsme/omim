#include "search_engine.hpp"
#include "search_query.hpp"
#include "geometry_utils.hpp"

#include "storage/country_info_getter.hpp"

#include "indexer/categories_holder.hpp"
#include "indexer/search_string_utils.hpp"
#include "indexer/mercator.hpp"
#include "indexer/scales.hpp"
#include "indexer/classificator.hpp"

#include "platform/platform.hpp"

#include "geometry/distance_on_sphere.hpp"

#include "base/stl_add.hpp"

#include "std/map.hpp"
#include "std/vector.hpp"
#include "std/bind.hpp"

#include "3party/Alohalytics/src/alohalytics.h"


namespace search
{

double const DIST_EQUAL_QUERY = 100.0;

using TSuggestsContainer = vector<Query::TSuggest>;

class EngineData
{
public:
  EngineData(Reader * categoriesR) : m_categories(categoriesR) {}

  CategoriesHolder m_categories;
  TSuggestsContainer m_stringsToSuggest;
};

namespace
{

class InitSuggestions
{
  using TSuggestMap = map<pair<strings::UniString, int8_t>, uint8_t>;
  TSuggestMap m_suggests;

public:
  void operator() (CategoriesHolder::Category::Name const & name)
  {
    if (name.m_prefixLengthToSuggest != CategoriesHolder::Category::EMPTY_PREFIX_LENGTH)
    {
      strings::UniString const uniName = NormalizeAndSimplifyString(name.m_name);

      uint8_t & score = m_suggests[make_pair(uniName, name.m_locale)];
      if (score == 0 || score > name.m_prefixLengthToSuggest)
        score = name.m_prefixLengthToSuggest;
    }
  }

  void GetSuggests(TSuggestsContainer & cont) const
  {
    cont.reserve(m_suggests.size());
    for (TSuggestMap::const_iterator i = m_suggests.begin(); i != m_suggests.end(); ++i)
      cont.push_back(Query::TSuggest(i->first.first, i->second, i->first.second));
  }
};

}

Engine::Engine(Index & index, Reader * categoriesR, storage::CountryInfoGetter const & infoGetter,
               string const & locale, unique_ptr<SearchQueryFactory> && factory)
  : m_factory(move(factory))
  , m_data(make_unique<EngineData>(categoriesR))
{
  m_isReadyThread.clear();

  InitSuggestions doInit;
  m_data->m_categories.ForEachName(bind<void>(ref(doInit), _1));
  doInit.GetSuggests(m_data->m_stringsToSuggest);

  m_query = m_factory->BuildSearchQuery(index, &m_data->m_categories, &m_data->m_stringsToSuggest,
                                        infoGetter);
  m_query->SetPreferredLocale(locale);
}

Engine::~Engine()
{
}

void Engine::SupportOldFormat(bool b)
{
  m_query->SupportOldFormat(b);
}

void Engine::PrepareSearch(m2::RectD const & viewport)
{
  // bind does copy of all rects
  GetPlatform().RunAsync(bind(&Engine::SetViewportAsync, this, viewport));
}

bool Engine::Search(SearchParams const & params, m2::RectD const & viewport)
{
  // Check for equal query.
  // There is no need to synchronize here for reading m_params,
  // because this function is always called from main thread (one-by-one for queries).

  if (!params.IsForceSearch() &&
      m_params.IsEqualCommon(params) &&
      (m_viewport.IsValid() && IsEqualMercator(m_viewport, viewport, DIST_EQUAL_QUERY)))
  {
    if (m_params.IsSearchAroundPosition() &&
        ms::DistanceOnEarth(m_params.m_lat, m_params.m_lon, params.m_lat, params.m_lon) > DIST_EQUAL_QUERY)
    {
      // Go forward only if we search around position and it's changed significantly.
    }
    else
    {
      // Skip this query in all other cases.
      return false;
    }
  }

  {
    // Assign new search params.
    // Put the synch here, because this params are reading in search threads.
    threads::MutexGuard guard(m_updateMutex);
    m_params = params;
    m_viewport = viewport;
  }

  // Run task.
  GetPlatform().RunAsync(bind(&Engine::SearchAsync, this));

  return true;
}

void Engine::GetResults(Results & res)
{
  threads::MutexGuard guard(m_searchMutex);
  res = m_searchResults;
}

void Engine::SetViewportAsync(m2::RectD const & viewport)
{
  // First of all - cancel previous query.
  m_query->Cancel();

  // Enter to run new search.
  threads::MutexGuard searchGuard(m_searchMutex);

  m2::RectD r(viewport);
  (void)GetInflatedViewport(r);
  m_query->SetViewport(r, true);
}

void Engine::EmitResults(SearchParams const & params, Results & res)
{
  m_searchResults = res;

  // Basic test of our statistics engine.
  alohalytics::LogEvent("searchEmitResults",
                        alohalytics::TStringMap({{params.m_query, strings::to_string(res.GetCount())}}));

  params.m_callback(res);
}

void Engine::SetRankPivot(SearchParams const & params,
                          m2::RectD const & viewport, bool viewportSearch)
{
  if (!viewportSearch && params.IsValidPosition())
  {
    m2::PointD const pos = MercatorBounds::FromLatLon(params.m_lat, params.m_lon);
    if (m2::Inflate(viewport, viewport.SizeX() / 4.0, viewport.SizeY() / 4.0).IsPointInside(pos))
    {
      m_query->SetRankPivot(pos);
      return;
    }
  }

  m_query->SetRankPivot(viewport.Center());
}

void Engine::SearchAsync()
{
  if (m_isReadyThread.test_and_set())
    return;

  // First of all - cancel previous query.
  m_query->Cancel();

  // Enter to run new search.
  threads::MutexGuard searchGuard(m_searchMutex);

  m_isReadyThread.clear();

  // Get current search params.
  SearchParams params;
  m2::RectD viewport;

  {
    threads::MutexGuard updateGuard(m_updateMutex);
    params = m_params;

    if (!params.GetSearchRect(viewport))
      viewport = m_viewport;
  }

  bool const viewportSearch = params.HasSearchMode(SearchParams::IN_VIEWPORT_ONLY);

  // Initialize query.
  m_query->Init(viewportSearch);

  SetRankPivot(params, viewport, viewportSearch);

  m_query->SetSearchInWorld(params.HasSearchMode(SearchParams::SEARCH_WORLD));

  // Language validity is checked inside
  m_query->SetInputLocale(params.m_inputLocale);

  ASSERT(!params.m_query.empty(), ());
  m_query->SetQuery(params.m_query);

  Results res;

  // Call m_query->IsCancelled() everywhere it needed without storing
  // return value.  This flag can be changed from another thread.

  m_query->SearchCoordinates(params.m_query, res);

  try
  {
    // Do search for address in all modes.
    // params.HasSearchMode(SearchParams::SEARCH_ADDRESS)

    if (viewportSearch)
    {
      m_query->SetViewport(viewport, true /* forceUpdate */);
      m_query->SearchViewportPoints(res);
    }
    else
    {
      m_query->SetViewport(viewport, params.IsSearchAroundPosition() /* forceUpdate */);
      m_query->Search(res, RESULTS_COUNT);
    }

    if (res.GetCount() > 0)
      EmitResults(params, res);
  }
  catch (Query::CancelException const &)
  {
  }

  // Emit finish marker to client.
  params.m_callback(Results::GetEndMarker(m_query->IsCancelled()));
}

bool Engine::GetNameByType(uint32_t type, int8_t locale, string & name) const
{
  uint8_t level = ftype::GetLevel(type);
  ASSERT_GREATER(level, 0, ());

  while (true)
  {
    if (m_data->m_categories.GetNameByType(type, locale, name))
      return true;

    if (--level == 0)
      break;

    ftype::TruncValue(type, level);
  }

  return false;
}

void Engine::ClearViewportsCache()
{
  threads::MutexGuard guard(m_searchMutex);

  m_query->ClearCaches();
}

void Engine::ClearAllCaches()
{
  //threads::MutexGuard guard(m_searchMutex);

  // Trying to lock mutex, because this function calls on memory warning notification.
  // So that allows to prevent lock of UI while search query wouldn't be processed.
  if (m_searchMutex.TryLock())
  {
    m_query->ClearCaches();

    m_searchMutex.Unlock();
  }
}

}  // namespace search
