#pragma once

#include "routing/edge_estimator.hpp"
#include "routing/index_graph.hpp"
#include "routing/index_graph_starter.hpp"
#include "routing/road_point.hpp"

#include "routing/base/astar_algorithm.hpp"

#include "traffic/traffic_info.hpp"

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
  void Init(unique_ptr<IndexGraph> graph)
  {
    m_graph = move(graph);
  }

  void DisableEdge(RoadPoint const & from, RoadPoint const & to, uint32_t featureId)
  {
    m_graph->DisableEdge(DirectedEdge(GetJointIdForTesting(from), GetJointIdForTesting(to),
                                      featureId));
  }

  uint32_t AddFakeFeature(RoadPoint const & from, RoadPoint const & to,
                          vector<RoadPoint> const & geometrySource, double speed)
  {
    return m_graph->AddFakeFeature(GetJointIdForTesting(from), GetJointIdForTesting(to),
                                   geometrySource, speed);
  }

  uint32_t AddFakeFeature(Joint::Id from, Joint::Id to,
                          vector<RoadPoint> const & geometrySource, double speed)
  {
    return m_graph->AddFakeFeature(from, to, geometrySource, speed);
  }

  Joint::Id GetJointIdForTesting(RoadPoint const & rp) const
  {
    return m_graph->GetJointIdForTesting(rp);
  }

  void ApplyRestrictionNoRealFeatures(RestrictionPoint const & restrictionPoint)
  {
    m_graph->ApplyRestrictionNoRealFeatures(restrictionPoint);
  }

  void ApplyRestrictionOnlyRealFeatures(RestrictionPoint const & restrictionPoint)
  {
    m_graph->ApplyRestrictionOnlyRealFeatures(restrictionPoint);
  }

  void GetSingleFeaturePath(RoadPoint const & from, RoadPoint const & to,
                            vector<RoadPoint> & path)
  {
    m_graph->GetSingleFeaturePath(from, to, path);
  }

  void GetFeatureConnectionPath(RoadPoint const & from, RoadPoint const & to,
                                uint32_t featureId, vector<RoadPoint> & path)
  {
    m_graph->GetFeatureConnectionPath(GetJointIdForTesting(from),
                                      GetJointIdForTesting(to), featureId, path);
  }

  void GetConnectionPaths(RoadPoint const & from, RoadPoint const & to,
                          vector<vector<RoadPoint>> & path)
  {
    m_graph->GetConnectionPaths(GetJointIdForTesting(from),
                                GetJointIdForTesting(to), path);
  }

  void CreateFakeFeatureGeometry(vector<RoadPoint> const & geometrySource, double speed,
                                 RoadGeometry & geometry)
  {
    m_graph->CreateFakeFeatureGeometry(geometrySource, speed, geometry);
  }

  unique_ptr<IndexGraph> m_graph;
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
    routing::IndexGraphStarter & graph, vector<routing::RoadPoint> & roadPoints);

void TestRouteSegments(
    routing::IndexGraphStarter & starter,
    routing::AStarAlgorithm<routing::IndexGraphStarter>::Result expectedRouteResult,
    vector<routing::RoadPoint> const & expectedRoute);

void TestRouteGeometry(
    routing::IndexGraphStarter & starter,
    routing::AStarAlgorithm<routing::IndexGraphStarter>::Result expectedRouteResult,
    vector<m2::PointD> const & expectedRouteGeom);
}  // namespace routing_test
