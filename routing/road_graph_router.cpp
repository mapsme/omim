#include "routing/road_graph_router.hpp"

#include "routing/bicycle_directions.hpp"
#include "routing/features_road_graph.hpp"
#include "routing/nearest_edge_finder.hpp"
#include "routing/pedestrian_directions.hpp"
#include "routing/route.hpp"
#include "routing/routing_helpers.hpp"

#include "routing_common/bicycle_model.hpp"
#include "routing_common/car_model.hpp"
#include "routing_common/pedestrian_model.hpp"

#include "coding/reader_wrapper.hpp"

#include "indexer/feature.hpp"
#include "indexer/feature_altitude.hpp"
#include "indexer/ftypes_matcher.hpp"
#include "indexer/index.hpp"

#include "platform/country_file.hpp"
#include "platform/local_country_file.hpp"
#include "platform/mwm_version.hpp"

#include "geometry/distance.hpp"

#include <algorithm>
#include <queue>
#include <set>

#include "base/assert.hpp"

using platform::CountryFile;
using platform::LocalCountryFile;

namespace routing
{

namespace
{
size_t constexpr kMaxRoadCandidates = 6;

size_t constexpr kTestConnectivityVisitJunctionsLimit = 25;

uint64_t constexpr kMinPedestrianMwmVersion = 150713;

// Check if the found edges lays on mwm with pedestrian routing support.
bool CheckMwmVersion(std::vector<pair<Edge, Junction>> const & vicinities, std::vector<std::string> & mwmNames)
{
  mwmNames.clear();
  for (auto const & vicinity : vicinities)
  {
    auto const mwmInfo = vicinity.first.GetFeatureId().m_mwmId.GetInfo();
    // mwmInfo gets version from a path of the file, so we must read the version header to be sure.
    ModelReaderPtr reader = FilesContainerR(mwmInfo->GetLocalFile().GetPath(MapOptions::Map))
                                            .GetReader(VERSION_FILE_TAG);
    ReaderSrc src(reader.GetPtr());

    version::MwmVersion version;
    version::ReadVersion(src, version);
    if (version.GetVersion() < kMinPedestrianMwmVersion)
      mwmNames.push_back(mwmInfo->GetCountryName());
  }
  return !mwmNames.empty();
}

// Checks is edge connected with world graph.
// Function does BFS while it finds some number of edges, if graph ends
// before this number is reached then junction is assumed as not connected to the world graph.
bool CheckGraphConnectivity(IRoadGraph const & graph, Junction const & junction, size_t limit)
{
  std::queue<Junction> q;
  q.push(junction);

  std::set<Junction> visited;
  visited.insert(junction);

  std::vector<Edge> edges;
  while (!q.empty() && visited.size() < limit)
  {
    Junction const u = q.front();
    q.pop();

    edges.clear();
    graph.GetOutgoingEdges(u, edges);
    for (Edge const & edge : edges)
    {
      Junction const & v = edge.GetEndJunction();
      if (visited.count(v) == 0)
      {
        q.push(v);
        visited.insert(v);
      }
    }
  }

  return visited.size() >= limit;
}

// Find closest candidates in the world graph
void FindClosestEdges(IRoadGraph const & graph, m2::PointD const & point,
                      std::vector<pair<Edge, Junction>> & vicinity)
{
  // WARNING: Take only one vicinity, with, maybe, its inverse.  It is
  // an oversimplification that is not as easily solved as tuning up
  // this constant because if you set it too high you risk to find a
  // feature that you cannot in fact reach because of an obstacle.
  // Using only the closest feature minimizes (but not eliminates)
  // this risk.
  std::vector<pair<Edge, Junction>> candidates;
  graph.FindClosestEdges(point, kMaxRoadCandidates, candidates);

  vicinity.clear();
  for (auto const & candidate : candidates)
  {
    auto const & edge = candidate.first;
    if (CheckGraphConnectivity(graph, edge.GetEndJunction(), kTestConnectivityVisitJunctionsLimit))
    {
      vicinity.emplace_back(candidate);

      // Need to add a reverse edge, if exists, because fake edges
      // must be added for reverse edge too.
      IRoadGraph::TEdgeVector revEdges;
      graph.GetOutgoingEdges(edge.GetEndJunction(), revEdges);
      for (auto const & revEdge : revEdges)
      {
        if (revEdge.GetFeatureId() == edge.GetFeatureId() &&
            revEdge.GetEndJunction() == edge.GetStartJunction() &&
            revEdge.GetSegId() == edge.GetSegId())
        {
          vicinity.emplace_back(revEdge, candidate.second);
          break;
        }
      }

      break;
    }
  }
}
}  // namespace

RoadGraphRouter::~RoadGraphRouter() {}
RoadGraphRouter::RoadGraphRouter(std::string const & name, Index const & index,
                                 TCountryFileFn const & countryFileFn, IRoadGraph::Mode mode,
                                 std::unique_ptr<VehicleModelFactory> && vehicleModelFactory,
                                 std::unique_ptr<IRoutingAlgorithm> && algorithm,
                                 std::unique_ptr<IDirectionsEngine> && directionsEngine)
  : m_name(name)
  , m_countryFileFn(countryFileFn)
  , m_index(index)
  , m_algorithm(move(algorithm))
  , m_roadGraph(my::make_unique<FeaturesRoadGraph>(index, mode, move(vehicleModelFactory)))
  , m_directionsEngine(move(directionsEngine))
{
}

void RoadGraphRouter::ClearState()
{
  m_roadGraph->ClearState();
}

bool RoadGraphRouter::CheckMapExistence(m2::PointD const & point, Route & route) const
{
  std::string const fileName = m_countryFileFn(point);
  if (!m_index.GetMwmIdByCountryFile(CountryFile(fileName)).IsAlive())
  {
    route.AddAbsentCountry(fileName);
    return false;
  }
  return true;
}

IRouter::ResultCode RoadGraphRouter::CalculateRoute(m2::PointD const & startPoint,
                                                    m2::PointD const & /* startDirection */,
                                                    m2::PointD const & finalPoint,
                                                    RouterDelegate const & delegate, Route & route)
{
  if (!CheckMapExistence(startPoint, route) || !CheckMapExistence(finalPoint, route))
    return IRouter::RouteFileNotExist;

  std::vector<pair<Edge, Junction>> finalVicinity;
  FindClosestEdges(*m_roadGraph, finalPoint, finalVicinity);

  if (finalVicinity.empty())
    return IRouter::EndPointNotFound;

  // TODO (ldragunov) Remove this check after several releases. (Estimated in november)
  std::vector<std::string> mwmNames;
  if (CheckMwmVersion(finalVicinity, mwmNames))
  {
    for (auto const & name : mwmNames)
      route.AddAbsentCountry(name);
  }

  std::vector<pair<Edge, Junction>> startVicinity;
  FindClosestEdges(*m_roadGraph, startPoint, startVicinity);

  if (startVicinity.empty())
    return IRouter::StartPointNotFound;

  // TODO (ldragunov) Remove this check after several releases. (Estimated in november)
  if (CheckMwmVersion(startVicinity, mwmNames))
  {
    for (auto const & name : mwmNames)
      route.AddAbsentCountry(name);
  }

  if (!route.GetAbsentCountries().empty())
    return IRouter::FileTooOld;

  // Let us assume that the closest to startPoint/finalPoint feature point has the same altitude
  // with startPoint/finalPoint.
  Junction const startPos(startPoint, startVicinity.front().second.GetAltitude());
  Junction const finalPos(finalPoint, finalVicinity.front().second.GetAltitude());

  m_roadGraph->ResetFakes();
  m_roadGraph->AddFakeEdges(startPos, startVicinity);
  m_roadGraph->AddFakeEdges(finalPos, finalVicinity);

  RoutingResult<Junction> result;
  IRoutingAlgorithm::Result const resultCode =
      m_algorithm->CalculateRoute(*m_roadGraph, startPos, finalPos, delegate, result);

  if (resultCode == IRoutingAlgorithm::Result::OK)
  {
    ASSERT(!result.path.empty(), ());
    ASSERT_EQUAL(result.path.front(), startPos, ());
    ASSERT_EQUAL(result.path.back(), finalPos, ());
    ASSERT_GREATER(result.distance, 0., ());

    CHECK(m_directionsEngine, ());
    ReconstructRoute(*m_directionsEngine, *m_roadGraph, nullptr /* trafficColoring */,
                     delegate, result.path, route);
  }

  m_roadGraph->ResetFakes();

  if (delegate.IsCancelled())
    return IRouter::Cancelled;

  if (!route.IsValid())
    return IRouter::RouteNotFound;

  switch (resultCode)
  {
    case IRoutingAlgorithm::Result::OK: return IRouter::NoError;
    case IRoutingAlgorithm::Result::NoPath: return IRouter::RouteNotFound;
    case IRoutingAlgorithm::Result::Cancelled: return IRouter::Cancelled;
  }
  ASSERT(false, ("Unexpected IRoutingAlgorithm::Result code:", resultCode));
  return IRouter::RouteNotFound;
}

unique_ptr<IRouter> CreatePedestrianAStarRouter(Index & index, TCountryFileFn const & countryFileFn)
{
  std::unique_ptr<VehicleModelFactory> vehicleModelFactory(new PedestrianModelFactory());
  std::unique_ptr<IRoutingAlgorithm> algorithm(new AStarRoutingAlgorithm());
  std::unique_ptr<IDirectionsEngine> directionsEngine(new PedestrianDirectionsEngine());
  std::unique_ptr<IRouter> router(new RoadGraphRouter(
      "astar-pedestrian", index, countryFileFn, IRoadGraph::Mode::IgnoreOnewayTag,
      move(vehicleModelFactory), move(algorithm), move(directionsEngine)));
  return router;
}

unique_ptr<IRouter> CreatePedestrianAStarBidirectionalRouter(Index & index, TCountryFileFn const & countryFileFn)
{
  std::unique_ptr<VehicleModelFactory> vehicleModelFactory(new PedestrianModelFactory());
  std::unique_ptr<IRoutingAlgorithm> algorithm(new AStarBidirectionalRoutingAlgorithm());
  std::unique_ptr<IDirectionsEngine> directionsEngine(new PedestrianDirectionsEngine());
  std::unique_ptr<IRouter> router(new RoadGraphRouter(
      "astar-bidirectional-pedestrian", index, countryFileFn, IRoadGraph::Mode::IgnoreOnewayTag,
      move(vehicleModelFactory), move(algorithm), move(directionsEngine)));
  return router;
}

unique_ptr<IRouter> CreateBicycleAStarBidirectionalRouter(Index & index, TCountryFileFn const & countryFileFn)
{
  std::unique_ptr<VehicleModelFactory> vehicleModelFactory(new BicycleModelFactory());
  std::unique_ptr<IRoutingAlgorithm> algorithm(new AStarBidirectionalRoutingAlgorithm());
  std::unique_ptr<IDirectionsEngine> directionsEngine(new BicycleDirectionsEngine(index, nullptr));
  std::unique_ptr<IRouter> router(new RoadGraphRouter(
      "astar-bidirectional-bicycle", index, countryFileFn, IRoadGraph::Mode::ObeyOnewayTag,
      move(vehicleModelFactory), move(algorithm), move(directionsEngine)));
  return router;
}
}  // namespace routing
