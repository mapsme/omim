#include "routing/routing_integration_tests/routing_test_tools.hpp"

#include "testing/testing.hpp"

#include "map/feature_vec_model.hpp"

#include "geometry/distance_on_sphere.hpp"
#include "geometry/latlon.hpp"

#include "routing/online_absent_fetcher.hpp"
#include "routing/online_cross_fetcher.hpp"
#include "routing/road_graph_router.hpp"
#include "routing/route.hpp"
#include "routing/router_delegate.hpp"

#include "indexer/index.hpp"

#include "platform/local_country_file.hpp"
#include "platform/local_country_file_utils.hpp"
#include "platform/platform.hpp"
#include "platform/preferred_languages.hpp"

#include "geometry/distance_on_sphere.hpp"

#include "std/limits.hpp"

#include "private.h"

#include <sys/resource.h>


using namespace routing;

namespace
{
  void ChangeMaxNumberOfOpenFiles(size_t n)
  {
    struct rlimit rlp;
    getrlimit(RLIMIT_NOFILE, &rlp);
    rlp.rlim_cur = n;
    setrlimit(RLIMIT_NOFILE, &rlp);
  }
}

namespace integration
{
  shared_ptr<model::FeaturesFetcher> CreateFeaturesFetcher(vector<LocalCountryFile> const & localFiles)
  {
    size_t const maxOpenFileNumber = 1024;
    ChangeMaxNumberOfOpenFiles(maxOpenFileNumber);
    shared_ptr<model::FeaturesFetcher> featuresFetcher(new model::FeaturesFetcher);
    featuresFetcher->InitClassificator();

    for (LocalCountryFile const & localFile : localFiles)
    {
      auto p = featuresFetcher->RegisterMap(localFile);
      if (p.second != MwmSet::RegResult::Success)
      {
        ASSERT(false, ("Can't register", localFile));
        return nullptr;
      }
    }
    return featuresFetcher;
  }

  unique_ptr<storage::CountryInfoGetter> CreateCountryInfoGetter()
  {
    Platform const & platform = GetPlatform();
    return make_unique<storage::CountryInfoReader>(platform.GetReader(PACKED_POLYGONS_FILE),
                                                   platform.GetReader(COUNTRIES_FILE));
  }

  shared_ptr<OsrmRouter> CreateOsrmRouter(Index & index,
                                          storage::CountryInfoGetter const & infoGetter)
  {
    shared_ptr<OsrmRouter> osrmRouter(new OsrmRouter(
        &index, [&infoGetter](m2::PointD const & pt)
        {
          return infoGetter.GetRegionFile(pt);
        }
        ));
    return osrmRouter;
  }

  shared_ptr<IRouter> CreatePedestrianRouter(Index & index,
                                             storage::CountryInfoGetter const & infoGetter)
  {
    auto countryFileGetter = [&infoGetter](m2::PointD const & pt)
    {
      return infoGetter.GetRegionFile(pt);
    };
    unique_ptr<IRouter> router = CreatePedestrianAStarBidirectionalRouter(index, countryFileGetter);
    return shared_ptr<IRouter>(move(router));
  }

  class OsrmRouterComponents : public IRouterComponents
  {
  public:
    OsrmRouterComponents(vector<LocalCountryFile> const & localFiles)
      : IRouterComponents(localFiles)
      , m_osrmRouter(CreateOsrmRouter(m_featuresFetcher->GetIndex(), *m_infoGetter))
    {
    }

    IRouter * GetRouter() const override { return m_osrmRouter.get(); }

  private:
    shared_ptr<OsrmRouter> m_osrmRouter;
  };

  class PedestrianRouterComponents : public IRouterComponents
  {
  public:
    PedestrianRouterComponents(vector<LocalCountryFile> const & localFiles)
      : IRouterComponents(localFiles)
      , m_router(CreatePedestrianRouter(m_featuresFetcher->GetIndex(), *m_infoGetter))
    {
    }

    IRouter * GetRouter() const override { return m_router.get(); }

