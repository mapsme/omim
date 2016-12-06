#include "testing/testing.hpp"

#include "routing/routing_tests/index_graph_tools.hpp"

#include "routing/car_model.hpp"
#include "routing/geometry.hpp"

#include "geometry/point2d.hpp"

#include "std/unique_ptr.hpp"
#include "std/vector.hpp"

namespace routing_test
{
using namespace routing;

void TestRoutes(vector<RoadPoint> const & starts, vector<RoadPoint> const & finishes,
                vector<vector<RoadPoint>> const & expectedRoutes, IndexGraph & graph)
{
  CHECK_EQUAL(starts.size(), expectedRoutes.size(), ());
  CHECK_EQUAL(finishes.size(), expectedRoutes.size(), ());

  for (size_t i = 0; i < expectedRoutes.size(); ++i)
  {
    IndexGraphStarter starter(graph, starts[i], finishes[i]);
    TestRouteSegments(starter, AStarAlgorithm<IndexGraphStarter>::Result::OK, expectedRoutes[i]);
  }
}

void EdgeTest(Joint::Id vertex, size_t expectedIntgoingNum, size_t expectedOutgoingNum,
              bool graphWithoutRestrictions, IndexGraph & graph)
{
  vector<IndexGraphStarter::TEdgeType> ingoing;
  graph.GetEdgeList(vertex, false /* is outgoing */, graphWithoutRestrictions, ingoing);
  TEST_EQUAL(ingoing.size(), expectedIntgoingNum, ());

  vector<IndexGraphStarter::TEdgeType> outgoing;
  graph.GetEdgeList(vertex, true /* is outgoing */, graphWithoutRestrictions, outgoing);
  TEST_EQUAL(outgoing.size(), expectedOutgoingNum, ());
}

// Finish
// 2 *
//   ^ ↖
//   |   F1
//   |      ↖
// 1 |        *
//   F0         ↖
//   |            F2
//   |              ↖
// 0 *<--F3---<--F3---* Start
//   0        1       2
// Note. F0, F1 and F2 are one segment features. F3 is a two segments feature.
unique_ptr<IndexGraph> BuildTriangularGraph()
{
  unique_ptr<TestGeometryLoader> loader = make_unique<TestGeometryLoader>();
  loader->AddRoad(0 /* featureId */, true /* oneWay */, 1.0 /* speed */,
                  RoadGeometry::Points({{0.0, 0.0}, {0.0, 2.0}}));
  loader->AddRoad(1 /* featureId */, true /* oneWay */, 1.0 /* speed */,
                  RoadGeometry::Points({{1.0, 1.0}, {0.0, 2.0}}));
  loader->AddRoad(2 /* featureId */, true /* oneWay */, 1.0 /* speed */,
                  RoadGeometry::Points({{0.0, 2.0}, {1.0, 1.0}}));
  loader->AddRoad(3 /* featureId */, true /* oneWay */, 1.0 /* speed */,
                  RoadGeometry::Points({{2.0, 0.0}, {1.0, 0.0}, {0.0, 0.0}}));

  vector<Joint> const joints = {
      MakeJoint({{2, 0}, {3, 0}}), /* joint at point (2, 0) */
      MakeJoint({{3, 2}, {0, 0}}), /* joint at point (0, 0) */
      MakeJoint({{2, 1}, {1, 0}}), /* joint at point (1, 1) */
      MakeJoint({{0, 1}, {1, 1}})  /* joint at point (0, 2) */
  };

  traffic::TrafficCache const trafficCache;
  unique_ptr<IndexGraph> graph = make_unique<IndexGraph>(move(loader), CreateEstimator(trafficCache));
  graph->Import(joints);
  return graph;
}

// Route through triangular graph without any restrictions.
UNIT_TEST(TriangularGraph)
{
  unique_ptr<IndexGraph> graph = BuildTriangularGraph();
  IndexGraphStarter starter(*graph, RoadPoint(2, 0) /* start */, RoadPoint(1, 1) /* finish */);
  vector<RoadPoint> const expectedRoute = {{2 /* feature id */, 0 /* seg id */}, {2, 1}, {1, 1}};
  TestRouteSegments(starter, AStarAlgorithm<IndexGraphStarter>::Result::OK, expectedRoute);
}

// Route through triangular graph with feature 2 disabled.
UNIT_CLASS_TEST(RestrictionTest, TriangularGraph_DisableF2)
{
  Init(BuildTriangularGraph());
  SetStarter(RoadPoint(2, 0) /* start */, RoadPoint(1, 1) /* finish */);

  DisableEdge({2 /* feature id */, 0 /* point id */},{2, 1}, 2 /* feature id */);
  vector<RoadPoint> const expectedRouteOneEdgeRemoved = {
      {3 /* feature id */, 0 /* seg id */}, {3, 1}, {3, 2}, {0, 1}};
  TestRouteSegments(*m_starter, AStarAlgorithm<IndexGraphStarter>::Result::OK,
                    expectedRouteOneEdgeRemoved);
}

// Route through triangular graph with restriction type no from feature 2 to feature 1.
UNIT_CLASS_TEST(RestrictionTest, TriangularGraph_RestrictionNoF2F1)
{
  Init(BuildTriangularGraph());
  SetStarter(RoadPoint(2, 0) /* start */, RoadPoint(1, 1) /* finish */);

  ApplyRestrictionNoRealFeatures(RestrictionPoint({2 /* feature id */, 1 /* seg id */}, {1, 0},
                                 GetJointIdForTesting({1, 0})));
  vector<RoadPoint> const expectedRouteRestrictionF2F1No = {
      {3 /* feature id */, 0 /* seg id */}, {3, 1}, {3, 2}, {0, 1}};
  TestRouteSegments(*m_starter, AStarAlgorithm<IndexGraphStarter>::Result::OK,
                    expectedRouteRestrictionF2F1No);
}

// Finish
// 2 *
//   ^ ↖
//   |   ↖
//   |     ↖
// 1 |       Fake adding one link feature
//   F0        ↖
//   |           ↖
//   |             ↖
// 0 *<--F1---<--F1--* Start
//   0        1       2
// Note. F1 is a two setments feature. The others are one setment ones.
unique_ptr<IndexGraph> BuildCornerGraph()
{
  unique_ptr<TestGeometryLoader> loader = make_unique<TestGeometryLoader>();
  loader->AddRoad(0 /* feature id */, true /* oneWay */, 1.0 /* speed */,
                  RoadGeometry::Points({{0.0, 0.0}, {0.0, 2.0}}));
  loader->AddRoad(1 /* feature id */, true /* oneWay */, 1.0 /* speed */,
                  RoadGeometry::Points({{2.0, 0.0}, {1.0, 0.0}, {0.0, 0.0}}));

  vector<Joint> const joints = {MakeJoint({{1 /* feature id */, 2 /* point id */}, {0, 0}}),
                                /* joint at point (0, 0) */};

  traffic::TrafficCache const trafficCache;
  unique_ptr<IndexGraph> graph = make_unique<IndexGraph>(move(loader), CreateEstimator(trafficCache));
  graph->Import(joints);
  graph->InsertJoint({1 /* feature id */, 0 /* point id */});  // Start joint.
  graph->InsertJoint({0 /* feature id */, 1 /* point id */});  // Finish joint.
  return graph;
}

// Route through corner graph without any restrictions.
UNIT_TEST(CornerGraph)
{
  unique_ptr<IndexGraph> graph = BuildCornerGraph();

  // Route along F1 and F0.
  IndexGraphStarter starter(*graph, RoadPoint(1, 0) /* start */, RoadPoint(0, 1) /* finish */);
  vector<RoadPoint> const expectedRoute = {
      {1 /* feature id */, 0 /* point id */}, {1, 1}, {1, 2}, {0, 1}};
  TestRouteSegments(starter, AStarAlgorithm<IndexGraphStarter>::Result::OK, expectedRoute);
}

// Generate geometry based on feature 1 of corner graph.
UNIT_CLASS_TEST(RestrictionTest, CornerGraph_CreateFakeFeature1Geometry)
{
  Init(BuildCornerGraph());

  vector<vector<RoadPoint>> oneEdgeConnectionPaths;
  GetConnectionPaths({1 /* feature id */, 0 /* point id */}, {1, 2}, oneEdgeConnectionPaths);
  TEST_EQUAL(oneEdgeConnectionPaths.size(), 1, ());

  vector<RoadPoint> const expectedDirectOrder = {{1, 0}, {1, 1}, {1, 2}};
  TEST_EQUAL(oneEdgeConnectionPaths[0], expectedDirectOrder, ());
  RoadGeometry geometryDirect;
  CreateFakeFeatureGeometry(oneEdgeConnectionPaths[0], 1.0 /* speed */, geometryDirect);
  RoadGeometry expectedGeomentryDirect(
      true /* one way */, 1.0 /* speed */,
      RoadGeometry::Points({{2.0, 0.0}, {1.0, 0.0}, {0.0, 0.0}}));
  TEST_EQUAL(geometryDirect, expectedGeomentryDirect, ());
}

// Generate geometry based on reversed feature 1 of corner graph.
UNIT_CLASS_TEST(RestrictionTest, CornerGraph_CreateFakeReversedFeature1Geometry)
{
  Init(BuildCornerGraph());

  vector<vector<RoadPoint>> oneEdgeConnectionPaths;
  GetConnectionPaths({1 /* feature id */, 2 /* point id */}, {1, 0}, oneEdgeConnectionPaths);
  TEST_EQUAL(oneEdgeConnectionPaths.size(), 1, ());

  vector<RoadPoint> const expectedBackOrder = {{1, 2}, {1, 1}, {1, 0}};
  TEST_EQUAL(oneEdgeConnectionPaths[0], expectedBackOrder, ());
  RoadGeometry geometryBack;
  CreateFakeFeatureGeometry(oneEdgeConnectionPaths[0], 1.0 /* speed */, geometryBack);
  RoadGeometry expectedGeomentryBack(
      true /* one way */, 1.0 /* speed */,
      RoadGeometry::Points({{0.0, 0.0}, {1.0, 0.0}, {2.0, 0.0}}));
  TEST_EQUAL(geometryBack, expectedGeomentryBack, ());
}

// Route through corner graph with adding a fake edge.
UNIT_CLASS_TEST(RestrictionTest, CornerGraph_AddFakeFeature)
{
  RoadPoint const kStart(1, 0);
  RoadPoint const kFinish(0, 1);
  Init(BuildCornerGraph());
  SetStarter(kStart, kFinish);

  AddFakeFeature(kStart, kFinish, {kStart, kFinish} /* geometrySource */, 1.0 /* speed */);

  vector<RoadPoint> const expectedRouteByFakeFeature = {
      {IndexGraph::kStartFakeFeatureIds, 0 /* seg id */},
      {IndexGraph::kStartFakeFeatureIds, 1 /* seg id */}};
  TestRouteSegments(*m_starter, AStarAlgorithm<IndexGraphStarter>::Result::OK,
                    expectedRouteByFakeFeature);
}

// Finish 2   Finish 1  Finish 0
// 2 *<---F5----*<---F6---*
//   ^ ↖       ^ ↖       ^
//   | Fake-1   | Fake-0  |
//   |     ↖   F1    ↖   F2
//   |       ↖ |       ↖ |
// 1 F0         *          *
//   |          ^  ↖      ^
//   |         F1  Fake-2 F2
//   |          |       ↖ |
// 0 *<----F4---*<---F3----* Start
//   0          1          2
// Note. F1 and F2 are two segments features. The others are one segment ones.
unique_ptr<IndexGraph> BuildTwoSquaresGraph()
{
  unique_ptr<TestGeometryLoader> loader = make_unique<TestGeometryLoader>();
  loader->AddRoad(0 /* feature id */, true /* oneWay */, 1.0 /* speed */,
                  RoadGeometry::Points({{0.0, 0.0}, {0.0, 2.0}}));
  loader->AddRoad(1 /* feature id */, true /* oneWay */, 1.0 /* speed */,
                  RoadGeometry::Points({{1.0, 0.0}, {1.0, 1.0}, {1.0, 2.0}}));
  loader->AddRoad(2 /* feature id */, true /* oneWay */, 1.0 /* speed */,
                  RoadGeometry::Points({{2.0, 0.0}, {2.0, 1.0}, {2.0, 2.0}}));
  loader->AddRoad(3 /* feature id */, true /* oneWay */, 1.0 /* speed */,
                  RoadGeometry::Points({{2.0, 0.0}, {1.0, 0.0}}));
  loader->AddRoad(4 /* feature id */, true /* oneWay */, 1.0 /* speed */,
                  RoadGeometry::Points({{1.0, 0.0}, {0.0, 0.0}}));
  loader->AddRoad(5 /* feature id */, true /* oneWay */, 1.0 /* speed */,
                  RoadGeometry::Points({{1.0, 2.0}, {0.0, 2.0}}));
  loader->AddRoad(6 /* feature id */, true /* oneWay */, 1.0 /* speed */,
                  RoadGeometry::Points({{2.0, 2.0}, {1.0, 2.0}}));

  vector<Joint> const joints = {
      MakeJoint({{4 /* featureId */, 1 /* pointId */}, {0, 0}}), /* joint at point (0, 0) */
      MakeJoint({{0, 1}, {5, 1}}),                               /* joint at point (0, 2) */
      MakeJoint({{4, 0}, {1, 0}, {3, 1}}),                       /* joint at point (1, 0) */
      MakeJoint({{5, 0}, {1, 2}, {6, 1}}),                       /* joint at point (1, 2) */
      MakeJoint({{3, 0}, {2, 0}}),                               /* joint at point (2, 0) */
      MakeJoint({{2, 2}, {6, 0}}),                               /* joint at point (2, 2) */
  };

  traffic::TrafficCache const trafficCache;
  unique_ptr<IndexGraph> graph = make_unique<IndexGraph>(move(loader), CreateEstimator(trafficCache));
  graph->Import(joints);
  return graph;
}

// Route through two squares graph without any restrictions.
UNIT_TEST(TwoSquaresGraph)
{
  unique_ptr<IndexGraph> graph = BuildTwoSquaresGraph();

  vector<RoadPoint> const starts = {{2 /* feature id */, 0 /* point id */}, {2, 0}, {2, 0}};
  vector<RoadPoint> const finishes = {{6 /* feature id */, 0 /* point id */}, {6, 1}, {5, 1}};
  vector<RoadPoint> const expectedRoute0 = {{2 /* feature id */, 0 /* point id */}, {2, 1}, {2, 2}};
  vector<RoadPoint> const expectedRoute1 = {
      {2 /* feature id */, 0 /* point id */}, {2, 1}, {2, 2}, {6, 1}};
  vector<RoadPoint> const expectedRoute2 = {
      {2 /* feature id */, 0 /* point id */}, {2, 1}, {2, 2}, {6, 1}, {5, 1}};

  TestRoutes(starts, finishes, {expectedRoute0, expectedRoute1, expectedRoute2}, *graph);
}

// Route through two squares graph with adding a Fake-0 edge.
UNIT_CLASS_TEST(RestrictionTest, TwoSquaresGraph_AddFakeFeatureZero)
{
  Init(BuildTwoSquaresGraph());
  AddFakeFeature(m_graph->InsertJoint({2 /* feature id */, 1 /* point id */}),
                 GetJointIdForTesting({6 /* feature id */, 1 /* point id */}),
                 {{2, 1}, {6, 1}} /* geometrySource */, 1.0 /* speed */);

  vector<RoadPoint> const starts = {{2 /* feature id */, 0 /* point id */}, {2, 0}, {2, 0}};
  vector<RoadPoint> const finishes = {{6 /* feature id */, 0 /* point id */}, {6, 1}, {5, 1}};
  vector<RoadPoint> const expectedRoute0 = {{2 /* feature id */, 0 /* point id */}, {2, 1}, {2, 2}};
  vector<RoadPoint> const expectedRoute1 = {
      {2 /* feature id */, 0 /* point id */}, {2, 1}, {IndexGraph::kStartFakeFeatureIds, 1}};
  vector<RoadPoint> const expectedRoute2 = {{2 /* feature id */, 0 /* point id */},
                                            {2, 1},
                                            {IndexGraph::kStartFakeFeatureIds, 1},
                                            {5, 1}};

  TestRoutes(starts, finishes, {expectedRoute0, expectedRoute1, expectedRoute2}, *m_graph);
}

// Route through two squares graph with adding a Fake-0, Fake-1 and Fake-2 edge.
UNIT_CLASS_TEST(RestrictionTest, TwoSquaresGraph_AddFakeFeatureZeroOneTwo)
{
  Init(BuildTwoSquaresGraph());
  // Adding features: Fake 0, Fake 1 and Fake 2.
  AddFakeFeature(m_graph->InsertJoint({2 /* feature id */, 1 /* point id */}),
                 GetJointIdForTesting({6 /* feature id */, 1 /* point id */}),
                 {{2, 1}, {6, 1}} /* geometrySource */, 1.0 /* speed */);
  AddFakeFeature(m_graph->InsertJoint({1 /* feature id */, 1 /* point id */}),
                 GetJointIdForTesting({5 /* feature id */, 1 /* point id */}),
                        {{1, 1}, {5, 1}} /* geometrySource */, 1.0 /* speed */);
  AddFakeFeature({2 /* feature id */, 0 /* point id */}, {1 /* feature id */, 1 /* point id */},
                 {{2, 0}, {1, 1}} /* geometrySource */, 1.0 /* speed */);

  vector<RoadPoint> const starts = {{2 /* feature id */, 0 /* point id */}, {2, 0}, {2, 0}};
  vector<RoadPoint> const finishes = {{6 /* feature id */, 0 /* point id */}, {6, 1}, {5, 1}};
  vector<RoadPoint> const expectedRoute0 = {{2 /* feature id */, 0 /* point id */}, {2, 1}, {2, 2}};
  vector<RoadPoint> const expectedRoute1 = {
      {2 /* feature id */, 0 /* point id */}, {2, 1}, {IndexGraph::kStartFakeFeatureIds, 1}};
  vector<RoadPoint> const expectedRoute2 = {{IndexGraph::kStartFakeFeatureIds + 2 /* Fake 2 */, 0},
                                            {IndexGraph::kStartFakeFeatureIds + 2 /* Fake 2 */, 1},
                                            {IndexGraph::kStartFakeFeatureIds + 1 /* Fake 1 */, 1}};
  TestRoutes(starts, finishes, {expectedRoute0, expectedRoute1, expectedRoute2}, *m_graph);

  // Disabling Fake-2 feature.
  DisableEdge({IndexGraph::kStartFakeFeatureIds + 2 /* Fake 2 */, 0},
              {IndexGraph::kStartFakeFeatureIds + 2 /* Fake 2 */, 1},
              IndexGraph::kStartFakeFeatureIds + 2 /* Fake feature id */);
  vector<RoadPoint> const expectedRoute2Disable2 = {{2 /* feature id */, 0 /* point id */},
                                                    {2, 1},
                                                    {IndexGraph::kStartFakeFeatureIds, 1},
                                                    {5, 1}};
  TestRoutes(starts, finishes, {expectedRoute0, expectedRoute1, expectedRoute2Disable2}, *m_graph);
}

//      Finish
// 1 *-F4-*-F5-*
//   |         |
//   F2        F3
//   |         |
// 0 *---F1----*---F0---* Start
//   0         1        2
// Note 1. All features are two-way. (It's possible to move along any direction of the features.)
// Note 2. Any feature contains of one segment.
unique_ptr<IndexGraph> BuildFlagGraph()
{
  unique_ptr<TestGeometryLoader> loader = make_unique<TestGeometryLoader>();
  loader->AddRoad(0 /* feature id */, false /* one way */, 1.0 /* speed */,
                  RoadGeometry::Points({{2.0, 0.0}, {1.0, 0.0}}));
  loader->AddRoad(1 /* feature id */, false /* one way */, 1.0 /* speed */,
                  RoadGeometry::Points({{1.0, 0.0}, {0.0, 0.0}}));
  loader->AddRoad(2 /* feature id */, false /* one way */, 1.0 /* speed */,
                  RoadGeometry::Points({{0.0, 0.0}, {0.0, 1.0}}));
  loader->AddRoad(3 /* feature id */, false /* one way */, 1.0 /* speed */,
                  RoadGeometry::Points({{1.0, 0.0}, {1.0, 1.0}}));
  loader->AddRoad(4 /* feature id */, false /* one way */, 1.0 /* speed */,
                  RoadGeometry::Points({{0.0, 1.0}, {0.5, 1.0}}));
  loader->AddRoad(5 /* feature id */, false /* one way */, 1.0 /* speed */,
                  RoadGeometry::Points({{0.5, 1.0}, {1.0, 1.0}}));

  vector<Joint> const joints = {
      MakeJoint({{1 /* feature id */, 1 /* point id */}, {2, 0}}), /* joint at point (0, 0) */
      MakeJoint({{2, 1}, {4, 0}}),                                 /* joint at point (0, 1) */
      MakeJoint({{4, 1}, {5, 0}}),                                 /* joint at point (0.5, 1) */
      MakeJoint({{1, 0}, {3, 0}, {0, 1}}),                         /* joint at point (1, 0) */
      MakeJoint({{3, 1}, {5, 1}}),                                 /* joint at point (1, 1) */
  };

  traffic::TrafficCache const trafficCache;
  unique_ptr<IndexGraph> graph = make_unique<IndexGraph>(move(loader), CreateEstimator(trafficCache));
  graph->Import(joints);
  // Note. It's necessary to insert start joint because the edge F0 is used
  // for creating restrictions.
  graph->InsertJoint({0 /* feature id */, 0 /* point id */});  // start
  return graph;
}

// @TODO(bykoianko) It's necessary to implement put several no restriction on the same junction
// ingoing edges of the same jucntion. For example test with flag graph with two restriction
// type no from F0 to F3 and form F0 to F1.

// Route through flag graph without any restrictions.
UNIT_TEST(FlagGraph)
{
  unique_ptr<IndexGraph> graph = BuildFlagGraph();
  IndexGraphStarter starter(*graph, RoadPoint(0, 0) /* start */, RoadPoint(5, 0) /* finish */);
  vector<m2::PointD> const expectedGeom = {{2 /* x */, 0 /* y */}, {1, 0}, {1, 1}, {0.5, 1}};
  TestRouteGeometry(starter, AStarAlgorithm<IndexGraphStarter>::Result::OK, expectedGeom);
}

// Route through flag graph with one restriciton (type no) from F0 to F3.
UNIT_CLASS_TEST(RestrictionTest, FlagGraph_RestrictionF0F3No)
{
  Init(BuildFlagGraph());
  Joint::Id const restictionCenterId = GetJointIdForTesting({0, 1});

  // Testing outgoing and ingoing edge number near restriction joint.
  EdgeTest(restictionCenterId, 3 /* expectedIntgoingNum */, 3 /* expectedOutgoingNum */,
           false /* graphWithoutRestrictions */, *m_graph);
  ApplyRestrictionNoRealFeatures(RestrictionPoint({0 /* feature id */, 1 /* point id */},
                                 {3 /* feature id */, 0 /* point id */}, restictionCenterId));
  EdgeTest(restictionCenterId, 2 /* expectedIntgoingNum */, 3 /* expectedOutgoingNum */,
           false /* graphWithoutRestrictions */, *m_graph);
  EdgeTest(restictionCenterId, 3 /* expectedIntgoingNum */, 3 /* expectedOutgoingNum */,
           true /* graphWithoutRestrictions */, *m_graph);

  // Testing route building after adding the restriction.
  SetStarter(RoadPoint(0, 0) /* start */, RoadPoint(5, 0) /* finish */);
  vector<m2::PointD> const expectedGeom = {
      {2 /* x */, 0 /* y */}, {1, 0}, {0, 0}, {0, 1}, {0.5, 1}};
  TestRouteGeometry(*m_starter, AStarAlgorithm<IndexGraphStarter>::Result::OK, expectedGeom);
}

// Route through flag graph with one restriciton (type only) from F0 to F1.
UNIT_CLASS_TEST(RestrictionTest, FlagGraph_RestrictionF0F1Only)
{
  Init(BuildFlagGraph());
  Joint::Id const restictionCenterId = GetJointIdForTesting({0, 1});
  ApplyRestrictionOnlyRealFeatures(RestrictionPoint({0 /* feature id */, 1 /* point id */},
                                   {1 /* feature id */, 0 /* point id */}, restictionCenterId));

  SetStarter(RoadPoint(0, 0) /* start */, RoadPoint(5, 0) /* finish */);
  vector<RoadPoint> const expectedRoute = {{IndexGraph::kStartFakeFeatureIds, 0 /* point id */},
                                           {IndexGraph::kStartFakeFeatureIds, 1},
                                           {IndexGraph::kStartFakeFeatureIds + 1, 1},
                                           {2 /* feature id */, 1 /* point id */},
                                           {4, 1}};
  TestRouteSegments(*m_starter, AStarAlgorithm<IndexGraphStarter>::Result::OK, expectedRoute);
}

// 1 *-F4-*-F5-*---F6---* Finish
//   |         |
//   F2        F3
//   |         |
// 0 *---F1----*---F0---* Start
//   0         1        2
// Note 1. All features are two-way. (It's possible to move along any direction of the features.)
// Note 2. Any feature contains of one segment.
unique_ptr<IndexGraph> BuildPosterGraph()
{
    unique_ptr<TestGeometryLoader> loader = make_unique<TestGeometryLoader>();
    loader->AddRoad(0 /* feature id */, false /* one way */, 1.0 /* speed */,
                    RoadGeometry::Points({{2.0, 0.0}, {1.0, 0.0}}));
    loader->AddRoad(1 /* feature id */, false /* one way */, 1.0 /* speed */,
                    RoadGeometry::Points({{1.0, 0.0}, {0.0, 0.0}}));
    loader->AddRoad(2 /* feature id */, false /* one way */, 1.0 /* speed */,
                    RoadGeometry::Points({{0.0, 0.0}, {0.0, 1.0}}));
    loader->AddRoad(3 /* feature id */, false /* one way */, 1.0 /* speed */,
                    RoadGeometry::Points({{1.0, 0.0}, {1.0, 1.0}}));
    loader->AddRoad(4 /* feature id */, false /* one way */, 1.0 /* speed */,
                    RoadGeometry::Points({{0.0, 1.0}, {0.5, 1.0}}));
    loader->AddRoad(5 /* feature id */, false /* one way */, 1.0 /* speed */,
                    RoadGeometry::Points({{0.5, 1.0}, {1.0, 1.0}}));
    loader->AddRoad(6 /* feature id */, false /* one way */, 1.0 /* speed */,
                    RoadGeometry::Points({{1.0, 1.0}, {2.0, 1.0}}));

  vector<Joint> const joints = {
      MakeJoint({{1 /* feature id */, 1 /* point id */}, {2, 0}}), /* joint at point (0, 0) */
      MakeJoint({{2, 1}, {4, 0}}),                                 /* joint at point (0, 1) */
      MakeJoint({{4, 1}, {5, 0}}),                                 /* joint at point (0.5, 1) */
      MakeJoint({{1, 0}, {3, 0}, {0, 1}}),                         /* joint at point (1, 0) */
      MakeJoint({{3, 1}, {5, 1}, {6, 0}}),                         /* joint at point (1, 1) */
  };

  traffic::TrafficCache const trafficCache;
  unique_ptr<IndexGraph> graph = make_unique<IndexGraph>(move(loader), CreateEstimator(trafficCache));
  graph->Import(joints);
  // Note. It's necessary to insert start and finish joints because the edge F0 is used
  // for creating restrictions.
  graph->InsertJoint({0 /* feature id */, 0 /* point id */});  // start
  graph->InsertJoint({6 /* feature id */, 1 /* point id */});  // finish

  return graph;
}

// Route through poster graph without any restrictions.
UNIT_TEST(PosterGraph)
{
  unique_ptr<IndexGraph> graph = BuildPosterGraph();
  IndexGraphStarter starter(*graph, RoadPoint(0, 0) /* start */, RoadPoint(6, 1) /* finish */);
  vector<m2::PointD> const expectedGeom = {{2 /* x */, 0 /* y */}, {1, 0}, {1, 1}, {2, 1}};

  TestRouteGeometry(starter, AStarAlgorithm<IndexGraphStarter>::Result::OK, expectedGeom);
}

// Route through poster graph with restrictions F0-F3 (type no).
UNIT_CLASS_TEST(RestrictionTest, PosterGraph_RestrictionF0F3No)
{
  Init(BuildPosterGraph());
  Joint::Id const restictionCenterId = GetJointIdForTesting({0, 1});

  ApplyRestrictionNoRealFeatures(RestrictionPoint({0 /* feature id */, 1 /* point id */},
                                 {3, 0}, restictionCenterId));

  SetStarter(RoadPoint(0, 0) /* start */, RoadPoint(6, 1) /* finish */);
  vector<m2::PointD> const expectedGeom = {
      {2 /* x */, 0 /* y */}, {1, 0}, {0, 0}, {0, 1}, {0.5, 1}, {1, 1}, {2, 1}};
  TestRouteGeometry(*m_starter, AStarAlgorithm<IndexGraphStarter>::Result::OK, expectedGeom);
}

// Route through poster graph with restrictions F0-F1 (type only).
UNIT_CLASS_TEST(RestrictionTest, PosterGraph_RestrictionF0F1Only)
{
  Init(BuildPosterGraph());
  Joint::Id const restictionCenterId = GetJointIdForTesting({0, 1});

  ApplyRestrictionOnlyRealFeatures(RestrictionPoint({0 /* feature id */, 1 /* point id */},
                                   {1, 0}, restictionCenterId));

  m_graph->ForEachNonBlockedEdgeMappingNode(
        DirectedEdge(GetJointIdForTesting({0 /* feature id */, 0 /* point id */}),
        GetJointIdForTesting({0, 1}), 0 /* feature id */),
                                [](DirectedEdge const & leaf){
    TEST_EQUAL(leaf, DirectedEdge(5 /* form joint id */, 7 /* to joint id */,
               IndexGraph::kStartFakeFeatureIds), ());
    return;
  });

  SetStarter(RoadPoint(0, 0) /* start */, RoadPoint(6, 1) /* finish */);
  vector<m2::PointD> const expectedGeom = {
      {2 /* x */, 0 /* y */}, {1, 0}, {0, 0}, {0, 1}, {0.5, 1}, {1, 1}, {2, 1}};
  TestRouteGeometry(*m_starter, AStarAlgorithm<IndexGraphStarter>::Result::OK, expectedGeom);
}

// 1               *--F1-->*
//               ↗          ↘
//            F1               F1
//          ↗                   ↘
// Start 0 *---F0--->-------F0----->* Finish
//         0        1       2       3
// Start
// Note. F0 is a two setments feature. F1 is a three segment one.
unique_ptr<IndexGraph> BuildTwoWayGraph()
{
  unique_ptr<TestGeometryLoader> loader = make_unique<TestGeometryLoader>();
  loader->AddRoad(0 /* feature id */, true /* oneWay */, 1.0 /* speed */,
                  RoadGeometry::Points({{0.0, 0.0}, {1.0, 0.0}, {3.0, 0}}));
  loader->AddRoad(
      1 /* feature id */, true /* oneWay */, 1.0 /* speed */,
      RoadGeometry::Points({{0.0, 0.0}, {1.0, 1.0}, {2.0, 1.0}, {3.0, 0.0}}));

  vector<Joint> const joints = {
      MakeJoint({{0 /* feature id */, 0 /* point id */}, {1, 0}}), /* joint at point (0, 0) */
      MakeJoint({{0 /* feature id */, 2 /* point id */}, {1, 3}})};

  traffic::TrafficCache const trafficCache;
  unique_ptr<IndexGraph> graph = make_unique<IndexGraph>(move(loader), CreateEstimator(trafficCache));
  graph->Import(joints);
  return graph;
}

UNIT_CLASS_TEST(RestrictionTest, TwoWay_GetSingleFeaturePath)
{
  Init(BuildTwoWayGraph());
  vector<RoadPoint> singleFeaturePath;

  // Full feature 0.
  GetSingleFeaturePath({0 /* feature id */, 0 /* point id */}, {0, 2}, singleFeaturePath);
  vector<RoadPoint> const expectedF0 = {{0 /* feature id */, 0 /* point id */}, {0, 1}, {0, 2}};
  TEST_EQUAL(singleFeaturePath, expectedF0, ());

  // Full feature 1.
  GetSingleFeaturePath({1 /* feature id */, 0 /* point id */}, {1, 3}, singleFeaturePath);
  vector<RoadPoint> const expectedF1 = {
      {1 /* feature id */, 0 /* point id */}, {1, 1}, {1, 2}, {1, 3}};
  TEST_EQUAL(singleFeaturePath, expectedF1, ());

  // Full feature 1 in reversed order.
  GetSingleFeaturePath({1 /* feature id */, 3 /* point id */}, {1, 0}, singleFeaturePath);
  vector<RoadPoint> const expectedReversedF1 = {
      {1 /* feature id */, 3 /* point id */}, {1, 2}, {1, 1}, {1, 0}};
  TEST_EQUAL(singleFeaturePath, expectedReversedF1, ());

  // Part of feature 0.
  GetSingleFeaturePath({0 /* feature id */, 1 /* point id */}, {0, 2}, singleFeaturePath);
  vector<RoadPoint> const expectedPartF0 = {{0 /* feature id */, 1 /* point id */}, {0, 2}};
  TEST_EQUAL(singleFeaturePath, expectedPartF0, ());

  // Part of feature 1 in reversed order.
  GetSingleFeaturePath({1 /* feature id */, 2 /* point id */}, {1, 1}, singleFeaturePath);
  vector<RoadPoint> const expectedPartF1 = {{1 /* feature id */, 2 /* point id */}, {1, 1}};
  TEST_EQUAL(singleFeaturePath, expectedPartF1, ());

  // Single point test.
  GetSingleFeaturePath({0 /* feature id */, 0 /* point id */}, {0, 0}, singleFeaturePath);
  vector<RoadPoint> const expectedSinglePoint = {{0 /* feature id */, 0 /* point id */}};
  TEST_EQUAL(singleFeaturePath, expectedSinglePoint, ());
}

UNIT_CLASS_TEST(RestrictionTest, TwoWay_GetConnectionPaths)
{
  Init(BuildTwoWayGraph());

  vector<vector<RoadPoint>> connectionPaths;
  GetConnectionPaths({1 /* feature id */, 0 /* point id */}, {0, 2}, connectionPaths);
  vector<vector<RoadPoint>> const expectedConnectionPaths = {
      {{0, 0}, {0, 1}, {0, 2}},          // feature 0
      {{1, 0}, {1, 1}, {1, 2}, {1, 3}},  // feature 1
  };
  sort(connectionPaths.begin(), connectionPaths.end());
  TEST_EQUAL(connectionPaths, expectedConnectionPaths, ());
}

UNIT_CLASS_TEST(RestrictionTest, TwoWay_GetFeatureConnectionPath)
{
  Init(BuildTwoWayGraph());
  vector<RoadPoint> featurePath;

  // Full feature 0.
  GetFeatureConnectionPath({1 /* feature id */, 0 /* point id */}, {1, 3},
                           0 /* feature id */, featurePath);
  vector<RoadPoint> const expectedF0Path = {{0, 0}, {0, 1}, {0, 2}};
  TEST_EQUAL(featurePath, expectedF0Path, ());

  // Full feature 1.
  GetFeatureConnectionPath({1 /* feature id */, 0 /* point id */}, {1, 3},
                            1 /* feature id */, featurePath);
  vector<RoadPoint> const expectedF1Path = {{1, 0}, {1, 1}, {1, 2}, {1, 3}};
  TEST_EQUAL(featurePath, expectedF1Path, ());

  // Reversed full feature 0.
  GetFeatureConnectionPath({1 /* feature id */, 3 /* point id */}, {1, 0},
                           0 /* feature id */, featurePath);
  vector<RoadPoint> const expectedReversedF0Path = {{0, 2}, {0, 1}, {0, 0}};
  TEST_EQUAL(featurePath, expectedReversedF0Path, ());
}
}  // namespace routing_test
