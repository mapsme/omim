#pragma once

#include "routing/edge_estimator.hpp"
#include "routing/geometry.hpp"
#include "routing/joint.hpp"
#include "routing/joint_index.hpp"
#include "routing/restrictions_serialization.hpp"
#include "routing/road_index.hpp"
#include "routing/road_point.hpp"

#include "geometry/point2d.hpp"

#include "std/cstdint.hpp"
#include "std/functional.hpp"
#include "std/set.hpp"
#include "std/shared_ptr.hpp"
#include "std/unique_ptr.hpp"
#include "std/utility.hpp"
#include "std/vector.hpp"

namespace routing_test
{
struct RestrictionTest;
}  // namespace routing_test

namespace routing
{
class JointEdge final
{
public:
  JointEdge(Joint::Id target, double weight) : m_target(target), m_weight(weight) {}
  Joint::Id GetTarget() const { return m_target; }
  double GetWeight() const { return m_weight; }

private:
  // Target is a vertex going to for outgoing edges, vertex going from for ingoing edges.
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
  // Target is a vertex going to for outgoing edges, vertex going from for ingoing edges.
  Joint::Id m_target = Joint::kInvalidId;
  vector<RoadPoint> m_path;
};

class DirectedEdge final
{
public:
  DirectedEdge() = default;
  DirectedEdge(Joint::Id from, Joint::Id to, uint32_t featureId)
    : m_from(from), m_to(to), m_featureId(featureId)
  {
  }

  bool operator<(DirectedEdge const & rhs) const;
  bool operator==(DirectedEdge const & rhs) const;

  Joint::Id GetFrom() const { return m_from; }
  Joint::Id GetTo() const { return m_to; }
  uint32_t GetFeatureId() const { return m_featureId; }
  static bool IsAdjacent(DirectedEdge const & ingoing, DirectedEdge const & outgoing)
  {
    return ingoing.GetTo() == outgoing.GetFrom();
  }

private:
  Joint::Id const m_from = Joint::kInvalidId;
  Joint::Id const m_to = Joint::kInvalidId;
  // Note. It's important to store feature id because two |m_from| and |m_to| may be
  // connected with several features.
  uint32_t const m_featureId = 0;
};

string DebugPrint(DirectedEdge const & directedEdge);

class RestrictionInfo final
{
public:
  RestrictionInfo() = default;

  RestrictionInfo(DirectedEdge const & ingoing, DirectedEdge const & outgoing)
    : m_from(ingoing.GetFrom())
    , m_center(ingoing.GetTo())
    , m_to(outgoing.GetTo())
    , m_fromFeatureId(ingoing.GetFeatureId())
    , m_toFeatureId(outgoing.GetFeatureId())
  {
    CHECK(DirectedEdge::IsAdjacent(ingoing, outgoing), ());
  }

  bool operator<(RestrictionInfo const & rhs) const;
  bool operator==(RestrictionInfo const & rhs) const { return !(*this < rhs || rhs < *this); }
  bool operator!=(RestrictionInfo const & rhs) const { return !(*this == rhs); }
  pair<DirectedEdge, DirectedEdge> Edges() const
  {
    return make_pair(DirectedEdge(m_from, m_center, m_fromFeatureId),
                     DirectedEdge(m_center, m_to, m_toFeatureId));
  }

  Joint::Id m_from = Joint::kInvalidId;
  Joint::Id m_center = Joint::kInvalidId;
  Joint::Id m_to = Joint::kInvalidId;
  uint32_t m_fromFeatureId = 0;
  uint32_t m_toFeatureId = 0;
};

class IndexGraph final
{
public:
  static uint32_t const kStartFakeFeatureIds = 1024 * 1024 * 1024;

  IndexGraph() = default;
  explicit IndexGraph(unique_ptr<GeometryLoader> loader, shared_ptr<EdgeEstimator> estimator);

  // Creates edge for points in same feature.
  void GetDirectedEdge(uint32_t featureId, uint32_t pointFrom, uint32_t pointTo, Joint::Id target,
                       bool forward, vector<JointEdge> & edges);
  void GetNeighboringEdges(RoadPoint const & rp, bool isOutgoing, bool graphWithoutRestrictions,
                           vector<JointEdge> & edges);
  /// \brief Fills |edges| with edges going through |rp|. |rp| should not be a joint.
  void GetIntermediatePointEdges(RoadPoint const & rp, bool graphWithoutRestrictions,
                                 vector<DirectedEdge> & edges);

  // Put outgoing (or ingoing) egdes for jointId to the |edges| vector.
  void GetEdgeList(Joint::Id jointId, bool isOutgoing, bool graphWithoutRestrictions,
                   vector<JointEdge> & edges);
  void GetEdgeList(Joint::Id jointId, bool isOutgoing, bool graphWithoutRestrictions,
                   vector<DirectedEdge> & edges);
  Joint::Id GetJointId(RoadPoint const & rp) const { return m_roadIndex.GetJointId(rp); }
  m2::PointD const & GetPoint(Joint::Id jointId);