  private:
    shared_ptr<IRouter> m_router;
  };

  template <typename TRouterComponents>
  shared_ptr<TRouterComponents> CreateAllMapsComponents()
  {
    // Setting stored paths from testingmain.cpp
    Platform & pl = GetPlatform();
    CommandLineOptions const & options = GetTestingOptions();
    if (options.m_dataPath)
      pl.SetWritableDirForTests(options.m_dataPath);
    if (options.m_resourcePath)
      pl.SetResourceDir(options.m_resourcePath);

    vector<LocalCountryFile> localFiles;
    platform::FindAllLocalMapsAndCleanup(numeric_limits<int64_t>::max() /* latestVersion */,
                                         localFiles);
    for (auto & file : localFiles)
      file.SyncWithDisk();
    ASSERT(!localFiles.empty(), ());
    return shared_ptr<TRouterComponents>(new TRouterComponents(localFiles));
  }

  shared_ptr<IRouterComponents> GetOsrmComponents(vector<platform::LocalCountryFile> const & localFiles)
  {
    return shared_ptr<IRouterComponents>(new OsrmRouterComponents(localFiles));
  }

  IRouterComponents & GetOsrmComponents()
  {
    static shared_ptr<IRouterComponents> const inst = CreateAllMapsComponents<OsrmRouterComponents>();
    ASSERT(inst, ());
    return *inst;
  }

  shared_ptr<IRouterComponents> GetPedestrianComponents(vector<platform::LocalCountryFile> const & localFiles)
  {
    return shared_ptr<IRouterComponents>(new PedestrianRouterComponents(localFiles));
  }

  IRouterComponents & GetPedestrianComponents()
  {
    static shared_ptr<IRouterComponents> const inst = CreateAllMapsComponents<PedestrianRouterComponents>();
    ASSERT(inst, ());
    return *inst;
  }

  TRouteResult CalculateRoute(IRouterComponents const & routerComponents,
                              m2::PointD const & startPoint, m2::PointD const & startDirection,
                              m2::PointD const & finalPoint)
  {
    RouterDelegate delegate;
    IRouter * router = routerComponents.GetRouter();
    ASSERT(router, ());
    shared_ptr<Route> route(new Route("mapsme"));
    IRouter::ResultCode result =
        router->CalculateRoute(startPoint, startDirection, finalPoint, delegate, *route.get());
    ASSERT(route, ());
    return TRouteResult(route, result);
  }

  void TestTurnCount(routing::Route const & route, uint32_t expectedTurnCount)
  {
    // We use -1 for ignoring the "ReachedYourDestination" turn record.
    TEST_EQUAL(route.GetTurns().size() - 1, expectedTurnCount, ());
  }

  void TestRouteLength(Route const & route, double expectedRouteMeters,
                       double relativeError)
  {
    double const delta = expectedRouteMeters * relativeError;
    double const routeMeters = route.GetTotalDistanceMeters();
    TEST(my::AlmostEqualAbs(routeMeters, expectedRouteMeters, delta),
        ("Route length test failed. Expected:", expectedRouteMeters, "have:", routeMeters, "delta:", delta));
  }

  void TestRouteTime(Route const & route, double expectedRouteSeconds, double relativeError)
  {
    double const delta = expectedRouteSeconds * relativeError;
    double const routeSeconds = route.GetTotalTimeSec();
    TEST(my::AlmostEqualAbs(routeSeconds, expectedRouteSeconds, delta),
        ("Route time test failed. Expected:", expectedRouteSeconds, "have:", routeSeconds, "delta:", delta));
  }

  void CalculateRouteAndTestRouteLength(IRouterComponents const & routerComponents,
                                        m2::PointD const & startPoint,
                                        m2::PointD const & startDirection,
                                        m2::PointD const & finalPoint, double expectedRouteMeters,
                                        double relativeError)
  {
    TRouteResult routeResult =
        CalculateRoute(routerComponents, startPoint, startDirection, finalPoint);
    IRouter::ResultCode const result = routeResult.second;
    TEST_EQUAL(result, IRouter::NoError, ());
    TestRouteLength(*routeResult.first, expectedRouteMeters, relativeError);
  }

