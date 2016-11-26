#pragma once

#include "routing/edge_estimator.hpp"
#include "routing/geometry.hpp"
#include "routing/joint.hpp"
#include "routing/joint_index.hpp"
#include "routing/road_index.hpp"
#include "routing/road_point.hpp"
#include "routing/routing_serialization.hpp"

#include "geometry/point2d.hpp"

#include "std/cstdint.hpp"
#include "std/set.hpp"
#include "std/shared_ptr.hpp"
#include "std/unique_ptr.hpp"
#include "std/utility.hpp"
#include "std/vector.hpp"

namespace routing
{
class JointEdge final
{
public:
  JointEdge(Joint::Id target, double weight) : m_target(target), m_weight(weight) {}
  Joint::Id GetTarget() const { return m_target; }
  double GetWeight() const { return m_weight; }

private:
  // Target is vertex going to for outgoing edges, vertex going from for ingoing edges.
  Joint::Id m_target;
  double m_weight;
};

class JointEdgeGeom final
{
public:
  JointEdgeGeom() = default;
  JointEdgeGeom(Joint::Id target, vector<RoadPoint> const & path) : m_target(target), m_path(path)
  {
  }
  Joint::Id GetTarget() const { return m_target; }
  vector<RoadPoint> const & GetPath() const { return m_path; }

private:
  // Target is vertex going to for outgoing edges, vertex going from for ingoing edges.
  Joint::Id m_target = Joint::kInvalidId;
  vector<RoadPoint> m_path;
};

class DirectedEdge final
{
public:
  DirectedEdge() = default;
  DirectedEdge(Joint::Id from, Joint::Id to) : m_from(from), m_to(to) {}

  bool operator<(DirectedEdge const & rh) const
  {
    if (m_from == rh.m_from)
      return m_to < rh.m_to;
    return m_from < rh.m_from;
  }

  bool operator==(DirectedEdge const & rh) const
  {
    return m_from == rh.m_from && m_to == rh.m_to;
  }

  Joint::Id m_from = Joint::kInvalidId;
  Joint::Id m_to = Joint::kInvalidId;
};

class IndexGraph final
{
public:
  IndexGraph() = default;
  explicit IndexGraph(unique_ptr<GeometryLoader> loader, shared_ptr<EdgeEstimator> estimator);

  // Creates edge for points in same feature.
  void GetDirectedEdge(uint32_t featureId, uint32_t pointFrom, uint32_t pointTo, Joint::Id target,
                       bool forward, vector<JointEdge> & edges);
  void GetNeighboringEdges(RoadPoint const & rp, bool isOutgoing, vector<JointEdge> & edges);

  // Put outgoing (or ingoing) egdes for jointId to the 'edges' vector.
  void GetEdgeList(Joint::Id jointId, bool isOutgoing, vector<JointEdge> & edges);
  Joint::Id GetJointId(RoadPoint const & rp) const { return m_roadIndex.GetJointId(rp); }
  m2::PointD const & GetPoint(Joint::Id jointId);

  Geometry & GetGeometry() { return m_geometry; }
  EdgeEstimator const & GetEstimator() const { return *m_estimator; }
  RoadJointIds const & GetRoad(uint32_t featureId) const { return m_roadIndex.GetRoad(featureId); }

  uint32_t GetNumRoads() const { return m_roadIndex.GetSize(); }
  uint32_t GetNumJoints() const { return m_jointIndex.GetNumJoints(); }
  uint32_t GetNumPoints() const { return m_jointIndex.GetNumPoints(); }

  void Build(uint32_t numJoints);
  m2::PointD const & GetPoint(RoadPoint const & ftp);

  void Import(vector<Joint> const & joints);
  Joint::Id InsertJoint(RoadPoint const & rp);
  bool JointLiesOnRoad(Joint::Id jointId, uint32_t featureId) const;

  void PushFromSerializer(Joint::Id jointId, RoadPoint const & rp)
  {
    m_roadIndex.PushFromSerializer(jointId, rp);
  }

  template <typename F>
  void ForEachRoad(F && f) const
  {
    m_roadIndex.ForEachRoad(forward<F>(f));
  }

  template <typename F>
  void ForEachPoint(Joint::Id jointId, F && f) const
  {
    m_jointIndex.ForEachPoint(jointId, forward<F>(f));
  }

  Joint::Id GetJointIdForTesting(RoadPoint const & ftp) const
  {
    return m_roadIndex.GetJointId(ftp);
  }

  /// \brief Disable all edges between |from| and |to| if they are different and adjacent.
  /// \note Despit the fact that |from| and |to| could be connected with several edges
  /// it's a rare case. In most cases |from| and |to| are connected with only one edge
  /// if they are adjacent.
  /// \note The method doesn't affect routing if |from| and |to| are not adjacent or
  /// if one of them is equal to Joint::kInvalidId.
  void DisableEdge(Joint::Id from, Joint::Id to)
  {
    m_blockedEdges.insert(DirectedEdge(from, to));
  }
  /// \brief Adding a fake oneway feature with a loose end starting from joint |from|.
  /// Geometry for the feature points is taken from |geometrySource|.
  /// If |geometrySource| contains more than two points the feature is created
  /// with intermediate (not joint) point(s).
  /// \returns feature id which was added.
  uint32_t AddFakeLooseEndFeature(Joint::Id from, vector<RoadPoint> const & geometrySource);

  /// \brief Connects joint |from| and |to| with a fake oneway feature. Geometry for the feature
  /// points is taken from |geometrySource|. If |geometrySource| contains more than
  /// two points the feature is created with intermediate (not joint) point(s).
  /// \returns feature id which was added.
  uint32_t AddFakeFeature(Joint::Id from, Joint::Id to, vector<RoadPoint> const & geometrySource);

