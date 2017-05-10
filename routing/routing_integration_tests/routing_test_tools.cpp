#include "routing/routing_integration_tests/routing_test_tools.hpp"

#include "routing/routing_tests/index_graph_tools.hpp"

#include "testing/testing.hpp"

#include "map/feature_vec_model.hpp"
#include "map/mwm_tree.hpp"

#include "geometry/distance_on_sphere.hpp"
#include "geometry/latlon.hpp"

#include "routing/index_router.hpp"
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

#include <limits>

#include "private.h"

#include <sys/resource.h>

using namespace routing;
using namespace routing_test;

using TRouterFactory =
    function<unique_ptr<IRouter>(Index & index, TCountryFileFn const & countryFileFn)>;

namespace
{
double constexpr kErrorMeters = 1.0;
double constexpr kErrorSeconds = 1.0;
void ChangeMaxNumberOfOpenFiles(size_t n)
{
  struct rlimit rlp;
  getrlimit(RLIMIT_NOFILE, &rlp);
  rlp.rlim_cur = n;
  setrlimit(RLIMIT_NOFILE, &rlp);
}
}  // namespace

namespace integration
{
  std::shared_ptr<model::FeaturesFetcher> CreateFeaturesFetcher(std::vector<LocalCountryFile> const & localFiles)
  {
    size_t const maxOpenFileNumber = 1024;
    ChangeMaxNumberOfOpenFiles(maxOpenFileNumber);
    std::shared_ptr<model::FeaturesFetcher> featuresFetcher(new model::FeaturesFetcher);
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
    return storage::CountryInfoReader::CreateCountryInfoReader(platform);
  }

  unique_ptr<CarRouter> CreateCarRouter(Index & index,
                                        storage::CountryInfoGetter const & infoGetter,
                                        traffic::TrafficCache const & trafficCache,
                                        std::vector<LocalCountryFile> const & localFiles)
  {
    auto const countryFileGetter = [&infoGetter](m2::PointD const & pt) {
      return infoGetter.GetRegionCountryId(pt);
    };

    auto const getMwmRectByName = [&infoGetter](std::string const & countryId) -> m2::RectD {
      return infoGetter.GetLimitRectForLeaf(countryId);
    };

    std::shared_ptr<NumMwmIds> numMwmIds = std::make_shared<NumMwmIds>();
    for (LocalCountryFile const & f : localFiles)
      numMwmIds->RegisterFile(f.GetCountryFile());

    auto carRouter = make_unique<CarRouter>(
        index, countryFileGetter,
        IndexRouter::CreateCarRouter(countryFileGetter, getMwmRectByName, numMwmIds,
                                     MakeNumMwmTree(*numMwmIds, infoGetter), trafficCache, index));
    return carRouter;
  }

  unique_ptr<IRouter> CreateAStarRouter(Index & index,
                                        storage::CountryInfoGetter const & infoGetter,
                                        TRouterFactory const & routerFactory)
  {
    // |infoGetter| should be a reference to an object which exists while the
    // result of the function is used.
    auto countryFileGetter = [&infoGetter](m2::PointD const & pt)
    {
      return infoGetter.GetRegionCountryId(pt);
    };
    unique_ptr<IRouter> router = routerFactory(index, countryFileGetter);
    return unique_ptr<IRouter>(std::move(router));
  }

  class OsrmRouterComponents : public IRouterComponents
  {
  public:
    OsrmRouterComponents(std::vector<LocalCountryFile> const & localFiles)
      : IRouterComponents(localFiles)
      , m_carRouter(CreateCarRouter(m_featuresFetcher->GetIndex(), *m_infoGetter, m_trafficCache,
                                    localFiles))
    {
    }

    IRouter * GetRouter() const override { return m_carRouter.get(); }

  private:
    traffic::TrafficCache m_trafficCache;
    unique_ptr<CarRouter> m_carRouter;
  };

  class PedestrianRouterComponents : public IRouterComponents
  {
  public:
    PedestrianRouterComponents(std::vector<LocalCountryFile> const & localFiles)
      : IRouterComponents(localFiles)
      , m_router(CreateAStarRouter(m_featuresFetcher->GetIndex(), *m_infoGetter,
                                   CreatePedestrianAStarBidirectionalRouter))
    {
    }

    IRouter * GetRouter() const override { return m_router.get(); }

  private:
    unique_ptr<IRouter> m_router;
  };

  class BicycleRouterComponents : public IRouterComponents
  {
  public:
    BicycleRouterComponents(std::vector<LocalCountryFile> const & localFiles)
      : IRouterComponents(localFiles)
      , m_router(CreateAStarRouter(m_featuresFetcher->GetIndex(), *m_infoGetter,
                                   CreateBicycleAStarBidirectionalRouter))
    {
    }

    IRouter * GetRouter() const override { return m_router.get(); }

  private:
    unique_ptr<IRouter> m_router;
  };

  template <typename TRouterComponents>
  std::shared_ptr<TRouterComponents> CreateAllMapsComponents()
  {
    // Setting stored paths from testingmain.cpp
    Platform & pl = GetPlatform();
    CommandLineOptions const & options = GetTestingOptions();
    if (options.m_dataPath)
      pl.SetWritableDirForTests(options.m_dataPath);
    if (options.m_resourcePath)
      pl.SetResourceDir(options.m_resourcePath);

    platform::migrate::SetMigrationFlag();

    std::vector<LocalCountryFile> localFiles;
    platform::FindAllLocalMapsAndCleanup(std::numeric_limits<int64_t>::max() /* latestVersion */,
                                         localFiles);
    for (auto & file : localFiles)
      file.SyncWithDisk();
    ASSERT(!localFiles.empty(), ());
    return std::shared_ptr<TRouterComponents>(new TRouterComponents(localFiles));
  }

