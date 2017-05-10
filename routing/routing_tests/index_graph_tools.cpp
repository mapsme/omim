#include "routing/routing_tests/index_graph_tools.hpp"

#include "testing/testing.hpp"

#include "routing_common/car_model.hpp"

#include "base/assert.hpp"

namespace routing_test
{
using namespace routing;

// TestGeometryLoader ------------------------------------------------------------------------------
void TestGeometryLoader::Load(uint32_t featureId, RoadGeometry & road) const
{
  auto it = m_roads.find(featureId);
  if (it == m_roads.cend())
    return;

  road = it->second;
}

void TestGeometryLoader::AddRoad(uint32_t featureId, bool oneWay, float speed,
                                 RoadGeometry::Points const & points)
{
  auto it = m_roads.find(featureId);
  CHECK(it == m_roads.end(), ("Already contains feature", featureId));
  m_roads[featureId] = RoadGeometry(oneWay, speed, points);
}

// ZeroGeometryLoader ------------------------------------------------------------------------------
void ZeroGeometryLoader::Load(uint32_t featureId, routing::RoadGeometry & road) const
{
  // Any valid road will do.
  auto const points = routing::RoadGeometry::Points({{0.0, 0.0}, {0.0, 1.0}});
  road = RoadGeometry(true /* oneWay */, 1.0 /* speed */, points);
}

// TestIndexGraphLoader ----------------------------------------------------------------------------
IndexGraph & TestIndexGraphLoader::GetIndexGraph(NumMwmId mwmId)
{
  auto it = m_graphs.find(mwmId);
  CHECK(it != m_graphs.end(), ("Not found mwm", mwmId));
  return *it->second;
}

void TestIndexGraphLoader::Clear() { m_graphs.clear(); }

void TestIndexGraphLoader::AddGraph(NumMwmId mwmId, std::unique_ptr<IndexGraph> graph)
{
  auto it = m_graphs.find(mwmId);
  CHECK(it == m_graphs.end(), ("Already contains mwm", mwmId));
  m_graphs[mwmId] = std::move(graph);
}

// WeightedEdgeEstimator --------------------------------------------------------------
double WeightedEdgeEstimator::CalcSegmentWeight(Segment const & segment,
                                                RoadGeometry const & /* road */) const
{
  auto const it = m_segmentWeights.find(segment);
  CHECK(it != m_segmentWeights.cend(), ());
  return it->second;
}

double WeightedEdgeEstimator::CalcHeuristic(m2::PointD const & /* from */,
                                            m2::PointD const & /* to */) const
{
  return 0.0;
}

double WeightedEdgeEstimator::CalcLeapWeight(m2::PointD const & /* from */,
                                             m2::PointD const & /* to */) const
{
  return 0.0;
}

double WeightedEdgeEstimator::GetUTurnPenalty() const { return 0.0; }

bool WeightedEdgeEstimator::LeapIsAllowed(NumMwmId /* mwmId */) const { return false; }

// TestIndexGraphTopology --------------------------------------------------------------------------
TestIndexGraphTopology::TestIndexGraphTopology(uint32_t numVertices) : m_numVertices(numVertices) {}

void TestIndexGraphTopology::AddDirectedEdge(Vertex from, Vertex to, double weight)
{
  AddDirectedEdge(m_edgeRequests, from, to, weight);
}

bool TestIndexGraphTopology::FindPath(Vertex start, Vertex finish, double & pathWeight,
                                      std::vector<Edge> & pathEdges) const
{
  CHECK_LESS(start, m_numVertices, ());
  CHECK_LESS(finish, m_numVertices, ());

  if (start == finish)
  {
    pathWeight = 0.0;
    pathEdges.clear();
    return true;
  }

  auto edgeRequests = m_edgeRequests;
  // Edges of the index graph are segments, so we need a loop at finish
  // for the end of our path and another loop at start for the bidirectional search.
  auto const startFeatureId = static_cast<uint32_t>(edgeRequests.size());
  AddDirectedEdge(edgeRequests, start, start, 0.0);
  auto const finishFeatureId = static_cast<uint32_t>(edgeRequests.size());
  AddDirectedEdge(edgeRequests, finish, finish, 0.0);

  Builder builder(m_numVertices);
  builder.BuildGraphFromRequests(edgeRequests);
  auto const worldGraph = builder.PrepareIndexGraph();
  CHECK(worldGraph != nullptr, ());

  auto const fakeStart = IndexGraphStarter::FakeVertex(kTestNumMwmId, startFeatureId,
                                                       0 /* pointId */, m2::PointD::Zero());
  auto const fakeFinish = IndexGraphStarter::FakeVertex(kTestNumMwmId, finishFeatureId,
                                                        0 /* pointId */, m2::PointD::Zero());

  IndexGraphStarter starter(fakeStart, fakeFinish, *worldGraph);

  std::vector<Segment> routeSegs;
  double timeSec;
  auto const resultCode = CalculateRoute(starter, routeSegs, timeSec);

  if (resultCode == AStarAlgorithm<IndexGraphStarter>::Result::NoPath)
    return false;
  CHECK_EQUAL(resultCode, AStarAlgorithm<IndexGraphStarter>::Result::OK, ());

  CHECK_GREATER_OR_EQUAL(routeSegs.size(), 2, ());
  CHECK_EQUAL(routeSegs.front(), starter.GetStart(), ());
  CHECK_EQUAL(routeSegs.back(), starter.GetFinish(), ());

  // We are not interested in the fake start and finish.
  pathEdges.resize(routeSegs.size() - 2);
  pathWeight = 0.0;
  for (size_t i = 1; i + 1 < routeSegs.size(); ++i)
  {
    auto const & seg = routeSegs[i];
    auto const it = builder.m_segmentToEdge.find(seg);
    CHECK(it != builder.m_segmentToEdge.cend(), ());
    auto const & edge = it->second;
    pathEdges[i - 1] = edge;
    pathWeight += builder.m_segmentWeights[seg];
  }

  // The loops from start to start and from finish to finish.
  CHECK_GREATER_OR_EQUAL(pathEdges.size(), 2, ());
  CHECK_EQUAL(pathEdges.front().first, pathEdges.front().second, ());
  CHECK_EQUAL(pathEdges.back().first, pathEdges.back().second, ());
  pathEdges.erase(pathEdges.begin());
  pathEdges.pop_back();

  return true;
}

void TestIndexGraphTopology::AddDirectedEdge(std::vector<EdgeRequest> & edgeRequests, Vertex from,
                                             Vertex to, double weight) const
{
  uint32_t const id = static_cast<uint32_t>(edgeRequests.size());
  edgeRequests.emplace_back(id, from, to, weight);
}

// TestIndexGraphTopology::Builder -----------------------------------------------------------------
unique_ptr<WorldGraph> TestIndexGraphTopology::Builder::PrepareIndexGraph()
{
  auto loader = my::make_unique<ZeroGeometryLoader>();
  auto estimator = std::make_shared<WeightedEdgeEstimator>(m_segmentWeights);

  BuildJoints();

  return BuildWorldGraph(std::move(loader), estimator, m_joints);
}

void TestIndexGraphTopology::Builder::BuildJoints()
{
  m_joints.resize(m_numVertices);
  for (uint32_t i = 0; i < m_joints.size(); ++i)
  {
    auto & joint = m_joints[i];

    for (auto const & segment : m_outgoingSegments[i])
      joint.AddPoint(RoadPoint(segment.GetFeatureId(), segment.GetPointId(false /* front */)));

    for (auto const & segment : m_ingoingSegments[i])
      joint.AddPoint(RoadPoint(segment.GetFeatureId(), segment.GetPointId(true /* front */)));
  }
}

void TestIndexGraphTopology::Builder::BuildGraphFromRequests(std::vector<EdgeRequest> const & requests)
{
  for (auto const & request : requests)
    BuildSegmentFromEdge(request);
}

void TestIndexGraphTopology::Builder::BuildSegmentFromEdge(EdgeRequest const & request)
{
  auto const edge = std::make_pair(request.m_from, request.m_to);
  auto p = m_edgeWeights.emplace(edge, request.m_weight);
  CHECK(p.second, ("Multi-edges are not allowed"));

  uint32_t const featureId = request.m_id;
  Segment const segment(kTestNumMwmId, featureId, 0 /* segmentIdx */, true /* std::forward */);

  m_segmentWeights[segment] = request.m_weight;
  m_segmentToEdge[segment] = edge;
  m_outgoingSegments[request.m_from].push_back(segment);
  m_ingoingSegments[request.m_to].push_back(segment);
}

// Functions ---------------------------------------------------------------------------------------
unique_ptr<WorldGraph> BuildWorldGraph(std::unique_ptr<TestGeometryLoader> geometryLoader,
                                       std::shared_ptr<EdgeEstimator> estimator,
                                       std::vector<Joint> const & joints)
{
  auto graph = my::make_unique<IndexGraph>(std::move(geometryLoader), estimator);
  graph->Import(joints);
  auto indexLoader = my::make_unique<TestIndexGraphLoader>();
  indexLoader->AddGraph(kTestNumMwmId, std::move(graph));
  return my::make_unique<WorldGraph>(nullptr /* crossMwmGraph */, std::move(indexLoader), estimator);
}

unique_ptr<WorldGraph> BuildWorldGraph(std::unique_ptr<ZeroGeometryLoader> geometryLoader,
                                       std::shared_ptr<EdgeEstimator> estimator,
                                       std::vector<Joint> const & joints)
{
  auto graph = my::make_unique<IndexGraph>(std::move(geometryLoader), estimator);
  graph->Import(joints);
  auto indexLoader = my::make_unique<TestIndexGraphLoader>();
  indexLoader->AddGraph(kTestNumMwmId, std::move(graph));
  return my::make_unique<WorldGraph>(nullptr /* crossMwmGraph */, std::move(indexLoader), estimator);
}

Joint MakeJoint(std::vector<RoadPoint> const & points)
{
  Joint joint;
  for (auto const & point : points)
    joint.AddPoint(point);

  return joint;
}

shared_ptr<EdgeEstimator> CreateEstimatorForCar(traffic::TrafficCache const & trafficCache)
{
  auto numMwmIds = std::make_shared<NumMwmIds>();
  auto stash = std::make_shared<TrafficStash>(trafficCache, numMwmIds);
  return CreateEstimatorForCar(stash);
}

shared_ptr<EdgeEstimator> CreateEstimatorForCar(std::shared_ptr<TrafficStash> trafficStash)
{
  return EdgeEstimator::CreateForCar(trafficStash, 90.0 /* maxSpeedKMpH */);
}

AStarAlgorithm<IndexGraphStarter>::Result CalculateRoute(IndexGraphStarter & starter,
                                                         std::vector<Segment> & roadPoints,
                                                         double & timeSec)
{
  AStarAlgorithm<IndexGraphStarter> algorithm;
  RoutingResult<Segment> routingResult;

  auto const resultCode = algorithm.FindPathBidirectional(
      starter, starter.GetStart(), starter.GetFinish(), routingResult, {} /* cancellable */,
      {} /* onVisitedVertexCallback */);

  timeSec = routingResult.distance;
  roadPoints = routingResult.path;
  return resultCode;
}

void TestRouteGeometry(IndexGraphStarter & starter,
                       AStarAlgorithm<IndexGraphStarter>::Result expectedRouteResult,
                       std::vector<m2::PointD> const & expectedRouteGeom)
{
  std::vector<Segment> routeSegs;
  double timeSec = 0.0;
  auto const resultCode = CalculateRoute(starter, routeSegs, timeSec);

  if (AStarAlgorithm<IndexGraphStarter>::Result::NoPath == expectedRouteResult &&
      expectedRouteGeom.empty())
  {
    // The route goes through a restriction. So there's no choice for building route
    // except for going through restriction. So no path.
    return;
  }

  TEST_EQUAL(resultCode, expectedRouteResult, ());
  if (resultCode != AStarAlgorithm<IndexGraphStarter>::Result::OK)
    return;

  CHECK(!routeSegs.empty(), ());
  std::vector<m2::PointD> geom;

  auto const pushPoint = [&geom](m2::PointD const & point) {
    if (geom.empty() || geom.back() != point)
      geom.push_back(point);
  };

  for (size_t i = 0; i < routeSegs.size(); ++i)
  {
    m2::PointD const & pnt = starter.GetPoint(routeSegs[i], false /* front */);
    // Note. In case of A* router all internal points of route are duplicated.
    // So it's necessary to exclude the duplicates.
    pushPoint(pnt);
  }

  pushPoint(starter.GetPoint(routeSegs.back(), false /* front */));
  TEST_EQUAL(geom, expectedRouteGeom, ());
}

void TestRouteTime(IndexGraphStarter & starter,
                   AStarAlgorithm<IndexGraphStarter>::Result expectedRouteResult,
                   double expectedTime)
{
  std::vector<Segment> routeSegs;
  double timeSec = 0.0;
  auto const resultCode = CalculateRoute(starter, routeSegs, timeSec);

  TEST_EQUAL(resultCode, expectedRouteResult, ());
  double const kEpsilon = 1e-5;
  TEST(my::AlmostEqualAbs(timeSec, expectedTime, kEpsilon), ());
}

void TestRestrictions(std::vector<m2::PointD> const & expectedRouteGeom,
                      AStarAlgorithm<IndexGraphStarter>::Result expectedRouteResult,
                      routing::IndexGraphStarter::FakeVertex const & start,
                      routing::IndexGraphStarter::FakeVertex const & finish,
                      RestrictionVec && restrictions, RestrictionTest & restrictionTest)
{
  restrictionTest.SetRestrictions(std::move(restrictions));
  restrictionTest.SetStarter(start, finish);
  TestRouteGeometry(*restrictionTest.m_starter, expectedRouteResult, expectedRouteGeom);
}
}  // namespace routing_test
