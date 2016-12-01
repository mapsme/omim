#include "testing/testing.hpp"

#include "routing/routing_tests/index_graph_tools.hpp"

#include "routing/car_model.hpp"
#include "routing/geometry.hpp"

#include "geometry/point2d.hpp"

#include "std/unique_ptr.hpp"
#include "std/vector.hpp"

namespace
{
using namespace routing;
using namespace routing_test;

//               Finish
//  3               *
//                  ^
//                  F5
//                  |
// 2 *              *
//    ↖          ↗   ↖
//      F2      F3      F4
//        ↖  ↗           ↖
// 1        *               *
//        ↗  ↖
//      F0      F1
//    ↗          ↖
// 0 *              *
//   0       1      2       3
//                Start
// Note. This graph contains of 6 one segment directed features.
unique_ptr<IndexGraph> BuildXYGraph()
{
  unique_ptr<TestGeometryLoader> loader = make_unique<TestGeometryLoader>();
  loader->AddRoad(0 /* featureId */, true /* oneWay */, 1.0 /* speed */,
                  RoadGeometry::Points({{0.0, 0.0}, {1.0, 1.0}}));
  loader->AddRoad(1 /* featureId */, true /* oneWay */, 1.0 /* speed */,
                  RoadGeometry::Points({{2.0, 0.0}, {1.0, 1.0}}));
  loader->AddRoad(2 /* featureId */, true /* oneWay */, 1.0 /* speed */,
                  RoadGeometry::Points({{1.0, 1.0}, {0.0, 2.0}}));
  loader->AddRoad(3 /* featureId */, true /* oneWay */, 1.0 /* speed */,
                  RoadGeometry::Points({{1.0, 1.0}, {2.0, 2.0}}));
  loader->AddRoad(4 /* featureId */, true /* oneWay */, 1.0 /* speed */,
                  RoadGeometry::Points({{3.0, 1.0}, {2.0, 2.0}}));
  loader->AddRoad(5 /* featureId */, true /* oneWay */, 1.0 /* speed */,
                  RoadGeometry::Points({{2.0, 2.0}, {2.0, 3.0}}));

  vector<Joint> const joints = {
    MakeJoint({{0 /* feature id */, 0 /* point id */}}), /* joint at point (0, 0) */
    MakeJoint({{1, 0}}), /* joint at point (2, 0) */
    MakeJoint({{0, 1}, {1, 1}, {2, 0}, {3, 0}}), /* joint at point (1, 1) */
    MakeJoint({{2, 1}}), /* joint at point (0, 2) */
    MakeJoint({{3, 1}, {4, 1}, {5, 0}}), /* joint at point (2, 2) */
    MakeJoint({{4, 0}}), /* joint at point (3, 1) */
    MakeJoint({{5, 1}}), /* joint at point (2, 3) */
  };

  unique_ptr<IndexGraph> graph = make_unique<IndexGraph>(move(loader), CreateEstimator());
  graph->Import(joints);
  return graph;
}

// Route through XY graph without any restrictions.
UNIT_TEST(XYGraph)
{
  unique_ptr<IndexGraph> graph = BuildXYGraph();
  IndexGraphStarter starter(*graph, RoadPoint(1, 0) /* start */, RoadPoint(5, 1) /* finish */);
  vector<m2::PointD> const expectedGeom = {{2 /* x */, 0 /* y */}, {1, 1}, {2, 2}, {2, 3}};
  TestRouteGeometry(starter, AStarAlgorithm<IndexGraphStarter>::Result::OK, expectedGeom);
}

// Route through XY graph with one restriciton (type only) from F1 to F3.
UNIT_TEST(XYGraph_RestrictionF1F3Only)
{
  unique_ptr<IndexGraph> graph = BuildXYGraph();

  Joint::Id const restictionF1F3Id = graph->GetJointIdForTesting(
      {1 /* feature id */, 1 /* point id */});
  graph->ApplyRestrictionOnlyRealFeatures(RestrictionPoint({1 /* feature id */, 1 /* point id */},
                                          {3, 0}, restictionF1F3Id));

  IndexGraphStarter starter(*graph, RoadPoint(1, 0) /* start */, RoadPoint(5, 1) /* finish */);
  vector<m2::PointD> const expectedGeom = {{2 /* x */, 0 /* y */}, {1, 1}, {2, 2}, {2, 3}};
  TestRouteGeometry(starter, AStarAlgorithm<IndexGraphStarter>::Result::OK, expectedGeom);
}

// Route through XY graph with one restriciton (type only) from F3 to F5.
UNIT_TEST(XYGraph_RestrictionF3F5Only)
{
  unique_ptr<IndexGraph> graph = BuildXYGraph();

  Joint::Id const restictionF3F5Id = graph->GetJointIdForTesting(
      {3 /* feature id */, 1 /* point id */});
  graph->ApplyRestrictionOnlyRealFeatures(RestrictionPoint({3 /* feature id */, 1 /* point id */},
                                          {5, 0}, restictionF3F5Id));

  IndexGraphStarter starter(*graph, RoadPoint(1, 0) /* start */, RoadPoint(5, 1) /* finish */);
  vector<m2::PointD> const expectedGeom = {{2 /* x */, 0 /* y */}, {1, 1}, {2, 2}, {2, 3}};
  TestRouteGeometry(starter, AStarAlgorithm<IndexGraphStarter>::Result::OK, expectedGeom);
}

// Cumulative case. Route through XY graph with two restricitons (type only) applying
// according to the order. First from F1 to F3 and then and from F3 to F5.
UNIT_TEST(XYGraph_RestrictionF1F3AndF3F5Only)
{
  unique_ptr<IndexGraph> graph = BuildXYGraph();

  Joint::Id const restictionF1F3Id = graph->GetJointIdForTesting(
      {1 /* feature id */, 1 /* point id */});
  graph->ApplyRestrictionOnlyRealFeatures(RestrictionPoint({1 /* feature id */, 1 /* point id */},
                                          {3, 0}, restictionF1F3Id));

  Joint::Id const restictionF3F5Id = graph->GetJointIdForTesting(
      {3 /* feature id */, 1 /* point id */});
  graph->ApplyRestrictionOnlyRealFeatures(RestrictionPoint({3 /* feature id */, 1 /* point id */},
                                          {5, 0}, restictionF3F5Id));

  IndexGraphStarter starter(*graph, RoadPoint(1, 0) /* start */, RoadPoint(5, 1) /* finish */);
  vector<m2::PointD> const expectedGeom = {{2 /* x */, 0 /* y */}, {1, 1}, {2, 2}, {2, 3}};
  TestRouteGeometry(starter, AStarAlgorithm<IndexGraphStarter>::Result::OK, expectedGeom);
}

// Cumulative case. Route through XY graph with two restricitons (type only) applying
// according to the order. First from F3 to F5 and then and from F1 to F3.
UNIT_TEST(XYGraph_RestrictionF3F5AndF1F3Only)
{
  unique_ptr<IndexGraph> graph = BuildXYGraph();

  Joint::Id const restictionF3F5Id = graph->GetJointIdForTesting(
      {3 /* feature id */, 1 /* point id */});
  graph->ApplyRestrictionOnlyRealFeatures(RestrictionPoint({3 /* feature id */, 1 /* point id */},
                                          {5, 0}, restictionF3F5Id));

  Joint::Id const restictionF1F3Id = graph->GetJointIdForTesting(
      {1 /* feature id */, 1 /* point id */});
  graph->ApplyRestrictionOnlyRealFeatures(RestrictionPoint({1 /* feature id */, 1 /* point id */},
                                          {3, 0}, restictionF1F3Id));

  IndexGraphStarter starter(*graph, RoadPoint(1, 0) /* start */, RoadPoint(5, 1) /* finish */);
  vector<m2::PointD> const expectedGeom = {{2 /* x */, 0 /* y */}, {1, 1}, {2, 2}, {2, 3}};
  TestRouteGeometry(starter, AStarAlgorithm<IndexGraphStarter>::Result::OK, expectedGeom);
}

// Cumulative case. Route through XY graph with two restricitons applying
// according to the order. First from F3 to F5 (type only)
// and then and from F0 to F2 (type no).
UNIT_TEST(XYGraph_RestrictionF3F5OnlyAndF0F2No)
{
  unique_ptr<IndexGraph> graph = BuildXYGraph();

  Joint::Id const restictionF3F5Id = graph->GetJointIdForTesting(
      {3 /* feature id */, 1 /* point id */});
  graph->ApplyRestrictionOnlyRealFeatures(RestrictionPoint({3 /* feature id */, 1 /* point id */},
                                          {5, 0}, restictionF3F5Id));

  Joint::Id const restictionF1F3Id = graph->GetJointIdForTesting(
      {1 /* feature id */, 1 /* point id */});
  graph->ApplyRestrictionNoRealFeatures(RestrictionPoint({0 /* feature id */, 1 /* point id */},
                                        {2, 0}, restictionF1F3Id));

  IndexGraphStarter starter(*graph, RoadPoint(1, 0) /* start */, RoadPoint(5, 1) /* finish */);
  vector<m2::PointD> const expectedGeom = {{2 /* x */, 0 /* y */}, {1, 1}, {2, 2}, {2, 3}};
  TestRouteGeometry(starter, AStarAlgorithm<IndexGraphStarter>::Result::OK, expectedGeom);
}

// Cumulative case. Trying to build route through XY graph with two restricitons applying
// according to the order. First from F3 to F5 (type only)
// and then and from F1 to F3 (type no).
UNIT_TEST(XYGraph_RestrictionF3F5OnlyAndF1F3No)
{
  unique_ptr<IndexGraph> graph = BuildXYGraph();

  Joint::Id const restictionF3F5Id = graph->GetJointIdForTesting(
      {3 /* feature id */, 1 /* point id */});
  graph->ApplyRestrictionOnlyRealFeatures(RestrictionPoint({3 /* feature id */, 1 /* point id */},
                                          {5, 0}, restictionF3F5Id));

  Joint::Id const restictionF1F3Id = graph->GetJointIdForTesting(
      {1 /* feature id */, 1 /* point id */});
  graph->ApplyRestrictionNoRealFeatures(RestrictionPoint({1 /* feature id */, 1 /* point id */},
                                        {3, 0}, restictionF1F3Id));

  IndexGraphStarter starter(*graph, RoadPoint(1, 0) /* start */, RoadPoint(5, 1) /* finish */);
  vector<m2::PointD> const expectedGeom = {};
  TestRouteGeometry(starter, AStarAlgorithm<IndexGraphStarter>::Result::NoPath, expectedGeom);
}

//                        Finish
//  3        *              *
//             ↖          ↗
//              F5     F6
//                ↖   ↗
// 2 *              *
//    ↖          ↗   ↖
//      F2      F3      F4
//        ↖  ↗           ↖
// 1        *               *
//        ↗  ↖             ^
//      F0      F1          F8
//    ↗          ↖         |
// 0 *              *--F7--->*
//   0       1      2       3
//                Start
// Note. This graph contains of 9 one segment directed features.
unique_ptr<IndexGraph> BuildXXGraph()
{
  unique_ptr<TestGeometryLoader> loader = make_unique<TestGeometryLoader>();
  loader->AddRoad(0 /* featureId */, true /* oneWay */, 1.0 /* speed */,
                  RoadGeometry::Points({{0.0, 0.0}, {1.0, 1.0}}));
  loader->AddRoad(1 /* featureId */, true /* oneWay */, 1.0 /* speed */,
                  RoadGeometry::Points({{2.0, 0.0}, {1.0, 1.0}}));
  loader->AddRoad(2 /* featureId */, true /* oneWay */, 1.0 /* speed */,
                  RoadGeometry::Points({{1.0, 1.0}, {0.0, 2.0}}));
  loader->AddRoad(3 /* featureId */, true /* oneWay */, 1.0 /* speed */,
                  RoadGeometry::Points({{1.0, 1.0}, {2.0, 2.0}}));
  loader->AddRoad(4 /* featureId */, true /* oneWay */, 1.0 /* speed */,
                  RoadGeometry::Points({{3.0, 1.0}, {2.0, 2.0}}));
  loader->AddRoad(5 /* featureId */, true /* oneWay */, 1.0 /* speed */,
                  RoadGeometry::Points({{2.0, 2.0}, {1.0, 3.0}}));
  loader->AddRoad(6 /* featureId */, true /* oneWay */, 1.0 /* speed */,
                  RoadGeometry::Points({{2.0, 2.0}, {3.0, 3.0}}));
  loader->AddRoad(7 /* featureId */, true /* oneWay */, 1.0 /* speed */,
                  RoadGeometry::Points({{2.0, 0.0}, {3.0, 0.0}}));
  loader->AddRoad(8 /* featureId */, true /* oneWay */, 1.0 /* speed */,
                  RoadGeometry::Points({{3.0, 0.0}, {3.0, 1.0}}));

  vector<Joint> const joints = {
    MakeJoint({{0 /* feature id */, 0 /* point id */}}), /* joint at point (0, 0) */
    MakeJoint({{1, 0}, {7, 0}}), /* joint at point (2, 0) */
    MakeJoint({{0, 1}, {1, 1}, {2, 0}, {3, 0}}), /* joint at point (1, 1) */
    MakeJoint({{2, 1}}), /* joint at point (0, 2) */
    MakeJoint({{3, 1}, {4, 1}, {5, 0}, {6, 0}}), /* joint at point (2, 2) */
    MakeJoint({{4, 0}, {8, 1}}), /* joint at point (3, 1) */
    MakeJoint({{5, 1}}), /* joint at point (1, 3) */
    MakeJoint({{6, 1}}), /* joint at point (3, 3) */
    MakeJoint({{7, 1}, {8, 0}}), /* joint at point (3, 0) */
  };

  unique_ptr<IndexGraph> graph = make_unique<IndexGraph>(move(loader), CreateEstimator());
  graph->Import(joints);
  return graph;
}

// Route through XY graph without any restrictions.
UNIT_TEST(XXGraph)
{
  unique_ptr<IndexGraph> graph = BuildXXGraph();
  IndexGraphStarter starter(*graph, RoadPoint(1, 0) /* start */, RoadPoint(6, 1) /* finish */);
  vector<m2::PointD> const expectedGeom = {{2 /* x */, 0 /* y */}, {1, 1}, {2, 2}, {3, 3}};
  TestRouteGeometry(starter, AStarAlgorithm<IndexGraphStarter>::Result::OK, expectedGeom);
}

// Cumulative case. Route through XX graph with two restricitons (type only) applying
// according to the order. First from F1 to F3 and then and from F3 to F6.
UNIT_TEST(XXGraph_RestrictionF1F3AndF3F6Only)
{
  unique_ptr<IndexGraph> graph = BuildXXGraph();

  Joint::Id const restictionF1F3Id = graph->GetJointIdForTesting(
      {1 /* feature id */, 1 /* point id */});
  graph->ApplyRestrictionOnlyRealFeatures(RestrictionPoint({1 /* feature id */, 1 /* point id */},
                                          {3, 0}, restictionF1F3Id));

  Joint::Id const restictionF3F6Id = graph->GetJointIdForTesting(
      {3 /* feature id */, 1 /* point id */});
  graph->ApplyRestrictionOnlyRealFeatures(RestrictionPoint({3 /* feature id */, 1 /* point id */},
                                          {6, 0}, restictionF3F6Id));

  IndexGraphStarter starter(*graph, RoadPoint(1, 0) /* start */, RoadPoint(6, 1) /* finish */);
  vector<m2::PointD> const expectedGeom = {{2 /* x */, 0 /* y */}, {1, 1}, {2, 2}, {3, 3}};
  TestRouteGeometry(starter, AStarAlgorithm<IndexGraphStarter>::Result::OK, expectedGeom);
}

// Route through XX graph with one restriciton (type no) from F1 to F3.
UNIT_TEST(XXGraph_RestrictionF1F3No)
{
  unique_ptr<IndexGraph> graph = BuildXXGraph();

  Joint::Id const restictionF1F3Id = graph->GetJointIdForTesting(
      {1 /* feature id */, 1 /* point id */});
  graph->ApplyRestrictionNoRealFeatures(RestrictionPoint({1 /* feature id */, 1 /* point id */},
                                        {3, 0}, restictionF1F3Id));

  IndexGraphStarter starter(*graph, RoadPoint(1, 0) /* start */, RoadPoint(6, 1) /* finish */);
  vector<m2::PointD> const expectedGeom = {{2 /* x */, 0 /* y */}, {3, 0}, {3, 1}, {2, 2}, {3, 3}};
  TestRouteGeometry(starter, AStarAlgorithm<IndexGraphStarter>::Result::OK, expectedGeom);
}

// Cumulative case. Route through XX graph with four restricitons applying
// according to the order:
// * from F1 to F3 type
// * from F7 to F8 type only
// * from F8 to F4 type only
// * from F4 to F6 type only
UNIT_TEST(XXGraph_RestrictionF1F3NoF7F8OnlyF8F4OnlyF4F6Only)
{
  unique_ptr<IndexGraph> graph = BuildXXGraph();

  Joint::Id const restictionF1F3Id = graph->GetJointIdForTesting(
      {1 /* feature id */, 1 /* point id */});
  graph->ApplyRestrictionNoRealFeatures(RestrictionPoint({1 /* feature id */, 1 /* point id */},
                                        {3, 0}, restictionF1F3Id));

  Joint::Id const restictionF7F8Id = graph->GetJointIdForTesting(
      {7 /* feature id */, 1 /* point id */});
  graph->ApplyRestrictionOnlyRealFeatures(RestrictionPoint({7 /* feature id */, 1 /* point id */},
                                          {8, 0}, restictionF7F8Id));

  Joint::Id const restictionF8F4Id = graph->GetJointIdForTesting(
      {8 /* feature id */, 1 /* point id */});
  graph->ApplyRestrictionOnlyRealFeatures(RestrictionPoint({8 /* feature id */, 1 /* point id */},
                                          {4, 0}, restictionF8F4Id));

  Joint::Id const restictionF4F6Id = graph->GetJointIdForTesting(
      {4 /* feature id */, 1 /* point id */});
  graph->ApplyRestrictionOnlyRealFeatures(RestrictionPoint({4 /* feature id */, 1 /* point id */},
                                          {6, 0}, restictionF4F6Id));

  IndexGraphStarter starter(*graph, RoadPoint(1, 0) /* start */, RoadPoint(6, 1) /* finish */);
  vector<m2::PointD> const expectedGeom = {{2 /* x */, 0 /* y */}, {3, 0}, {3, 1}, {2, 2}, {3, 3}};
  TestRouteGeometry(starter, AStarAlgorithm<IndexGraphStarter>::Result::OK, expectedGeom);
}
}  // namespace