  std::shared_ptr<IRouterComponents> GetOsrmComponents(std::vector<platform::LocalCountryFile> const & localFiles)
  {
    return std::shared_ptr<IRouterComponents>(new OsrmRouterComponents(localFiles));
  }

  IRouterComponents & GetOsrmComponents()
  {
    static auto const instance = CreateAllMapsComponents<OsrmRouterComponents>();
    ASSERT(instance, ());
    return *instance;
  }

  std::shared_ptr<IRouterComponents> GetPedestrianComponents(std::vector<platform::LocalCountryFile> const & localFiles)
  {
    return std::make_shared<PedestrianRouterComponents>(localFiles);
  }

  IRouterComponents & GetPedestrianComponents()
  {
    static auto const instance = CreateAllMapsComponents<PedestrianRouterComponents>();
    ASSERT(instance, ());
    return *instance;
  }

  std::shared_ptr<IRouterComponents> GetBicycleComponents(
      std::vector<platform::LocalCountryFile> const & localFiles)
  {
    return std::make_shared<BicycleRouterComponents>(localFiles);
  }

  IRouterComponents & GetBicycleComponents()
  {
    static auto const instance = CreateAllMapsComponents<BicycleRouterComponents>();
    ASSERT(instance, ());
    return *instance;
  }

  TRouteResult CalculateRoute(IRouterComponents const & routerComponents,
                              m2::PointD const & startPoint, m2::PointD const & startDirection,
                              m2::PointD const & finalPoint)
  {
    RouterDelegate delegate;
    IRouter * router = routerComponents.GetRouter();
    ASSERT(router, ());
    std::shared_ptr<Route> route(new Route("mapsme"));
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

  void TestCurrentStreetName(routing::Route const & route, std::string const & expectedStreetName)
  {
    std::string streetName;
    route.GetCurrentStreetName(streetName);
    TEST_EQUAL(streetName, expectedStreetName, ());
  }

  void TestNextStreetName(routing::Route const & route, std::string const & expectedStreetName)
  {
    std::string streetName;
    double distance;
    turns::TurnItem turn;
    TEST(route.GetCurrentTurn(distance, turn), ());
    route.GetStreetNameAfterIdx(turn.m_index, streetName);
    TEST_EQUAL(streetName, expectedStreetName, ());
  }

  void TestRouteLength(Route const & route, double expectedRouteMeters,
                       double relativeError)
  {
    double const delta = max(expectedRouteMeters * relativeError, kErrorMeters);
    double const routeMeters = route.GetTotalDistanceMeters();
    TEST(my::AlmostEqualAbs(routeMeters, expectedRouteMeters, delta),
        ("Route length test failed. Expected:", expectedRouteMeters, "have:", routeMeters, "delta:", delta));
  }

  void TestRouteTime(Route const & route, double expectedRouteSeconds, double relativeError)
  {
    double const delta = max(expectedRouteSeconds * relativeError, kErrorSeconds);
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
      std::set<routing::turns::TurnDirection> const & expectedDirections) const
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
                         std::vector<std::string> const & expected, IRouterComponents & routerComponents)
  {
    auto countryFileGetter = [&routerComponents](m2::PointD const & p) -> std::string
    {
      return routerComponents.GetCountryInfoGetter().GetRegionCountryId(p);
    };
    auto localFileChecker =
        [&routerComponents](std::string const & /* countryFile */) -> bool
    {
      // Always returns that the file is absent.
      return false;
    };
    routing::OnlineAbsentCountriesFetcher fetcher(countryFileGetter, localFileChecker);
    fetcher.GenerateRequest(MercatorBounds::FromLatLon(startPoint),
                            MercatorBounds::FromLatLon(finalPoint));
    std::vector<std::string> absent;
    fetcher.GetAbsentCountries(absent);
    if (expected.size() < 2)
    {
      // Single MWM case. Do not use online routing.
      TEST(absent.empty(), ());
      return;
    }
    TEST_EQUAL(absent.size(), expected.size(), ());
    for (std::string const & name : expected)
      TEST(find(absent.begin(), absent.end(), name) != absent.end(), ("Can't find", name));
  }

  void TestOnlineCrosses(ms::LatLon const & startPoint, ms::LatLon const & finalPoint,
                         std::vector<std::string> const & expected,
                         IRouterComponents & routerComponents)
  {
    routing::OnlineCrossFetcher fetcher(OSRM_ONLINE_SERVER_URL, startPoint, finalPoint);
    fetcher.Do();
    std::vector<m2::PointD> const & points = fetcher.GetMwmPoints();
    std::set<std::string> foundMwms;

    for (m2::PointD const & point : points)
    {
      std::string const mwmName = routerComponents.GetCountryInfoGetter().GetRegionCountryId(point);
      TEST(find(expected.begin(), expected.end(), mwmName) != expected.end(),
           ("Can't find ", mwmName));
      foundMwms.insert(mwmName);
    }
    TEST_EQUAL(expected.size(), foundMwms.size(), ());
  }
}