  const TestTurn & TestTurn::TestValid() const
  {
    TEST(m_isValid, ());
    return *this;
  }

  const TestTurn & TestTurn::TestNotValid() const
  {
    TEST(!m_isValid, ());
    return *this;
  }

  const TestTurn & TestTurn::TestPoint(m2::PointD const & expectedPoint, double inaccuracyMeters) const
  {
    double const distanceMeters = ms::DistanceOnEarth(expectedPoint.y, expectedPoint.x, m_point.y, m_point.x);
    TEST_LESS(distanceMeters, inaccuracyMeters, ());
    return *this;
  }

  const TestTurn & TestTurn::TestDirection(routing::turns::TurnDirection expectedDirection) const
  {
    TEST_EQUAL(m_direction, expectedDirection, ());
    return *this;
  }

  const TestTurn & TestTurn::TestOneOfDirections(
      set<routing::turns::TurnDirection> const & expectedDirections) const
  {
    TEST(expectedDirections.find(m_direction) != expectedDirections.cend(), ());
    return *this;
  }

  const TestTurn & TestTurn::TestRoundAboutExitNum(uint32_t expectedRoundAboutExitNum) const
  {
    TEST_EQUAL(m_roundAboutExitNum, expectedRoundAboutExitNum, ());
    return *this;
  }

  TestTurn GetNthTurn(routing::Route const & route, uint32_t turnNumber)
  {
    Route::TTurns const & turns = route.GetTurns();
    if (turnNumber >= turns.size())
      return TestTurn();

    TurnItem const & turn = turns[turnNumber];
    return TestTurn(route.GetPoly().GetPoint(turn.m_index), turn.m_turn, turn.m_exitNum);
  }

  void TestOnlineFetcher(ms::LatLon const & startPoint, ms::LatLon const & finalPoint,
                         vector<string> const & expected, IRouterComponents & routerComponents)
  {
    auto countryFileGetter = [&routerComponents](m2::PointD const & p) -> string
    {
      return routerComponents.GetCountryInfoGetter().GetRegionFile(p);
    };
    auto localFileGetter =
        [&routerComponents](string const & countryFile) -> shared_ptr<LocalCountryFile>
    {
      // Always returns empty LocalCountryFile.
      return make_shared<LocalCountryFile>();
    };
    routing::OnlineAbsentCountriesFetcher fetcher(countryFileGetter, localFileGetter);
    fetcher.GenerateRequest(MercatorBounds::FromLatLon(startPoint),
                            MercatorBounds::FromLatLon(finalPoint));
    vector<string> absent;
    fetcher.GetAbsentCountries(absent);
    if (expected.size() < 2)
    {
      // Single MWM case. Do not use online routing.
      TEST(absent.empty(), ());
      return;
    }
    TEST_EQUAL(absent.size(), expected.size(), ());
    for (string const & name : expected)
      TEST(find(absent.begin(), absent.end(), name) != absent.end(), ("Can't find", name));
  }

  void TestOnlineCrosses(ms::LatLon const & startPoint, ms::LatLon const & finalPoint,
                         vector<string> const & expected,
                         IRouterComponents & routerComponents)
  {
    routing::OnlineCrossFetcher fetcher(OSRM_ONLINE_SERVER_URL, startPoint, finalPoint);
    fetcher.Do();
    vector<m2::PointD> const & points = fetcher.GetMwmPoints();
    TEST_EQUAL(points.size(), expected.size(), ());
    for (m2::PointD const & point : points)
    {
      string const mwmName = routerComponents.GetCountryInfoGetter().GetRegionFile(point);
      TEST(find(expected.begin(), expected.end(), mwmName) != expected.end(),
           ("Can't find ", mwmName));
    }
    TestOnlineFetcher(startPoint, finalPoint, expected, routerComponents);
  }
}
