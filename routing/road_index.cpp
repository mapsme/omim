#include "routing/road_index.hpp"

#include "routing/routing_exceptions.hpp"
#include "routing/road_point.hpp"

namespace
{
void SetCrossingPoint(uint32_t featureIdFrom, uint32_t pointIdFrom,
                      uint32_t featureIdTo, uint32_t pointIdTo, routing::Joint::Id centerId,
                      routing::RestrictionPoint & crossingPoint)
{
  crossingPoint.m_from = routing::RoadPoint(featureIdFrom, pointIdFrom);
  crossingPoint.m_to = routing::RoadPoint(featureIdTo, pointIdTo);
  crossingPoint.m_centerId = centerId;
}
}  // namespace

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

bool RoadIndex::GetAdjacentFtPoint(uint32_t featureIdFrom, uint32_t featureIdTo,
                                   RestrictionPoint & crossingPoint) const
{
  auto const fromIt = m_roads.find(featureIdFrom);
  if (fromIt == m_roads.cend())
  {
    LOG(LERROR, ("Cannot find in |m_roads| featureIdFrom =", featureIdFrom));
    return false;
  }

  auto const toIt = m_roads.find(featureIdTo);
  if (toIt == m_roads.cend())
  {
    LOG(LERROR, ("Cannot find in |m_roads| toIt =", featureIdTo));
    return false;
  }

  RoadJointIds const & roadJointIdsFrom = fromIt->second;
  RoadJointIds const & roadJointIdsTo = toIt->second;
  if (roadJointIdsFrom.IsEmpty() || roadJointIdsTo.IsEmpty())
    return false;  // No sense in restrictions on features without joints.

  // Note. It's important to check other variant besides a restriction from last segment
  // of featureIdFrom to first segment of featureIdTo since two way features can have
  // reverse point order.
  if (roadJointIdsFrom.Back() == roadJointIdsTo.Front())
  {
    SetCrossingPoint(featureIdFrom, roadJointIdsFrom.GetSize() - 1, featureIdTo, 0 /* pointId */,
                     roadJointIdsFrom.Back(), crossingPoint);
    return true;
  }

  if (roadJointIdsFrom.Front() == roadJointIdsTo.Back())
  {
    SetCrossingPoint(featureIdFrom, 0 /* pointId */, featureIdTo, roadJointIdsTo.GetSize() - 1,
                     roadJointIdsFrom.Front(), crossingPoint);
    return true;
  }

  if (roadJointIdsFrom.Back() == roadJointIdsTo.Back())
  {
    SetCrossingPoint(featureIdFrom, roadJointIdsFrom.GetSize() - 1, featureIdTo, roadJointIdsTo.GetSize() - 1,
                     roadJointIdsFrom.Back(), crossingPoint);
    return true;
  }

  if (roadJointIdsFrom.Front() == roadJointIdsTo.Front())
  {
    SetCrossingPoint(featureIdFrom, 0 /* pointId */, featureIdTo, 0 /* pointId */,
                     roadJointIdsFrom.Front(), crossingPoint);
    return true;
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

string DebugPrint(RestrictionPoint const & crossingPoint)
{
  ostringstream out;
  out << "CrossingPoint [ m_from: " << DebugPrint(crossingPoint.m_from)
      << " m_to: " << DebugPrint(crossingPoint.m_to) << " m_centerId: " << crossingPoint.m_centerId
      << " ]";
  return out.str();
}
}  // namespace routing