  EdgeEstimator const & GetEstimator() const { return *m_estimator; }
  RoadJointIds const & GetRoad(uint32_t featureId) const { return m_roadIndex.GetRoad(featureId); }

  uint32_t GetNumRoads() const { return m_roadIndex.GetSize(); }
  uint32_t GetNumJoints() const { return m_jointIndex.GetNumJoints(); }
  uint32_t GetNumStaticPoints() const { return m_jointIndex.GetNumStaticPoints(); }
  /// \brief Builds |m_jointIndex|.
  /// \param numJoints number of joints.
  /// \note This method should be called when |m_roadIndex| is ready.
  void Build(uint32_t numJoints);

  uint32_t GetNextFakeFeatureId() const { return m_nextFakeFeatureId; }
  m2::PointD const & GetPoint(RoadPoint const & rp);
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

  /// \brief Add restrictions in |restrictions| to |m_ftPointIndex|.
  void ApplyRestrictions(RestrictionVec const & restrictions);

  /// \returns RoadGeometry by a real or fake featureId.
  RoadGeometry const & GetRoad(uint32_t featureId);

  static bool IsFakeFeature(uint32_t featureId) { return featureId >= kStartFakeFeatureIds; }
  /// \brief Calls |f| for |directedEdge| if it's not blocked and recursively for every
  /// non blocked edge in |m_edgeMapping|.
  template <class F>
  void ForEachNonBlockedEdgeMappingNode(DirectedEdge const & directedEdge, F && f) const
  {
    auto const it = m_edgeMapping.find(directedEdge);
    if (it != m_edgeMapping.end())
    {
      for (DirectedEdge const & e : it->second)
        ForEachNonBlockedEdgeMappingNode(e, f);
    }

    if (m_blockedEdges.count(directedEdge) == 0)
      f(directedEdge);
  }

  template <class F>
  void ForEachEdgeMappingNode(DirectedEdge const & directedEdge, F && f) const
  {
    auto const it = m_edgeMapping.find(directedEdge);
    if (it != m_edgeMapping.end())
    {
      for (DirectedEdge const & e : it->second)
        ForEachEdgeMappingNode(e, f);
    }

    f(directedEdge);
  }

private:
  friend struct routing_test::RestrictionTest;

  /// \brief Disables an edge between |from| and |to| along |featureId|.
  /// \note Despite the fact that |from| and |to| could be connected with several edges
  /// it's a rare case. In most cases |from| and |to| are connected with only one edge
  /// if they are adjacent.
  /// \note The method doesn't affect routing if |from| and |to| are not adjacent or
  /// if one of them is equal to Joint::kInvalidId.
  void DisableEdge(DirectedEdge const & edge) { m_blockedEdges.insert(edge); }
  void DisableAllEdges(Joint::Id from, Joint::Id to);

  /// \brief Adds a fake oneway feature with a loose end starting from joint |from|.
  /// Geometry for the feature points is taken from |geometrySource|.
  /// If |geometrySource| contains more than two points the feature is created
  /// with intermediate (not joint) point(s).
  /// \returns feature id which was added.
  uint32_t AddFakeLooseEndFeature(Joint::Id from, vector<RoadPoint> const & geometrySource,
                                  double speed);

  /// \brief Connects joint |from| and |to| with a fake oneway feature. Geometry for the feature
  /// points is taken from |geometrySource|. If |geometrySource| contains more than
  /// two points the feature is created with intermediate (not joint) point(s).
  /// \returns feature id which was added.
  uint32_t AddFakeFeature(Joint::Id from, Joint::Id to, vector<RoadPoint> const & geometrySource,
                          double speed);

  void ApplyRestrictionNo(RestrictionInfo const & restrictionInfo);

  void ApplyRestrictionOnly(RestrictionInfo const & restrictionInfo);

  /// \brief Adds restriction to navigation graph which says that it's prohibited to go from
  /// |restrictionPoint.m_from| to |restrictionPoint.m_to|.
  /// \note |from| and |to| could be only begining or ending feature points and they has to belong
  /// to the same junction with |jointId|. That means features |from| and |to| has to be adjacent.
  /// \note This method could be called only after |m_roadIndex| have been loaded with the help of
  /// Deserialize() or Import().
  void ApplyRestrictionNoRealFeatures(RestrictionPoint const & restrictionPoint);

  /// \brief Adds restriction to navigation graph which says that from feature
  /// |restrictionPoint.m_from| it's permitted only
  /// to go to feature |restrictionPoint.m_to|. All other ways starting form
  /// |restrictionPoint.m_form| is prohibited.
  /// \note All notes which are valid for ApplyRestrictionNoRealFeatures()
  /// are valid for ApplyRestrictionOnlyRealFeatures().
  void ApplyRestrictionOnlyRealFeatures(RestrictionPoint const & restrictionPoint);

  /// \brief Fills |path| with points from point |from| to point |to|
  /// \note |from| and |to| should belong to the same feature.
  /// \note The order on points in items of |connectionPaths| is from |from| to |to|.
  void GetSingleFeaturePath(RoadPoint const & from, RoadPoint const & to, vector<RoadPoint> & path);

