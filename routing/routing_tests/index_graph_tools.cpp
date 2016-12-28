#include "routing/routing_tests/index_graph_tools.hpp"

#include "testing/testing.hpp"

#include "routing/car_model.hpp"

namespace routing_test
{
using namespace routing;

void TestGeometryLoader::Load(uint32_t featureId, RoadGeometry & road) const
{
  auto it = m_roads.find(featureId);
  if (it == m_roads.cend())
  {
    TEST(false, ("There's no road in |m_roads| for feature id:", featureId));
    return;
  }

  road = it->second;
}

void TestGeometryLoader::AddRoad(uint32_t featureId, bool oneWay, float speed,
                                 RoadGeometry::Points const & points)
{
  auto it = m_roads.find(featureId);
  CHECK(it == m_roads.end(), ("Already contains feature", featureId));
  m_roads[featureId] = RoadGeometry(oneWay, speed, points);
}

Joint MakeJoint(vector<RoadPoint> const & points)
{
  Joint joint;
  for (auto const & point : points)
    joint.AddPoint(point);

  return joint;
}

shared_ptr<EdgeEstimator> CreateEstimator(traffic::TrafficCache const & trafficCache)
{
  return EdgeEstimator::CreateForCar(*make_shared<CarModelFactory>()->GetVehicleModel(), trafficCache);
}

AStarAlgorithm<IndexGraphStarter>::Result CalculateRoute(IndexGraphStarter & starter,
                                                         vector<RoadPoint> & roadPoints,
                                                         double & timeSec)
{
  AStarAlgorithm<IndexGraphStarter> algorithm;
  RoutingResult<IndexGraphStarter::TVertexType> routingResult;

  auto const resultCode = algorithm.FindPath(starter, starter.GetStartVertex(),
                                             starter.GetFinishVertex(), routingResult, {}, {});
  timeSec = routingResult.distance;

  vector<Joint::Id> path;
  for (auto const & u : routingResult.path)
    path.emplace_back(u.GetCurr());
  if (resultCode == AStarAlgorithm<IndexGraphStarter>::Result::OK && path.size() >= 2)
  {
    CHECK_EQUAL(path[path.size() - 1], path[path.size() - 2], ());
    path.pop_back();
  }

  vector<RoutePoint> routePoints;
  starter.RedressRoute(path, routePoints);

  roadPoints.clear();
  roadPoints.reserve(routePoints.size());
  for (auto const & point : routePoints)
    roadPoints.push_back(point.GetRoadPoint());

  return resultCode;
}

void TestRouteSegments(IndexGraphStarter & starter,
                       AStarAlgorithm<IndexGraphStarter>::Result expectedRouteResult,
                       vector<RoadPoint> const & expectedRoute)
{
  vector<RoadPoint> route;
  double timeSec = 0.0;
  auto const resultCode = CalculateRoute(starter, route, timeSec);
  TEST_EQUAL(resultCode, expectedRouteResult, ());
  TEST_EQUAL(route, expectedRoute, ());
}

void TestRouteGeometry(IndexGraphStarter & starter,
                       AStarAlgorithm<IndexGraphStarter>::Result expectedRouteResult,
                       vector<m2::PointD> const & expectedRouteGeom)
{
  vector<RoadPoint> route;
  double timeSec = 0.0;
  auto const resultCode = CalculateRoute(starter, route, timeSec);

  TEST_EQUAL(resultCode, expectedRouteResult, ());
  vector<m2::PointD> geom;
  for (size_t i = 0; i < route.size(); ++i)
  {
    m2::PointD const & pnt = starter.GetPoint(route[i]);
    // Note. In case of A* router all internal points of route are duplicated.
    // So it's necessary to exclude the duplicates.
    if (geom.empty() || geom.back() != pnt)
      geom.push_back(pnt);
  }
  TEST_EQUAL(geom, expectedRouteGeom, ());
}

void TestRouteTime(IndexGraphStarter & starter,
                   AStarAlgorithm<IndexGraphStarter>::Result expectedRouteResult,
                   double expectedTime)
{
  vector<RoadPoint> route;
  double timeSec = 0.0;
  auto const resultCode = CalculateRoute(starter, route, timeSec);

  TEST_EQUAL(resultCode, expectedRouteResult, ());
  double const kEpsilon = 1e-6;
  TEST(my::AlmostEqualAbs(timeSec, expectedTime, kEpsilon), ());
}

void TestRestrictionPermutations(RestrictionVec restrictions,
                                 vector<m2::PointD> const & expectedRouteGeom,
                                 RoadPoint const & start, RoadPoint const & finish,
                                 RestrictionTest & restrictionTest)
{
  sort(restrictions.begin(), restrictions.end());
  do
  {
    for (Restriction const & restriction : restrictions)
    {
      CHECK_EQUAL(restriction.m_featureIds.size(), 2, ());
      RestrictionPoint restrictionPoint;
      CHECK(restrictionTest.GetRestrictionPoint(restriction.m_featureIds[0],
                                                restriction.m_featureIds[1], restrictionPoint),
            ());

      switch (restriction.m_type)
      {
      case Restriction::Type::No:
        restrictionTest.ApplyRestrictionNoRealFeatures(restrictionPoint);
        continue;
      case Restriction::Type::Only:
        restrictionTest.ApplyRestrictionOnlyRealFeatures(restrictionPoint);
        continue;
      }
    }
    restrictionTest.SetStarter(start, finish);
    TestRouteGeometry(*restrictionTest.m_starter, AStarAlgorithm<IndexGraphStarter>::Result::OK,
                      expectedRouteGeom);
  } while (next_permutation(restrictions.begin(), restrictions.end()));
}
}  // namespace routing_test
