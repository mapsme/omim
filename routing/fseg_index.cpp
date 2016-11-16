#include "routing/fseg_index.hpp"

#include "base/logging.hpp"

#include "std/utility.hpp"

namespace routing
{
void FSegIndex::Import(vector<Joint> const & joints)
{
  for (JointId jointId = 0; jointId < joints.size(); ++jointId)
  {
    Joint const & joint = joints[jointId];
    for (uint32_t i = 0; i < joint.GetSize(); ++i)
    {
      FSegId const & entry = joint.GetEntry(i);
      RoadJointIds & roadJoints = m_roads[entry.GetFeatureId()];
      roadJoints.AddJoint(entry.GetSegId(), jointId);
      if (entry.GetFeatureId() > m_maxFeatureId)
        m_maxFeatureId = entry.GetFeatureId();
    }
  }
}

bool FSegIndex::GetAdjacentSegments(uint32_t featureIdFrom, uint32_t featureIdTo,
                                    routing::FSegId & from, routing::FSegId & to, JointId & jointId) const
{
  auto const fromIt = m_roads.find(featureIdFrom);
  if (fromIt == m_roads.cend())
    return false;

  auto const toIt = m_roads.find(featureIdTo);
  if (toIt == m_roads.cend())
    return false;

  routing::RoadJointIds const & roadJointIdsFrom = fromIt->second;
  routing::RoadJointIds const & roadJointIdsTo = toIt->second;
  if (roadJointIdsFrom.GetSize() == 0 || roadJointIdsTo.GetSize() == 0)
    return false; // No sence in restrictions on features without joints.

  // Note. It's important to check other variant besides a restriction from last segment
  // of featureIdFrom to first segment of featureIdTo since two way features can have
  // reverse point order.
  if (roadJointIdsFrom.Back() == roadJointIdsTo.Front())
  {
    from = routing::FSegId(featureIdFrom, roadJointIdsFrom.GetSize() - 1);
    to = routing::FSegId(featureIdTo, 0);
    jointId = roadJointIdsFrom.Back();
    return true;
  }

  if (roadJointIdsFrom.Front() == roadJointIdsTo.Back())
  {
    from = routing::FSegId(featureIdFrom, 0);
    to = routing::FSegId(featureIdTo, roadJointIdsTo.GetSize() - 1);
    jointId = roadJointIdsFrom.Front();
    return true;
  }

  if (roadJointIdsFrom.Back() == roadJointIdsTo.Back())
  {
    from = routing::FSegId(featureIdFrom, roadJointIdsFrom.GetSize() - 1);
    to = routing::FSegId(featureIdTo, roadJointIdsTo.GetSize() - 1);
    jointId = roadJointIdsFrom.Back();
    return true;
  }

  if (roadJointIdsFrom.Front() == roadJointIdsTo.Front())
  {
    from = routing::FSegId(featureIdFrom, 0);
    to = routing::FSegId(featureIdTo, 0);
    jointId = roadJointIdsFrom.Front();
    return true;
  }
  return false; // |featureIdFrom| and |featureIdTo| are not adjacent.
}

void FSegIndex::ApplyRestrictionNo(routing::FSegId const & from, routing::FSegId const & to, JointId jointId)
{
}

void FSegIndex::ApplyRestrictionOnly(routing::FSegId const & from, routing::FSegId const & to, JointId jointId)
{
}

pair<JointId, uint32_t> FSegIndex::FindNeigbor(FSegId fseg, bool forward) const
{
  auto const it = m_roads.find(fseg.GetFeatureId());
  if (it == m_roads.cend())
    return make_pair(kInvalidJointId, 0);

  RoadJointIds const & joints = it->second;
  int32_t const step = forward ? 1 : -1;

  for (uint32_t segId = fseg.GetSegId() + step; segId < joints.GetSize(); segId += step)
  {
    JointId const jointId = joints.GetJointId(segId);
    if (jointId != kInvalidJointId)
      return make_pair(jointId, segId);
  }

  return make_pair(kInvalidJointId, 0);
}
}  // namespace routing
