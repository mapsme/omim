#include "index_graph.hpp"

#include "base/assert.hpp"
#include "base/exception.hpp"
#include "base/stl_helpers.hpp"

#include "std/limits.hpp"

namespace routing
{
size_t restrictionNoCounter = 0;
size_t restrictionNoDataNotPreparedCounter = 0;
size_t appliedRestrictionNoCounter = 0;
size_t foundRemovedRestrictionNoCounter = 0;

size_t restrictionOnlyCounter = 0;
size_t restrictionOnlyDataNotPreparedCounter = 0;
size_t appliedRestrictionOnlyCounter = 0;
size_t foundRemovedRestrictionOnlyCounter = 0;

IndexGraph::IndexGraph(unique_ptr<GeometryLoader> loader, shared_ptr<EdgeEstimator> estimator)
  : m_geometry(move(loader)), m_estimator(move(estimator))
{
  ASSERT(m_estimator, ());
}

void IndexGraph::GetEdgeList(Joint::Id jointId, bool isOutgoing, vector<JointEdge> & edges)
{
  m_jointIndex.ForEachPoint(jointId, [this, &edges, isOutgoing](RoadPoint const & rp) {
    GetNeighboringEdges(rp, isOutgoing, edges);
  });
}

m2::PointD const & IndexGraph::GetPoint(RoadPoint const & ftp)
{
  RoadGeometry const & road = GetRoad(ftp.GetFeatureId());
  CHECK_LESS(ftp.GetPointId(), road.GetPointsCount(), ());
  return road.GetPoint(ftp.GetPointId());
}

m2::PointD const & IndexGraph::GetPoint(Joint::Id jointId)
{
  return GetPoint(m_jointIndex.GetPoint(jointId));
}

double IndexGraph::GetSpeed(RoadPoint ftp) { return GetRoad(ftp.GetFeatureId()).GetSpeed(); }
void IndexGraph::Build(uint32_t jointNumber) { m_jointIndex.Build(m_roadIndex, jointNumber); }
void IndexGraph::Import(vector<Joint> const & joints)
{
  CHECK_LESS_OR_EQUAL(joints.size(), numeric_limits<uint32_t>::max(), ());
  m_roadIndex.Import(joints);
  Build(static_cast<uint32_t>(joints.size()));
}

void IndexGraph::GetSingleFeaturePaths(RoadPoint from, RoadPoint to,
                                       vector<RoadPoint> & singleFeaturePath)
{
  CHECK_EQUAL(from.GetFeatureId(), to.GetFeatureId(), ());
  singleFeaturePath.clear();
  if (from == to)
  {
    singleFeaturePath.push_back(from);
    return;
  }

  int shift = to.GetPointId() > from.GetPointId() ? 1 : -1;
  for (int i = from.GetPointId(); i != to.GetPointId(); i += shift)
    singleFeaturePath.emplace_back(from.GetFeatureId(), i);
  singleFeaturePath.emplace_back(from.GetFeatureId(), to.GetPointId());
}

void IndexGraph::GetConnectionPaths(Joint::Id from, Joint::Id to,
                                    vector<vector<RoadPoint>> & connectionPaths)
{
  CHECK_NOT_EQUAL(from, Joint::kInvalidId, ());
  CHECK_NOT_EQUAL(to, Joint::kInvalidId, ());

  vector<pair<RoadPoint, RoadPoint>> connections;
  m_jointIndex.FindPointsWithCommonFeature(from, to, connections);
  CHECK_LESS(0, connections.size(), ());

  connectionPaths.clear();
  for (pair<RoadPoint, RoadPoint> const & c : connections)
  {
    vector<RoadPoint> roadPoints;
    GetSingleFeaturePaths(c.first /* from */, c.second /* to */, roadPoints);
    connectionPaths.push_back(move(roadPoints));
  }
}

void IndexGraph::GetShortestConnectionPath(Joint::Id from, Joint::Id to,
                                           vector<RoadPoint> & shortestConnectionPath)
{
  vector<pair<RoadPoint, RoadPoint>> connections;
  m_jointIndex.FindPointsWithCommonFeature(from, to, connections);
  CHECK_LESS(0, connections.size(), ());

  // Note. The case when size of connections is zero is the most common case. If not,
  // it's necessary to perform a costy calls below.
  if (connections.size() == 1)
  {
    GetSingleFeaturePaths(connections[0].first /* from */, connections[0].second /* to */,
                          shortestConnectionPath);
    return;
  }

  double minWeight = numeric_limits<double>::max();
  pair<RoadPoint, RoadPoint> minWeightConnection;
  for (pair<RoadPoint, RoadPoint> const & c : connections)
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

  if (minWeight == numeric_limits<double>::max())
  {
    MYTHROW(RootException, ("Joint from:", from, "and joint to:", to,
                            "are not connected a feature necessary type."));
  }

  GetSingleFeaturePaths(minWeightConnection.first /* from */, minWeightConnection.second /* to */,
                        shortestConnectionPath);
}

void IndexGraph::GetFeatureConnectionPath(Joint::Id from, Joint::Id to, uint32_t featureId,
                                          vector<RoadPoint> & featureConnectionPath)
{
  vector<pair<RoadPoint, RoadPoint>> connections;
  m_jointIndex.FindPointsWithCommonFeature(from, to, connections);
  CHECK_LESS(0, connections.size(), ());

  for (auto & c : connections)
  {
    CHECK_EQUAL(c.first.GetFeatureId(), c.second.GetFeatureId(), ());
    if (c.first.GetFeatureId() == featureId)
    {
      GetSingleFeaturePaths(c.first /* from */, c.second /* to */, featureConnectionPath);
      return;
    }
  }
  MYTHROW(RootException,
          ("Joint from:", from, "and joint to:", to, "are not connected feature:", featureId));
}

void IndexGraph::GetOutgoingGeomEdges(vector<JointEdge> const & outgoingEdges, Joint::Id center,
                                      vector<JointEdgeGeom> & outgoingGeomEdges)
{
  for (JointEdge const & e : outgoingEdges)
  {
    vector<vector<RoadPoint>> connectionPaths;
    GetConnectionPaths(center, e.GetTarget(), connectionPaths);
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
  buffer_vector<m2::PointD, 32> points(geometrySource.size());
  for (size_t i = 0; i < geometrySource.size(); ++i)
  {
    RoadPoint const ftp = geometrySource[i];
    averageSpeed += GetSpeed(ftp) / geometrySource.size();
    points[i] = GetPoint(ftp);
  }

  geometry = RoadGeometry(true /* oneWay */, averageSpeed, points);
}

uint32_t IndexGraph::AddFakeLooseEndFeature(Joint::Id from,
                                            vector<RoadPoint> const & geometrySource)
{
  CHECK_LESS(from, m_jointIndex.GetNumJoints(), ());
  CHECK_LESS(1, geometrySource.size(), ());

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
  CHECK_LESS(1, geometrySource.size(), ());

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
  m_roadIndex.ForEachJoint(center.GetFeatureId(), [&](uint32_t /*pointId*/, Joint::Id jointId) {
    for (JointEdge const & e : edges)
    {
      if (e.GetTarget() == jointId)
        oneStepAside.push_back(jointId);
    }
  });
}

bool IndexGraph::ApplyRestrictionPrepareData(CrossingPoint const & restrictionPoint,
                                             vector<JointEdge> & ingoingEdges,
                                             vector<JointEdge> & outgoingEdges,
                                             Joint::Id & fromFirstOneStepAside,
                                             Joint::Id & toFirstOneStepAside)
{
  GetEdgeList(restrictionPoint.m_centerId, false /* isOutgoing */, ingoingEdges);
  vector<Joint::Id> fromOneStepAside;
  FindOneStepAsideRoadPoint(restrictionPoint.m_from, restrictionPoint.m_centerId, ingoingEdges,
                            fromOneStepAside);
  if (fromOneStepAside.empty())
    return false;

  GetEdgeList(restrictionPoint.m_centerId, true /* isOutgoing */, outgoingEdges);
  vector<Joint::Id> toOneStepAside;
  FindOneStepAsideRoadPoint(restrictionPoint.m_to, restrictionPoint.m_centerId, outgoingEdges,
                            toOneStepAside);
  if (toOneStepAside.empty())
    return false;

  fromFirstOneStepAside = fromOneStepAside.back();
  toFirstOneStepAside = toOneStepAside.back();
  return true;
}

CrossingPoint IndexGraph::LookUpMovedCrossing(CrossingPoint const & crossing)
{
  CrossingPoint result = crossing;
  map<CrossingPoint, CrossingPoint>::iterator it;
  while (true)
  {
    it = m_movedCrossings.find(result);
    if (it == m_movedCrossings.cend())
      return result;
    result = it->second;
  }
}

void IndexGraph::ApplyRestrictionNo(CrossingPoint restrictionPoint)
{
  restrictionNoCounter += 1;
  CrossingPoint result = LookUpMovedCrossing(restrictionPoint);
  if (result != restrictionPoint)
  {
    restrictionPoint = result;
    foundRemovedRestrictionNoCounter += 1;
  }

  Joint::Id centerId = restrictionPoint.m_centerId;

  vector<JointEdge> ingoingEdges;
  vector<JointEdge> outgoingEdges;
  Joint::Id fromFirstOneStepAside = Joint::kInvalidId;
  Joint::Id toFirstOneStepAside = Joint::kInvalidId;
  if (!ApplyRestrictionPrepareData(restrictionPoint, ingoingEdges, outgoingEdges,
                                   fromFirstOneStepAside, toFirstOneStepAside))
  {
    restrictionNoDataNotPreparedCounter += 1;
    return;
  }

  ASSERT_EQUAL(m_blockedEdges.count(make_pair(fromFirstOneStepAside, centerId)), 0, ());
  ASSERT_EQUAL(m_blockedEdges.count(make_pair(centerId, toFirstOneStepAside)), 0, ());

  // One ingoing edge case.
  if (ingoingEdges.size() == 1)
  {
    appliedRestrictionNoCounter += 1;
    DisableEdge(centerId, toFirstOneStepAside);
    return;
  }

  // One outgoing edge case.
  if (outgoingEdges.size() == 1)
  {
    appliedRestrictionNoCounter += 1;
    DisableEdge(fromFirstOneStepAside, centerId);
    return;
  }

  // Prohibition of moving from one segment to another in case of any number of ingoing and outgoing
  // edges.
  // The idea is to tranform the navigation graph for every non-degenerate case as it's shown below.
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
  // 4       5       7                     4       5       7

  outgoingEdges.erase(
      remove_if(outgoingEdges.begin(), outgoingEdges.end(),
                [&](JointEdge const & e) {
                  // Removing edge N->3 in example above.
                  return e.GetTarget() == toFirstOneStepAside
                         // Preveting form adding in loop below
                         // cycles |fromFirstOneStepAside|->|centerId|->|fromFirstOneStepAside|.
                         // @TODO(bykoianko) e.GetTarget() == fromFirstOneStepAside should be
                         // process correctly.
                         // It's a popular case of U-turn prohibition.
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

    CrossingPoint const removedCrossing(restrictionPoint.m_from, it->GetPath().front(), centerId);
    CrossingPoint const newCrossing(RoadPoint(ingoingFeatureId, ingoingEdge.GetPath().size() - 1),
                                    RoadPoint(outgoingFeatureId, 0), newJoint);
    m_movedCrossings.insert(make_pair(removedCrossing, newCrossing));
    // @TODO(bykoianko) In rare cases |m_movedCrossings| contains the key which is
    // equivalent to the adding key. This issue should be investigated.
  }

  appliedRestrictionNoCounter += 1;
  DisableEdge(fromFirstOneStepAside, centerId);
}

void IndexGraph::ApplyRestrictionOnly(CrossingPoint restrictionPoint)
{
  restrictionOnlyCounter += 1;
  CrossingPoint result = LookUpMovedCrossing(restrictionPoint);
  if (result != restrictionPoint)
  {
    restrictionPoint = result;
    foundRemovedRestrictionOnlyCounter += 1;
  }

  Joint::Id centerId = restrictionPoint.m_centerId;

  vector<JointEdge> ingoingEdges;
  vector<JointEdge> outgoingEdges;
  Joint::Id fromFirstOneStepAside = Joint::kInvalidId;
  Joint::Id toFirstOneStepAside = Joint::kInvalidId;
  if (!ApplyRestrictionPrepareData(restrictionPoint, ingoingEdges, outgoingEdges,
                                   fromFirstOneStepAside, toFirstOneStepAside))
  {
    restrictionOnlyDataNotPreparedCounter += 1;
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
    appliedRestrictionOnlyCounter += 1;
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
    appliedRestrictionOnlyCounter += 1;
    return;
  }

  // It's possible to move only from one segment to another in case of any number of ingoing and
  // outgoing edges.
  // The idea is to tranform the navigation graph for every non-degenerate case as it's shown below.
  // At the picture below a restriction for permission moving only from 7 to O to 3 is shown.
  // So to implement it it's necessary to remove (disable) an edge 7-O and add feature (edge) 4-N-3.
  // Adding N is important for a route recovery stage. (The geometry of O will be copied to N.)
  //
  // 1       2       3                     1       2       3
  // *       *       *                     *       *       *
  //  ↖     ^     ↗                        ↖     ^     ↗^
  //    ↖   |   ↗                            ↖   |   ↗  |
  //      ↖ | ↗                                ↖| ↗     |
  //         *  O             ==>                  *  O    * N
  //      ↗ ^ ↖                                 ↗^       ^
  //    ↗   |   ↖                             ↗  |       |
  //  ↗     |     ↖                         ↗    |       |
  // *       *       *                     *       *       *
  // 4       5       7                     4       5       7

  vector<RoadPoint> ingoingPath;
  GetFeatureConnectionPath(fromFirstOneStepAside, centerId, restrictionPoint.m_from.GetFeatureId(),
                           ingoingPath);
  vector<RoadPoint> outgoingPath;
  GetFeatureConnectionPath(centerId, toFirstOneStepAside, restrictionPoint.m_to.GetFeatureId(),
                           outgoingPath);
  CHECK_LESS(0, ingoingPath.size(), ());
  vector<RoadPoint> geometrySource(ingoingPath.cbegin(), ingoingPath.cend() - 1);
  geometrySource.insert(geometrySource.end(), outgoingPath.cbegin(), outgoingPath.cend());

  AddFakeFeature(fromFirstOneStepAside, toFirstOneStepAside, geometrySource);
  DisableEdge(fromFirstOneStepAside, centerId);
  appliedRestrictionOnlyCounter += 1;
  // Note. |m_movedCrossings| should be added with nothing in that case.
  // Despite the fact instead of crossing (7, O, 3) in examle abobe a crossing
  // (7, N, 3) is generated (7, N, 3) could not be used in further correct restrictions.
  // After a restriction (7, O, 3) is added there are the following variants:
  // * There's a restriction with type only starting from (7, O).
  //   It's either an invalid case (7, O, 2) for example or dublicate (7, 0, 3).
  // * There's a restriction with type no starting from (7, O).
  //   It could be a valid restriction (7, 0, 2) but it's taken into acount in modification
  //   above. Or it could be an invalid case. For example (7, 0, 3).
}

void IndexGraph::ApplyRestrictions(RestrictionVec const & restrictions)
{
  for (Restriction const & restriction : restrictions)
  {
    if (restriction.m_featureIds.size() != 2)
    {
      LOG(LERROR, ("Only to link restriction are supported."));
      continue;
    }

    CrossingPoint restrictionPoint;
    if (!m_roadIndex.GetAdjacentFtPoints(restriction.m_featureIds[0], restriction.m_featureIds[1],
                                         restrictionPoint))
    {
      continue;  // Restriction is not contain adjacent features.
    }

    try
    {
      switch (restriction.m_type)
      {
      case Restriction::Type::No: ApplyRestrictionNo(restrictionPoint); continue;
      case Restriction::Type::Only: ApplyRestrictionOnly(restrictionPoint); continue;
      }
    }
    catch (RootException const & e)
    {
      LOG(LERROR, ("Exception while applying restrictions. Message:", e.Msg()));
    }
  }

  LOG(LINFO, ("restrictionNoCounter:", restrictionNoCounter, "appliedRestrictionNoCounter:",
              appliedRestrictionNoCounter, "restrictionNoDataNotPreparedCounter",
              restrictionNoDataNotPreparedCounter, "foundRemovedRestrictionNoCounter",
              foundRemovedRestrictionNoCounter, "restrictionOnlyCounter", restrictionOnlyCounter,
              "appliedRestrictionOnlyCounter", appliedRestrictionOnlyCounter,
              "restrictionOnlyDataNotPreparedCounter", restrictionOnlyDataNotPreparedCounter,
              "foundRemovedRestrictionOnlyCounter", foundRemovedRestrictionOnlyCounter));
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
                                     vector<JointEdge> & edges)
{
  RoadGeometry const & road = GetRoad(rp.GetFeatureId());

  bool const bidirectional = !road.IsOneWay();
  if (!isOutgoing || bidirectional)
    GetNeighboringEdge(road, rp, false /* forward */, isOutgoing, edges);

  if (isOutgoing || bidirectional)
    GetNeighboringEdge(road, rp, true /* forward */, isOutgoing, edges);
}

void IndexGraph::GetNeighboringEdge(RoadGeometry const & road, RoadPoint const & rp, bool forward,
                                    bool outgoing, vector<JointEdge> & edges) const
{
  pair<Joint::Id, uint32_t> const & neighbor = m_roadIndex.FindNeighbor(rp, forward);
  if (neighbor.first == Joint::kInvalidId)
    return;

  Joint::Id const rbJointId = m_roadIndex.GetJointId(rp);
  auto const edge =
      outgoing ? make_pair(rbJointId, neighbor.first) : make_pair(neighbor.first, rbJointId);
  if (m_blockedEdges.find(edge) != m_blockedEdges.end())
    return;

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
