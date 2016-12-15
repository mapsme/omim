#include "routing/road_index.hpp"

#include "routing/road_point.hpp"
#include "routing/routing_exceptions.hpp"

namespace
{
void MakeRestrictionPoint(uint32_t featureIdFrom, uint32_t pointIdFrom, uint32_t featureIdTo,
                          uint32_t pointIdTo, routing::Joint::Id centerId,
                          routing::RestrictionPoint & restrictionPoint)
{
  restrictionPoint.m_from = routing::RoadPoint(featureIdFrom, pointIdFrom);
  restrictionPoint.m_to = routing::RoadPoint(featureIdTo, pointIdTo);
  restrictionPoint.m_centerId = centerId;
}
}  // namespace

namespace routing
{
bool RestrictionPoint::operator<(RestrictionPoint const & rhs) const
{
  if (m_from != rhs.m_from)
    return m_from < rhs.m_from;

  if (m_to != rhs.m_to)
    return m_to < rhs.m_to;

  return m_centerId < rhs.m_centerId;
}

void RoadIndex::Import(vector<Joint> const & joints)
{
  for (Joint::Id jointId = 0; jointId < joints.size(); ++jointId)
  {
    Joint const & joint = joints[jointId];
    for (uint32_t i = 0; i < joint.GetSize(); ++i)
    {
      RoadPoint const & entry = joint.GetEntry(i);
      RoadJointIds & roadJoints = m_roads[entry.GetFeatureId()];
      roadJoints.AddJoint(entry.GetPointId(), jointId);
    }
  }
}

bool RoadIndex::GetRestrictionPoint(uint32_t featureIdFrom, uint32_t featureIdTo,
                                    RestrictionPoint & restrictionPoint) const
{
  auto const fromIt = m_roads.find(featureIdFrom);
  if (fromIt == m_roads.cend())
    return false;  // No sense in restrictions to non-road features.

  auto const toIt = m_roads.find(featureIdTo);
  if (toIt == m_roads.cend())
    return false;  // No sense in restrictions to non-road features.

  RoadJointIds const & roadJointIdsFrom = fromIt->second;
  RoadJointIds const & roadJointIdsTo = toIt->second;
  if (roadJointIdsFrom.IsEmpty() || roadJointIdsTo.IsEmpty())
    return false;  // No sense in restrictions on features without joints.

  // Note 1. It's important to check other variant besides a restriction from last segment
  // of featureIdFrom to first segment of featureIdTo since two way features can have
  // reverse point order.
  // Note 2. The code below assumes that feature of restrictions are connected with its ends.
  // It's true for overwhelming majority for restrictions. If a restriction is defined with
  // two feature which is not connected at all or which is connected but not with its ends
  // the restriction is not used.
  vector<size_t> from = {0, roadJointIdsFrom.GetSize() - 1};
  vector<size_t> to = {0, roadJointIdsTo.GetSize() - 1};

  for (size_t fromIdx : from)
  {
    for (size_t toIdx : to)
    {
      if (roadJointIdsFrom.GetJointId(fromIdx) == roadJointIdsTo.GetJointId(toIdx))
      {
        MakeRestrictionPoint(featureIdFrom, fromIdx, featureIdTo, toIdx,
                             roadJointIdsFrom.GetJointId(fromIdx), restrictionPoint);
        return true;
      }
    }
  }

  return false;  // |featureIdFrom| and |featureIdTo| are not adjacent.
}

pair<Joint::Id, uint32_t> RoadIndex::FindNeighbor(RoadPoint const & rp, bool forward) const
{
  auto const it = m_roads.find(rp.GetFeatureId());
  if (it == m_roads.cend())
    MYTHROW(RoutingException, ("RoadIndex doesn't contains feature", rp.GetFeatureId()));

  return it->second.FindNeighbor(rp.GetPointId(), forward);
}

string DebugPrint(RestrictionPoint const & restrictionPoint)
{
  ostringstream out;
  out << "restrictionPoint [ m_from: " << DebugPrint(restrictionPoint.m_from)
      << " m_to: " << DebugPrint(restrictionPoint.m_to)
      << " m_centerId: " << restrictionPoint.m_centerId << " ]";
  return out.str();
}
}  // namespace routing
