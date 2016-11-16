#include "routing/road_index.hpp"

#include "routing/routing_exceptions.hpp"

namespace routing
{
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

bool RoadIndex::GetAdjacentFtPoints(uint32_t featureIdFrom, uint32_t featureIdTo, RoadPoint & from,
                                    RoadPoint & to, Joint::Id & jointId) const
{
  auto const fromIt = m_roads.find(featureIdFrom);
  if (fromIt == m_roads.cend())
    return false;

  auto const toIt = m_roads.find(featureIdTo);
  if (toIt == m_roads.cend())
    return false;

  RoadJointIds const & roadJointIdsFrom = fromIt->second;
  RoadJointIds const & roadJointIdsTo = toIt->second;
  if (roadJointIdsFrom.GetSize() == 0 || roadJointIdsTo.GetSize() == 0)
    return false; // No sence in restrictions on features without joints.

  // Note. It's important to check other variant besides a restriction from last segment
  // of featureIdFrom to first segment of featureIdTo since two way features can have
  // reverse point order.
  if (roadJointIdsFrom.Back() == roadJointIdsTo.Front())
  {
    from = RoadPoint(featureIdFrom, roadJointIdsFrom.GetSize() - 1);
    to = RoadPoint(featureIdTo, 0);
    jointId = roadJointIdsFrom.Back();
    return true;
  }

  if (roadJointIdsFrom.Front() == roadJointIdsTo.Back())
  {
    from = RoadPoint(featureIdFrom, 0);
    to = RoadPoint(featureIdTo, roadJointIdsTo.GetSize() - 1);
    jointId = roadJointIdsFrom.Front();
    return true;
  }

  if (roadJointIdsFrom.Back() == roadJointIdsTo.Back())
  {
    from = RoadPoint(featureIdFrom, roadJointIdsFrom.GetSize() - 1);
    to = RoadPoint(featureIdTo, roadJointIdsTo.GetSize() - 1);
    jointId = roadJointIdsFrom.Back();
    return true;
  }

  if (roadJointIdsFrom.Front() == roadJointIdsTo.Front())
  {
    from = RoadPoint(featureIdFrom, 0);
    to = RoadPoint(featureIdTo, 0);
    jointId = roadJointIdsFrom.Front();
    return true;
  }
  return false; // |featureIdFrom| and |featureIdTo| are not adjacent.
}

pair<Joint::Id, uint32_t> RoadIndex::FindNeighbor(RoadPoint const & rp, bool forward) const
{
  auto const it = m_roads.find(rp.GetFeatureId());
  if (it == m_roads.cend())
    MYTHROW(RoutingException, ("RoadIndex doesn't contains feature", rp.GetFeatureId()));

  return it->second.FindNeighbor(rp.GetPointId(), forward);
}
}  // namespace routing