  /// \brief Adds restriction to navigation graph which says that it's prohibited to go from
  /// |restrictionPoint.m_from| to |restrictionPoint.m_to|.
  /// \note |from| and |to| could be only begining or ending feature points and they has to belong
  /// to
  /// the same junction with |jointId|. That means features |from| and |to| has to be adjacent.
  /// \note This method could be called only after |m_roadIndex| have been loaded with the help of
  /// Deserialize()
  /// or Import().
  void ApplyRestrictionNo(CrossingPoint restrictionPoint);

  /// \brief Adds restriction to navigation graph which says that from feature
  /// |restrictionPoint.m_from| it's permitted only
  /// to go to feature |restrictionPoint.m_to|. All other ways starting form
  /// |restrictionPoint.m_form| is prohibited.
  /// \note All notes which are valid for ApplyRestrictionNo() is valid for ApplyRestrictionOnly().
  void ApplyRestrictionOnly(CrossingPoint restrictionPoint);

  /// \brief Add restrictions in |restrictions| to |m_ftPointIndex|.
  void ApplyRestrictions(RestrictionVec const & restrictions);

  /// \brief Fills |singleFeaturePath| with points from point |from| to point |to|
  /// \note |from| and |to| has to belong the same feature.
  /// \note The order on points in items of |connectionPaths| is from |from| to |to|.
  void GetSingleFeaturePaths(RoadPoint from, RoadPoint to, vector<RoadPoint> & singleFeaturePath);

  /// \brief  Fills |connectionPaths| with all path from joint |from| to joint |to|.
  /// If |from| and |to| don't belong to the same feature |connectionPaths| an exception
  /// |RootException| with be raised.
  /// If |from| and |to| belong to only one feature |connectionPaths| will have one item.
  /// It's most common case.
  /// If |from| and |to| could be connected by several feature |connectionPaths|
  /// will have several items.
  /// \note The order on points in items of |connectionPaths| is from |from| to |to|.
  void GetConnectionPaths(Joint::Id from, Joint::Id to,
                          vector<vector<RoadPoint>> & connectionPaths);

  /// \brief Fills |shortestConnectionPath| with shortest path from joint |from| to joint |to|.
  /// \note The order on points in |shortestConnectionPath| is from |from| to |to|.
  /// \note In most cases the method doesn't load geometry to find the shortest path
  /// because there's only one path between |from| and |to|.
  /// \note if |shortestConnectionPath| could be filled after call of the method
  /// an exception of type |RootException| is raised. It could happend,
  /// for example, if |from| and |to| are connected with a pedestrian road
  /// but a car route is looked for.
  void GetShortestConnectionPath(Joint::Id from, Joint::Id to,
                                 vector<RoadPoint> & shortestConnectionPath);

  /// \brief Fills |featureConnectionPath| with a path from |from| to |to| of points of |featureId|.
  void GetFeatureConnectionPath(Joint::Id from, Joint::Id to, uint32_t featureId,
                                vector<RoadPoint> & featureConnectionPath);

  void GetOutgoingGeomEdges(vector<JointEdge> const & outgoingEdges, Joint::Id center,
                            vector<JointEdgeGeom> & outgoingGeomEdges);

  void CreateFakeFeatureGeometry(vector<RoadPoint> const & geometrySource, RoadGeometry & geometry);

  /// \returns RoadGeometry by a real or fake featureId.
  RoadGeometry const & GetRoad(uint32_t featureId);

  bool IsFakeFeature(uint32_t featureId) const { return featureId >= kStartFakeFeatureIds; }

  CrossingPoint LookUpMovedCrossing(CrossingPoint const & crossing);

  static uint32_t const kStartFakeFeatureIds = 1024 * 1024 * 1024;

private:
  void GetNeighboringEdge(RoadGeometry const & road, RoadPoint const & rp, bool forward,
                          bool outgoing, vector<JointEdge> & edges) const;

  double GetSpeed(RoadPoint ftp);

  /// \brief Finds neghboring of |centerId| joint on feature id of |center|
  /// which is contained in |edges| and fills |oneStepAside| with it.
  /// \note If oneStepAside is empty no neghboring nodes were found.
  /// \note Taking into account the way of setting restrictions almost always |oneStepAside|
  /// will contain one or zero items. Besides the it it's posible to draw map
  /// the (wrong) way |oneStepAside| will contain any number of items.
  void FindOneStepAsideRoadPoint(RoadPoint const & center, Joint::Id centerId,
                                 vector<JointEdge> const & edges,
                                 vector<Joint::Id> & oneStepAside) const;

  bool ApplyRestrictionPrepareData(CrossingPoint const & restrictionPoint,
                                   vector<JointEdge> & ingoingEdges,
                                   vector<JointEdge> & outgoingEdges,
                                   Joint::Id & fromFirstOneStepAside,
                                   Joint::Id & toFirstOneStepAside);

  Geometry m_geometry;
  shared_ptr<EdgeEstimator> m_estimator;
  RoadIndex m_roadIndex;
  JointIndex m_jointIndex;

  set<DirectedEdge> m_blockedEdges;
  uint32_t m_nextFakeFeatureId = kStartFakeFeatureIds;
  // Mapping from fake feature id to fake feature geometry.
  map<uint32_t, RoadGeometry> m_fakeFeatureGeometry;
  // Adding restrictions leads to disabling some nodes and edges and adding others.
  // According to graph trasformation implemented in ApplyRestrictionNo() and
  // ApplyRestrictionOnly() a sequence (edge, node, edge) could:
  // * disappears at all
  // * be transformed to other sequence (edge1, node1, edge1). If so the mapping
  //   is kept in |m_movedCrossings|.
  map<CrossingPoint, CrossingPoint> m_movedCrossings;
};
}  // namespace routing
