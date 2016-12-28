#pragma once

#include "routing/edge_estimator.hpp"
#include "routing/index_graph.hpp"
#include "routing/index_graph_starter.hpp"
#include "routing/road_point.hpp"

#include "routing/base/astar_algorithm.hpp"

#include "traffic/traffic_info.hpp"

#include "indexer/classificator_loader.hpp"

#include "geometry/point2d.hpp"

#include "std/algorithm.hpp"
#include "std/shared_ptr.hpp"
#include "std/unique_ptr.hpp"
#include "std/unordered_map.hpp"
#include "std/vector.hpp"

namespace routing_test
{
using namespace routing;

struct RestrictionTest
{
  RestrictionTest() { classificator::Load(); }

  void Init(unique_ptr<IndexGraph> graph) { m_graph = move(graph); }
  void SetStarter(RoadPoint const & start, RoadPoint const & finish)
  {
    m_starter = make_unique<IndexGraphStarter>(*m_graph, start, finish);
  }

  bool GetRestrictionPoint(uint32_t featureIdFrom, uint32_t featureIdTo,
                           RestrictionPoint & crossingPoint) const
  {
    return m_graph->m_roadIndex.GetRestrictionPoint(featureIdFrom, featureIdTo, crossingPoint);
  }

  void DisableEdge(RoadPoint const & from, RoadPoint const & to, uint32_t featureId)
  {
    m_graph->DisableEdge(DirectedEdge(GetJointId(from), GetJointId(to), featureId));
  }

  uint32_t AddFakeFeature(RoadPoint const & from, RoadPoint const & to,
                          vector<RoadPoint> const & geometrySource, double speed)
  {
    return m_graph->AddFakeFeature(GetJointId(from), GetJointId(to), geometrySource, speed);
  }

  uint32_t AddFakeFeature(Joint::Id from, Joint::Id to, vector<RoadPoint> const & geometrySource,
                          double speed)
  {
    return m_graph->AddFakeFeature(from, to, geometrySource, speed);
  }

  Joint::Id GetJointId(RoadPoint const & rp) const { return m_graph->m_roadIndex.GetJointId(rp); }
  void ApplyRestrictionNoRealFeatures(RestrictionPoint const & restrictionPoint)
  {
    m_graph->ApplyRestrictionNoRealFeatures(restrictionPoint);
  }

  void ApplyRestrictionOnlyRealFeatures(RestrictionPoint const & restrictionPoint)
  {
    m_graph->ApplyRestrictionOnlyRealFeatures(restrictionPoint);
  }

  void GetSingleFeaturePath(RoadPoint const & from, RoadPoint const & to, vector<RoadPoint> & path)
  {
    m_graph->GetSingleFeaturePath(from, to, path);
  }

  void GetFeatureConnectionPath(RoadPoint const & from, RoadPoint const & to, uint32_t featureId,
                                vector<RoadPoint> & path)
  {
    m_graph->GetFeatureConnectionPath(GetJointId(from), GetJointId(to), featureId, path);
  }

  void GetConnectionPaths(RoadPoint const & from, RoadPoint const & to,
                          vector<vector<RoadPoint>> & path)
  {
    m_graph->GetConnectionPaths(GetJointId(from), GetJointId(to), path);
  }

  void CreateFakeFeatureGeometry(vector<RoadPoint> const & geometrySource, double speed,
                                 RoadGeometry & geometry)
  {
    m_graph->CreateFakeFeatureGeometry(geometrySource, speed, geometry);
  }

  vector<DirectedEdge> const & GetFakeZeroLengthEdges() const
  {
    return m_starter->m_fakeZeroLengthEdges;
  }

  void FindPointsWithCommonFeature(Joint::Id jointId0, Joint::Id jointId1, RoadPoint & result0,
                                   RoadPoint & result1)
  {
    return m_starter->FindPointsWithCommonFeature(jointId0, jointId1, result0, result1);
  }

  unique_ptr<IndexGraph> m_graph;
  unique_ptr<IndexGraphStarter> m_starter;
};

class TestGeometryLoader final : public routing::GeometryLoader
{
public:
  // GeometryLoader overrides:
  void Load(uint32_t featureId, routing::RoadGeometry & road) const override;

  void AddRoad(uint32_t featureId, bool oneWay, float speed,
               routing::RoadGeometry::Points const & points);

private:
  unordered_map<uint32_t, routing::RoadGeometry> m_roads;
};

routing::Joint MakeJoint(vector<routing::RoadPoint> const & points);

shared_ptr<routing::EdgeEstimator> CreateEstimator(traffic::TrafficCache const & trafficCache);

routing::AStarAlgorithm<routing::IndexGraphStarter>::Result CalculateRoute(
    routing::IndexGraphStarter & graph, vector<routing::RoadPoint> & roadPoints,
    double & timeSec);

void TestRouteSegments(
    routing::IndexGraphStarter & starter,
    routing::AStarAlgorithm<routing::IndexGraphStarter>::Result expectedRouteResult,
    vector<routing::RoadPoint> const & expectedRoute);

void TestRouteGeometry(
    routing::IndexGraphStarter & starter,
    routing::AStarAlgorithm<routing::IndexGraphStarter>::Result expectedRouteResult,
    vector<m2::PointD> const & expectedRouteGeom);

void TestRouteTime(IndexGraphStarter & starter,
                   AStarAlgorithm<IndexGraphStarter>::Result expectedRouteResult,
                   double expectedTime);

/// \brief Applies all possible permulations of |restrictions| to graph in |restrictionTest| and
/// tests resulting routes.
/// \note restrictionTest should have valid |restrictionTest.m_graph|.
void TestRestrictionPermutations(RestrictionVec restrictions,
                                 vector<m2::PointD> const & expectedRouteGeom,
                                 RoadPoint const & start, RoadPoint const & finish,
                                 RestrictionTest & restrictionTest);
}  // namespace routing_test