  /// \brief Fills |path| with a path from |from| to |to| of points of |featureId|.
  void GetFeatureConnectionPath(Joint::Id from, Joint::Id to, uint32_t featureId,
                                vector<RoadPoint> & path);

  void GetOutgoingGeomEdges(vector<JointEdge> const & outgoingEdges, Joint::Id center,
                            vector<JointEdgeGeom> & outgoingGeomEdges);

  /// \brief Fills |connectionPaths| with all path from joint |from| to joint |to|.
  /// If |from| and |to| don't belong to the same feature |connectionPaths| will
  /// be empty.
  /// If |from| and |to| belong to only one feature |connectionPaths| will have one item.
  /// It's the most common case.
  /// If |from| and |to| could be connected by several features |connectionPaths|
  /// will have several items.
  /// \note The order on points in items of |connectionPaths| is from |from| to |to|.
  void GetConnectionPaths(Joint::Id from, Joint::Id to,
                          vector<vector<RoadPoint>> & connectionPaths);

  void CreateFakeFeatureGeometry(vector<RoadPoint> const & geometrySource, double speed,
                                 RoadGeometry & geometry);

  void GetNeighboringEdge(RoadGeometry const & road, RoadPoint const & rp, bool forward,
                          bool outgoing, bool graphWithoutRestrictions,
                          vector<JointEdge> & edges) const;

  double GetSpeed(RoadPoint const & rp);

  /// \brief Finds all joints of |featureId| which are equal to JointEdge::m_target
  /// of |edges. Fills |oneStepAside| with found joints.
  void FindOneStepAsideRoadPoint(uint32_t featureId, vector<JointEdge> const & edges,
                                 vector<Joint::Id> & oneStepAside) const;

  /// \returns false if it cannot get ingoing or outgoing edges and true otherwise.
  bool GetIngoingAndOutgoingEdges(Joint::Id centerId, bool graphWithoutRestrictions,
                                  vector<JointEdge> & ingoingEdges,
                                  vector<JointEdge> & outgoingEdges);

  bool ApplyRestrictionPrepareData(RestrictionPoint const & restrictionPoint,
                                   RestrictionInfo & restrictionInfo);

  void InsertToEdgeMapping(DirectedEdge const & key, DirectedEdge const & value);

  /// \brief Calls |f| for |restrictionPoint| and for all restriction points formed by
  /// all ingoing and outgoing edges which were generated from the ingoing edge
  /// and the outgoing edge of |restrictionPoint|.
  template<class F>
  void ApplyRestrictionRealFeatures(RestrictionPoint const & restrictionPoint, F && f)
  {
    RestrictionInfo restrictionInfo;
    if (!ApplyRestrictionPrepareData(restrictionPoint, restrictionInfo))
      return;

    // Note. It's necessary to collect edges ingoing to the restriction and
    // outgoing from the restriction at first and then to apply |f|
    // because |f| edits the graph and excecuting |f| affect the behavior of
    // ForEachNonBlockedEdgeMappingNode().
    auto const edges = restrictionInfo.Edges();
    vector<DirectedEdge> ingoingRestEdges;
    ForEachNonBlockedEdgeMappingNode(
        edges.first, [&](DirectedEdge const & ingoing) { ingoingRestEdges.push_back(ingoing); });

    vector<DirectedEdge> outgoingRestEdges;
    ForEachNonBlockedEdgeMappingNode(
        edges.second, [&](DirectedEdge const & outgoing) { outgoingRestEdges.push_back(outgoing); });

    for (DirectedEdge const & ingoing : ingoingRestEdges)
    {
      for (DirectedEdge const & outgoing : outgoingRestEdges)
      {
        if (DirectedEdge::IsAdjacent(ingoing, outgoing))
          f(RestrictionInfo(ingoing, outgoing));
      }
    }
  }

  Geometry m_geometry;
  shared_ptr<EdgeEstimator> m_estimator;
  RoadIndex m_roadIndex;
  JointIndex m_jointIndex;

  set<DirectedEdge> m_blockedEdges;
  uint32_t m_nextFakeFeatureId = kStartFakeFeatureIds;
  // Mapping from fake feature id to fake feature geometry.
  map<uint32_t, RoadGeometry> m_fakeFeatureGeometry;
  // Adding restrictions leads to disabling some edges and adding others.
  // According to graph trasformation implemented in ApplyRestrictionNo() and
  // ApplyRestrictionOnly() every |DirectedEdge| (a pair of joints) could:
  // * disappears at all (in that case it's added to |m_blockedEdges|)
  // * be transformed to another DirectedEdge. If so the mapping
  //   is kept in |m_edgeMapping| and source edge is blocked (added to |m_blockedEdges|).
  // * be copied. If so the mapping is kept in |m_edgeMapping| and the source edge is not blocked.
  // See ApplyRestriction* method for a detailed comments about trasformation rules.
  map<DirectedEdge, vector<DirectedEdge>> m_edgeMapping;
};
}  // namespace routing
