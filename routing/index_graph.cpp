#include "index_graph.hpp"

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

double IndexGraph::GetSpeed(RoadPoint ftp)
{
  return GetRoad(ftp.GetFeatureId()).GetSpeed();
}

void IndexGraph::Build(uint32_t jointNumber)
{
  m_jointIndex.Build(m_roadIndex, jointNumber);
}

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
    GetSingleFeaturePaths(c.first, c.second, roadPoints);
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
    MYTHROW(RootException,
            ("Joint from:", from, "and joint to:", to, "are not connected a feature necessary type."));
  }

  GetSingleFeaturePaths(minWeightConnection.first /* from */, minWeightConnection.second /* to */,
      shortestConnectionPath);
}

void IndexGraph::GetOutgoingGeomEdges(vector<JointEdge> const & outgoingEdges, Joint::Id center,
                                      vector<JointEdgeGeom> & outgoingGeomEdges)
{
  for (JointEdge const & e : outgoingEdges)
  {
    vector<RoadPoint> shortestConnectionPath;
    GetShortestConnectionPath(center, e.GetTarget(), shortestConnectionPath);
    outgoingGeomEdges.emplace_back(e.GetTarget(), shortestConnectionPath);
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

uint32_t IndexGraph::AddFakeFeature(Joint::Id from, Joint::Id to, vector<RoadPoint> const & geometrySource)
{
  CHECK_LESS(from, m_jointIndex.GetNumJoints(), ());
  CHECK_LESS(to, m_jointIndex.GetNumJoints(), ());
  CHECK_LESS(1, geometrySource.size(), ());

  // Getting fake feature geometry.
  RoadGeometry geom;
  CreateFakeFeatureGeometry(geometrySource, geom);
  m_fakeFeatureGeometry.insert(make_pair(m_nextFakeFeatureId, geom));

  // Adding to indexes for fake feature: |from|->{via point}->|to|.
  RoadPoint const fromFakeFtPoint(m_nextFakeFeatureId, 0 /* point id */);
  RoadPoint const toFakeFtPoint(m_nextFakeFeatureId, geometrySource.size() - 1 /* point id */);

  m_roadIndex.AddJoint(fromFakeFtPoint, from);
  m_roadIndex.AddJoint(toFakeFtPoint, to);

  m_jointIndex.AppendToJoint(from, fromFakeFtPoint);
  m_jointIndex.AppendToJoint(to, toFakeFtPoint);

  return m_nextFakeFeatureId++;
}

void IndexGraph::FindOneStepAsideRoadPoint(RoadPoint const & center, Joint::Id centerId,
                                           vector<JointEdge> const & edges,
                                           vector<Joint::Id> & oneStepAside) const
{
  oneStepAside.clear();
  m_roadIndex.ForEachJoint(center.GetFeatureId(),
                           [&](uint32_t /*pointId*/, Joint::Id jointId){
    for (JointEdge const & e : edges)
    {
      if (e.GetTarget() == jointId)
        oneStepAside.push_back(jointId);
    }
  });
}

bool IndexGraph::ApplyRestrictionPrepareData(RoadPoint const & from, RoadPoint const & to, Joint::Id centerId,
                                             vector<JointEdge> & ingoingEdges, vector<JointEdge> & outgoingEdges,
                                             Joint::Id & fromFirstOneStepAside, Joint::Id & toFirstOneStepAside)
{
  GetEdgeList(centerId, false /* isOutgoing */, ingoingEdges);
  vector<Joint::Id> fromOneStepAside;
  FindOneStepAsideRoadPoint(from, centerId, ingoingEdges, fromOneStepAside);
  if (fromOneStepAside.empty())
    return false;

  GetEdgeList(centerId, true /* isOutgoing */, outgoingEdges);
  vector<Joint::Id> toOneStepAside;
  FindOneStepAsideRoadPoint(to, centerId, outgoingEdges, toOneStepAside);
  if (toOneStepAside.empty())
    return false;

  fromFirstOneStepAside = fromOneStepAside.back();
  toFirstOneStepAside = toOneStepAside.back();
  return true;
}

void IndexGraph::ApplyRestrictionNo(RoadPoint const & from, RoadPoint const & to,
                                    Joint::Id centerId)
{
  vector<JointEdge> ingoingEdges;
  vector<JointEdge> outgoingEdges;
  Joint::Id fromFirstOneStepAside = Joint::kInvalidId;
  Joint::Id toFirstOneStepAside = Joint::kInvalidId;
  if (!ApplyRestrictionPrepareData(from, to, centerId, ingoingEdges, outgoingEdges,
                                   fromFirstOneStepAside, toFirstOneStepAside))
  {
    return;
  }

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

  // Prohibition of moving from one segment to another in case of any number of ingoing and outgoing edges.
  // The idea is to tranform the navigation graph for every non-degenerate case as it's shown below.
  // At the picture below a restriction for prohibition moving from 4 to O to 3 is shown.
  // So to implement it it's necessary to remove (disable) an edge 4-O and add features (edges) 4-N-1 and N-2.
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

  outgoingEdges.erase(remove_if(outgoingEdges.begin(), outgoingEdges.end(),
                                [&](JointEdge const & e)
  {
    // Removing edge N->3 in example above.
    return e.GetTarget() == toFirstOneStepAside
    // Preveting form adding in loop below
    // cycles |fromFirstOneStepAside|->|centerId|->|fromFirstOneStepAside|.
        || e.GetTarget() == fromFirstOneStepAside
    // Removing edges |centerId|->|centerId|.
        || e.GetTarget() == centerId;
  }), outgoingEdges.end());

  // Getting outgoing shortest edges geometry.
  // Node. |centerId| could be connected with any outgoing joint with
  // mote than one edge but only the shortest one should be takenn into
  // account.
  my::SortUnique(outgoingEdges, my::LessBy(&JointEdge::GetTarget),
                 my::EqualsBy(&JointEdge::GetTarget));
  vector<JointEdgeGeom> outgoingShortestEdges;
  GetOutgoingGeomEdges(outgoingEdges, centerId, outgoingShortestEdges);

  vector<RoadPoint> shortestIngoingPath;
  GetShortestConnectionPath(fromFirstOneStepAside, centerId, shortestIngoingPath);
  JointEdgeGeom ingoingShortestEdge(fromFirstOneStepAside, shortestIngoingPath);

  Joint::Id newJoint = Joint::kInvalidId;
  for (auto it = outgoingShortestEdges.cbegin();
       it != outgoingShortestEdges.cend(); ++it)
  {
    if (it == outgoingShortestEdges.cbegin())
    {
      // Adding the edge 4->N->1 in example above.
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
      CHECK(!ingoingShortestEdge.GetShortestPath().empty(), ());
      vector<RoadPoint> geometrySource(ingoingShortestEdge.GetShortestPath().cbegin(),
                                       ingoingShortestEdge.GetShortestPath().cend() - 1);
      geometrySource.insert(geometrySource.end(), it->GetShortestPath().cbegin(),
                            it->GetShortestPath().cend());

      uint32_t const featureId = AddFakeFeature(fromFirstOneStepAside, it->GetTarget(), geometrySource);
      newJoint = InsertJoint({featureId,
                              static_cast<uint32_t>(ingoingShortestEdge.GetShortestPath().size() - 1)
                              /* Intermediate joint of added feature */});
    }
    else
    {
      // Adding the edge N->2 in example above.
      AddFakeFeature(newJoint, it->GetTarget(), it->GetShortestPath());
    }
  }

  DisableEdge(fromFirstOneStepAside, centerId);
}

void IndexGraph::ApplyRestrictionOnly(RoadPoint const & from, RoadPoint const & to,
                                      Joint::Id centerId)
{
  vector<JointEdge> ingoingEdges;
  vector<JointEdge> outgoingEdges;
  Joint::Id fromFirstOneStepAside = Joint::kInvalidId;
  Joint::Id toFirstOneStepAside = Joint::kInvalidId;
  if (!ApplyRestrictionPrepareData(from, to, centerId, ingoingEdges, outgoingEdges,
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

  // It's possible to move only from one segment to another in case of any number of ingoing and outgoing edges.
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

  vector<RoadPoint> ingoingShortestPath;
  GetShortestConnectionPath(fromFirstOneStepAside, centerId, ingoingShortestPath);
  vector<RoadPoint> outgoingShortestPath;
  GetShortestConnectionPath(centerId, toFirstOneStepAside, outgoingShortestPath);
  CHECK_LESS(0, ingoingShortestPath.size(), ());
  vector<RoadPoint> geometrySource(ingoingShortestPath.cbegin(), ingoingShortestPath.cend() - 1);
  geometrySource.insert(geometrySource.end(), outgoingShortestPath.cbegin(), outgoingShortestPath.cend());

  AddFakeFeature(fromFirstOneStepAside, toFirstOneStepAside, geometrySource);
  DisableEdge(fromFirstOneStepAside, centerId);
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

    RoadPoint from;
    RoadPoint to;
    Joint::Id jointId;
    if (!m_roadIndex.GetAdjacentFtPoints(restriction.m_featureIds[0], restriction.m_featureIds[1],
        from, to, jointId))
    {
      continue; // Restriction is not contain adjacent features.
    }

    try
    {
      switch (restriction.m_type)
      {
      case Restriction::Type::No: ApplyRestrictionNo(from, to, jointId); continue;
      case Restriction::Type::Only: ApplyRestrictionOnly(from, to, jointId); continue;
      }
    }
    catch(RootException const & e)
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
  auto const edge = outgoing ? make_pair(rbJointId, neighbor.first)
                             : make_pair(neighbor.first, rbJointId);
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
