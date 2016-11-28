#include "routing/index_graph.hpp"

#include "routing/routing_exceptions.hpp"

#include "base/assert.hpp"
#include "base/exception.hpp"
#include "base/stl_helpers.hpp"

#include "std/limits.hpp"

namespace routing
{
IndexGraph::IndexGraph(unique_ptr<GeometryLoader> loader, shared_ptr<EdgeEstimator> estimator)
  : m_geometry(move(loader)), m_estimator(move(estimator))
{
  ASSERT(m_estimator, ());
}

void IndexGraph::GetEdgeList(Joint::Id jointId, bool isOutgoing, bool graphWithoutRestrictions,
                             vector<JointEdge> & edges)
{
  m_jointIndex.ForEachPoint(jointId, [&](RoadPoint const & rp) {
    GetNeighboringEdges(rp, isOutgoing, graphWithoutRestrictions, edges);
  });
}

m2::PointD const & IndexGraph::GetPoint(RoadPoint const & rp)
{
  RoadGeometry const & road = GetRoad(rp.GetFeatureId());
  CHECK_LESS(rp.GetPointId(), road.GetPointsCount(), ());
  return road.GetPoint(rp.GetPointId());
}

m2::PointD const & IndexGraph::GetPoint(Joint::Id jointId)
{
  return GetPoint(m_jointIndex.GetPoint(jointId));
}

double IndexGraph::GetSpeed(RoadPoint const & rp) { return GetRoad(rp.GetFeatureId()).GetSpeed(); }

void IndexGraph::Build(uint32_t numJoints) { m_jointIndex.Build(m_roadIndex, numJoints); }

void IndexGraph::Import(vector<Joint> const & joints)
{
  CHECK_LESS_OR_EQUAL(joints.size(), numeric_limits<uint32_t>::max(), ());
  m_roadIndex.Import(joints);
  Build(static_cast<uint32_t>(joints.size()));
}

void IndexGraph::GetSingleFeaturePath(RoadPoint from, RoadPoint to,
                                      vector<RoadPoint> & singleFeaturePath)
{
  CHECK_EQUAL(from.GetFeatureId(), to.GetFeatureId(), ());

  singleFeaturePath.clear();
  int const shift = to.GetPointId() > from.GetPointId() ? 1 : -1;
  for (uint32_t i = from.GetPointId(); i != to.GetPointId(); i += shift)
    singleFeaturePath.emplace_back(from.GetFeatureId(), i);
  singleFeaturePath.push_back(to);
}

void IndexGraph::GetConnectionPaths(Joint::Id from, Joint::Id to,
                                    vector<vector<RoadPoint>> & connectionPaths)
{
  CHECK_NOT_EQUAL(from, Joint::kInvalidId, ());
  CHECK_NOT_EQUAL(to, Joint::kInvalidId, ());

  connectionPaths.clear();
  vector<pair<RoadPoint, RoadPoint>> connections;
  m_jointIndex.FindPointsWithCommonFeature(from, to, connections);
  if (connections.empty())
    return;

  connectionPaths.reserve(connections.size());
  for (auto const & c : connections)
  {
    connectionPaths.emplace_back();
    GetSingleFeaturePath(c.first /* from */, c.second /* to */, connectionPaths.back());
  }
}

void IndexGraph::GetShortestConnectionPath(Joint::Id from, Joint::Id to, vector<RoadPoint> & path)
{
  double constexpr kInfinity = numeric_limits<double>::max();
  vector<pair<RoadPoint, RoadPoint>> connections;
  m_jointIndex.FindPointsWithCommonFeature(from, to, connections);
  path.clear();
  if (connections.empty())
    return;

  // Note. The case when size of connections is zero is the most common case. If not,
  // it's necessary to perform a expensive calls below.
  if (connections.size() == 1)
  {
    GetSingleFeaturePath(connections[0].first /* from */, connections[0].second /* to */,
                         path);
    return;
  }

  double minWeight = kInfinity;
  pair<RoadPoint, RoadPoint> minWeightConnection;
  for (auto const & c : connections)
  {
    CHECK_EQUAL(c.first.GetFeatureId(), c.second.GetFeatureId(), ());
    RoadGeometry const & geom = GetRoad(c.first.GetFeatureId());

    double weight = m_estimator->CalcEdgesWeight(c.first.GetFeatureId(),
                                                 geom, c.first.GetPointId() /* from */,
                                                 c.second.GetPointId() /* to */);
    if (weight < minWeight)
    {
      minWeight = weight;
      minWeightConnection = c;
    }
  }

  if (minWeight == kInfinity)
  {
    MYTHROW(RoutingException, ("Joint from:", from, "and joint to:", to,
                               "are not connected a feature necessary type."));
  }

  GetSingleFeaturePath(minWeightConnection.first /* from */, minWeightConnection.second /* to */,
                       path);
}

void IndexGraph::GetFeatureConnectionPath(Joint::Id from, Joint::Id to, uint32_t featureId,
                                          vector<RoadPoint> & path)
{
  path.clear();
  vector<pair<RoadPoint, RoadPoint>> connections;
  m_jointIndex.FindPointsWithCommonFeature(from, to, connections);
  if (connections.empty())
    return;

  for (auto & c : connections)
  {
    CHECK_EQUAL(c.first.GetFeatureId(), c.second.GetFeatureId(), ());
    if (c.first.GetFeatureId() == featureId)
    {
      GetSingleFeaturePath(c.first /* from */, c.second /* to */, path);
      return;
    }
  }
}

void IndexGraph::GetOutgoingGeomEdges(vector<JointEdge> const & outgoingEdges, Joint::Id center,
                                      vector<JointEdgeGeom> & outgoingGeomEdges)
{
  for (JointEdge const & e : outgoingEdges)
  {
    vector<vector<RoadPoint>> connectionPaths;
    GetConnectionPaths(center, e.GetTarget(), connectionPaths);
    if (connectionPaths.empty())
      MYTHROW(RoutingException, ("Can't find common feature for joints", center, e.GetTarget()));

    for (auto const & path : connectionPaths)
    {
      CHECK(!path.empty(), ());
      outgoingGeomEdges.emplace_back(e.GetTarget(), path);
    }
  }
}

void IndexGraph::CreateFakeFeatureGeometry(vector<RoadPoint> const & geometrySource,
                                           RoadGeometry & geometry)
{
  double averageSpeed = 0.0;
  RoadGeometry::Points points(geometrySource.size());
  for (size_t i = 0; i < geometrySource.size(); ++i)
  {
    RoadPoint const rp = geometrySource[i];
    averageSpeed += GetSpeed(rp) / geometrySource.size();
    points[i] = GetPoint(rp);
  }

  geometry = RoadGeometry(true /* oneWay */, averageSpeed, points);
}

uint32_t IndexGraph::AddFakeLooseEndFeature(Joint::Id from,
                                            vector<RoadPoint> const & geometrySource)
{
  CHECK_LESS(from, m_jointIndex.GetNumJoints(), ());
  CHECK_GREATER(geometrySource.size(), 1, ());

  // Getting fake feature geometry.
  RoadGeometry geom;
  CreateFakeFeatureGeometry(geometrySource, geom);
  m_fakeFeatureGeometry.insert(make_pair(m_nextFakeFeatureId, geom));

  RoadPoint const fromFakeFtPoint(m_nextFakeFeatureId, 0 /* point id */);
  m_roadIndex.AddJoint(fromFakeFtPoint, from);
  m_jointIndex.AppendToJoint(from, fromFakeFtPoint);

  return m_nextFakeFeatureId++;
}

uint32_t IndexGraph::AddFakeFeature(Joint::Id from, Joint::Id to,
                                    vector<RoadPoint> const & geometrySource)
{
  CHECK_LESS(from, m_jointIndex.GetNumJoints(), ());
  CHECK_LESS(to, m_jointIndex.GetNumJoints(), ());
  CHECK_GREATER(geometrySource.size(), 1, ());

  uint32_t fakeFeatureId = AddFakeLooseEndFeature(from, geometrySource);
  RoadPoint const toFakeFtPoint(fakeFeatureId, geometrySource.size() - 1 /* point id */);
  m_roadIndex.AddJoint(toFakeFtPoint, to);
  m_jointIndex.AppendToJoint(to, toFakeFtPoint);

  return fakeFeatureId;
}

void IndexGraph::FindOneStepAsideRoadPoint(RoadPoint const & center, Joint::Id centerId,
                                           vector<JointEdge> const & edges,
                                           vector<Joint::Id> & oneStepAside) const
{
  oneStepAside.clear();
  m_roadIndex.ForEachJoint(center.GetFeatureId(), [&](uint32_t /* pointId */, Joint::Id jointId) {
    for (JointEdge const & e : edges)
    {
      if (e.GetTarget() == jointId)
        oneStepAside.push_back(jointId);
    }
  });
}

bool IndexGraph::ApplyRestrictionPrepareData(RestrictionPoint const & restrictionPoint,
                                             vector<JointEdge> & ingoingEdges,
                                             vector<JointEdge> & outgoingEdges,
                                             Joint::Id & fromFirstOneStepAside,
                                             Joint::Id & toFirstOneStepAside)
{
  GetEdgeList(restrictionPoint.m_centerId, false /* isOutgoing */, false /* graphWithoutRestrictions */, ingoingEdges);
  vector<Joint::Id> fromOneStepAside;
  FindOneStepAsideRoadPoint(restrictionPoint.m_from, restrictionPoint.m_centerId, ingoingEdges,
                            fromOneStepAside);
  if (fromOneStepAside.empty())
    return false;

  GetEdgeList(restrictionPoint.m_centerId, true /* isOutgoing */, false /* graphWithoutRestrictions */, outgoingEdges);
  vector<Joint::Id> toOneStepAside;
  FindOneStepAsideRoadPoint(restrictionPoint.m_to, restrictionPoint.m_centerId, outgoingEdges,
                            toOneStepAside);
  if (toOneStepAside.empty())
    return false;

  fromFirstOneStepAside = fromOneStepAside.back();
  toFirstOneStepAside = toOneStepAside.back();
  return true;
}

void IndexGraph::ApplyRestrictionNoRealFeatures(RestrictionPoint const & restrictionPoint)
{
  // @TODO(bykoianko) It's necessary to check all leaves in forest |m_edgeMapping|
  // for staring and ending edges of the |restrictionPoint| and call ApplyRestrictionNo()
  // for every combinations of them.
  //
  // To find fake edges(features) which correspond the real features in |restrictionPoint|
  // GetEdgeList(..., true /* graphWithoutRestrictions */, ...) in
  // ApplyRestrictionPrepareData() will be used. It let us to find |fromFirstOneStepAside|
  // and |toFirstOneStepAside| on real (source) graph. And then to understand which
  // edges on current (partly transformed) graph corresponds to edges
  // |toFirstOneStepAside|-restrictionPoint.m_centerId
  // and restrictionPoint.m_centerId-|fromFirstOneStepAside|.
  ApplyRestrictionNo(restrictionPoint);
}

void IndexGraph::ApplyRestrictionNo(RestrictionPoint const & restrictionPoint)
{
  Joint::Id centerId = restrictionPoint.m_centerId;

  vector<JointEdge> ingoingEdges;
  vector<JointEdge> outgoingEdges;
  Joint::Id fromFirstOneStepAside = Joint::kInvalidId;
  Joint::Id toFirstOneStepAside = Joint::kInvalidId;
  if (!ApplyRestrictionPrepareData(restrictionPoint, ingoingEdges, outgoingEdges,
                                   fromFirstOneStepAside, toFirstOneStepAside))
  {
    return;
  }

  ASSERT_EQUAL(m_blockedEdges.count(DirectedEdge(fromFirstOneStepAside, centerId)), 0, ());
  ASSERT_EQUAL(m_blockedEdges.count(DirectedEdge(centerId, toFirstOneStepAside)), 0, ());

  // One ingoing edge case.
  if (ingoingEdges.size() == 1)
  {
    DisableEdge(centerId, toFirstOneStepAside);
    return;
  }

  // One outgoing edge case.
  if (outgoingEdges.size() == 1)
  {
    DisableEdge(fromFirstOneStepAside, centerId);
    return;
  }

  // Prohibition of moving from one segment to another in case of any number of ingoing and outgoing
  // edges.
  // The idea is to transform the navigation graph for every non-degenerate case as it's shown below.
  // At the picture below a restriction for prohibition moving from 4 to O to 3 is shown.
  // So to implement it it's necessary to remove (disable) an edge 4-O and add features (edges)
  // 4-N-1 and N-2.
  //
  // 1       2       3                     1       2       3
  // *       *       *                     *       *       *
  //  ↖     ^     ↗                       ^↖   ↗^     ↗
  //    ↖   |   ↗                         |  ↖   |   ↗
  //      ↖ | ↗                           |↗   ↖| ↗
  //         *  O             ==>        N *       *  O
  //      ↗ ^ ↖                           ^       ^ ↖
  //    ↗   |   ↖                         |       |   ↖
  //  ↗     |     ↖                       |       |     ↖
  // *       *       *                     *       *       *
  // 4       5       6                     4       5       6
  //
  // In case of this transformation the following edge mapping happens:
  // 4-O -> 4-N
  // O-1 -> O-1; N-1
  // O-2 -> 0-2; N-2
  //
  // @TODO(bykoianko) It's necessary to add item to |m_edgeMapping| according mapping
  // written above.
  outgoingEdges.erase(
      remove_if(outgoingEdges.begin(), outgoingEdges.end(),
                [&](JointEdge const & e) {
                  // Removing edge N->3 in example above.
                  return e.GetTarget() == toFirstOneStepAside
                         // Preveting form adding in loop below
                         // cycles |fromFirstOneStepAside|->|centerId|->|fromFirstOneStepAside|.
                         // @TODO(bykoianko) e.GetTarget() == fromFirstOneStepAside should be
                         // process correctly.
                         // It's a common case of U-turn prohibition.
                         || e.GetTarget() == fromFirstOneStepAside
                         // Removing edges |centerId|->|centerId|.
                         || e.GetTarget() == centerId;
                }),
      outgoingEdges.end());

  my::SortUnique(outgoingEdges, my::LessBy(&JointEdge::GetTarget),
                 my::EqualsBy(&JointEdge::GetTarget));
  // Node. |centerId| could be connected with any outgoing joint with more than one edge (feature).
  // In GetOutgoingGeomEdges() below is taken into account the case.
  vector<JointEdgeGeom> outgoingGeomEdges;
  GetOutgoingGeomEdges(outgoingEdges, centerId, outgoingGeomEdges);

  vector<RoadPoint> ingoingPath;
  GetFeatureConnectionPath(fromFirstOneStepAside, centerId, restrictionPoint.m_from.GetFeatureId(),
                           ingoingPath);
  if (ingoingPath.empty())
    return;

  JointEdgeGeom ingoingEdge(fromFirstOneStepAside, ingoingPath);
  CHECK(!ingoingEdge.GetPath().empty(), ());

  Joint::Id newJoint = Joint::kInvalidId;
  uint32_t ingoingFeatureId = 0;
  uint32_t outgoingFeatureId = 0;
  for (auto it = outgoingGeomEdges.cbegin(); it != outgoingGeomEdges.cend(); ++it)
  {
    // Adding features 4->N and N->1 in example above.
    if (it == outgoingGeomEdges.cbegin())
    {
      if (fromFirstOneStepAside == centerId || centerId == it->GetTarget())
      {
        // @TODO(bykoianko) In rare cases it's posible that outgoing edges staring from |centerId|
        // contain as targets |centerId|. The same thing with ingoing edges.
        // The most likely it's a consequence of adding
        // restrictions with type no for some bidirectional roads. It's necessary to
        // investigate this case, to undestand the reasons of appearing such edges clearly,
        // prevent appearing of such edges and write unit tests on it.
        return;
      }

      // Ingoing edge.
      ingoingFeatureId = AddFakeLooseEndFeature(fromFirstOneStepAside, ingoingEdge.GetPath());
      newJoint =
          InsertJoint({ingoingFeatureId, static_cast<uint32_t>(ingoingEdge.GetPath().size() - 1)});
      // Outgoing edge.
      outgoingFeatureId = AddFakeFeature(newJoint, it->GetTarget(), it->GetPath());
    }
    else
    {
      // Adding the edge N->2 in example above.
      outgoingFeatureId = AddFakeFeature(newJoint, it->GetTarget(), it->GetPath());
    }
  }

  DisableEdge(fromFirstOneStepAside, centerId);
}

void IndexGraph::ApplyRestrictionOnlyRealFeatures(RestrictionPoint const & restrictionPoint)
{
  // @TODO(bykoianko) It's necessary to check all leaves in forest |m_edgeMapping|
  // for staring and ending edges of the |restrictionPoint| and call ApplyRestrictionOnly()
  // for every combinations of them.
  //
  // To find fake edges(features) which correspond the real features in |restrictionPoint|
  // GetEdgeList(..., true /* graphWithoutRestrictions */, ...) in
  // ApplyRestrictionPrepareData() will be used. It let us to find |fromFirstOneStepAside|
  // and |toFirstOneStepAside| on real (source) graph. And then to understand which
  // edges on current (partly transformed) graph corresponds to edges
  // |toFirstOneStepAside|-restrictionPoint.m_centerId
  // and restrictionPoint.m_centerId-|fromFirstOneStepAside|.

  ApplyRestrictionOnly(restrictionPoint);
}

void IndexGraph::ApplyRestrictionOnly(RestrictionPoint const & restrictionPoint)
{
  Joint::Id centerId = restrictionPoint.m_centerId;

  vector<JointEdge> ingoingEdges;
  vector<JointEdge> outgoingEdges;
  Joint::Id fromFirstOneStepAside = Joint::kInvalidId;
  Joint::Id toFirstOneStepAside = Joint::kInvalidId;
  if (!ApplyRestrictionPrepareData(restrictionPoint, ingoingEdges, outgoingEdges,
                                   fromFirstOneStepAside, toFirstOneStepAside))
  {
    return;
  }

  // One outgoing edge case.
  if (outgoingEdges.size() == 1)
  {
    for (auto const & e : ingoingEdges)
    {
      if (e.GetTarget() != fromFirstOneStepAside)
        DisableEdge(e.GetTarget(), centerId);
    }
    return;
  }

  // One ingoing edge case.
  if (ingoingEdges.size() == 1)
  {
    for (auto const & e : outgoingEdges)
    {
      if (e.GetTarget() != toFirstOneStepAside)
        DisableEdge(centerId, e.GetTarget());
    }
    return;
  }

  // It's possible to move only from one segment to another in case of any number of ingoing and
  // outgoing edges.
  // The idea is to tranform the navigation graph for every non-degenerate case as it's shown below.
  // At the picture below a restriction for permission moving only from 6 to O to 3 is shown.
  // So to implement it it's necessary to remove (disable) an edge 6-O and add feature (edge) 4-N-3.
  // Adding N is important for a route recovery stage. (The geometry of O will be copied to N.)
  //
  // 1       2       3                     1       2       3
  // *       *       *                     *       *       *
  //  ↖     ^     ↗                        ↖     ^     ↗^
  //    ↖   |   ↗                            ↖   |   ↗  |
  //      ↖ | ↗                                ↖ | ↗    |
  //         *  O             ==>                  *  O    * N
  //      ↗ ^ ↖                                 ↗^       ^
  //    ↗   |   ↖                             ↗  |       |
  //  ↗     |     ↖                         ↗    |       |
  // *       *       *                     *       *       *
  // 4       5       6                     4       5       6
  //
  // In case of this transformation the following edge mapping happens:
  // 6-O -> 6-N
  // O-3 -> O-3; N-3
  //
  // @TODO(bykoianko) It's necessary to add item to |m_edgeMapping| according mapping
  // written above.

  vector<RoadPoint> ingoingPath;
  GetFeatureConnectionPath(fromFirstOneStepAside, centerId, restrictionPoint.m_from.GetFeatureId(),
                           ingoingPath);
  if (ingoingPath.empty())
    return;

  vector<RoadPoint> outgoingPath;
  GetFeatureConnectionPath(centerId, toFirstOneStepAside, restrictionPoint.m_to.GetFeatureId(),
                           outgoingPath);
  if (outgoingPath.empty())
    return;

  CHECK_GREATER(ingoingPath.size(), 0, ());
  vector<RoadPoint> geometrySource(ingoingPath.cbegin(), ingoingPath.cend() - 1);
  geometrySource.insert(geometrySource.end(), outgoingPath.cbegin(), outgoingPath.cend());

  AddFakeFeature(fromFirstOneStepAside, toFirstOneStepAside, geometrySource);
  DisableEdge(fromFirstOneStepAside, centerId);
}

void IndexGraph::ApplyRestrictions(RestrictionVec const & restrictions)
{
  for (Restriction const & restriction : restrictions)
  {
    if (restriction.m_featureIds.size() != 2)
    {
      LOG(LERROR, ("Only two link restriction are supported. It's a",
                   restriction.m_featureIds.size(), "-link restriction."));
      continue;
    }

    RestrictionPoint restrictionPoint;
    if (!m_roadIndex.GetAdjacentFtPoint(restriction.m_featureIds[0], restriction.m_featureIds[1],
                                        restrictionPoint))
    {
      continue;  // Restriction is not contain adjacent features.
    }

    try
    {
      switch (restriction.m_type)
      {
      case Restriction::Type::No: ApplyRestrictionNoRealFeatures(restrictionPoint); continue;
      case Restriction::Type::Only: ApplyRestrictionOnlyRealFeatures(restrictionPoint); continue;
      }
    }
    catch (RootException const & e)
    {
      LOG(LERROR, ("Exception while applying restrictions. Message:", e.Msg()));
    }
  }
}

Joint::Id IndexGraph::InsertJoint(RoadPoint const & rp)
{
  Joint::Id const existId = m_roadIndex.GetJointId(rp);
  if (existId != Joint::kInvalidId)
    return existId;

  Joint::Id const jointId = m_jointIndex.InsertJoint(rp);
  m_roadIndex.AddJoint(rp, jointId);
  return jointId;
}

bool IndexGraph::JointLiesOnRoad(Joint::Id jointId, uint32_t featureId) const
{
  bool result = false;
  m_jointIndex.ForEachPoint(jointId, [&result, featureId](RoadPoint const & rp) {
    if (rp.GetFeatureId() == featureId)
      result = true;
  });

  return result;
}

void IndexGraph::GetNeighboringEdges(RoadPoint const & rp, bool isOutgoing,
                                     bool graphWithoutRestrictions, vector<JointEdge> & edges)
{
  RoadGeometry const & road = GetRoad(rp.GetFeatureId());

  bool const bidirectional = !road.IsOneWay();
  if (!isOutgoing || bidirectional)
    GetNeighboringEdge(road, rp, false /* forward */, isOutgoing, graphWithoutRestrictions, edges);

  if (isOutgoing || bidirectional)
    GetNeighboringEdge(road, rp, true /* forward */, isOutgoing, graphWithoutRestrictions, edges);
}

void IndexGraph::GetNeighboringEdge(RoadGeometry const & road, RoadPoint const & rp, bool forward,
                                    bool outgoing, bool graphWithoutRestrictions, vector<JointEdge> & edges) const
{
  if (graphWithoutRestrictions && IsFakeFeature(rp.GetFeatureId()))
    return;

  pair<Joint::Id, uint32_t> const & neighbor = m_roadIndex.FindNeighbor(rp, forward);
  if (neighbor.first == Joint::kInvalidId)
    return;

  if (!graphWithoutRestrictions)
  {
    Joint::Id const rbJointId = m_roadIndex.GetJointId(rp);
    DirectedEdge const edge = outgoing ? DirectedEdge(rbJointId, neighbor.first)
                                       : DirectedEdge(neighbor.first, rbJointId);
    if (m_blockedEdges.find(edge) != m_blockedEdges.end())
      return;
  }

  double const distance = m_estimator->CalcEdgesWeight(rp.GetFeatureId(), road,
                                                       rp.GetPointId(), neighbor.second);
  edges.push_back({neighbor.first, distance});
}

RoadGeometry const & IndexGraph::GetRoad(uint32_t featureId)
{
  auto const it = m_fakeFeatureGeometry.find(featureId);
  if (it != m_fakeFeatureGeometry.cend())
    return it->second;

  return m_geometry.GetRoad(featureId);
}

void IndexGraph::GetDirectedEdge(uint32_t featureId, uint32_t pointFrom, uint32_t pointTo,
                                 Joint::Id target, bool forward, vector<JointEdge> & edges)
{
  RoadGeometry const & road = GetRoad(featureId);

  if (road.IsOneWay() && forward != (pointFrom < pointTo))
    return;

  double const distance = m_estimator->CalcEdgesWeight(featureId, road, pointFrom, pointTo);
  edges.emplace_back(target, distance);
}
}  // namespace routing
