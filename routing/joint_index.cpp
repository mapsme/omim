#include "routing/joint_index.hpp"

#include "routing/routing_exceptions.hpp"

namespace routing
{
RoadPoint JointIndex::GetPoint(Joint::Id jointId) const
{
  if (IsStatic(jointId))
    return m_points[Begin(jointId)];

  CHECK_LESS(jointId, GetNumJoints(), ());
  auto const jointIt = m_dynamicJoints.find(jointId);
  CHECK(jointIt != m_dynamicJoints.cend(), ());
  return jointIt->second.GetEntry(0 /* first point in joint */);
}

Joint::Id JointIndex::InsertJoint(RoadPoint const & rp)
{
  Joint::Id const jointId = GetNumJoints();
  AppendToJoint(jointId, rp);
  return jointId;
}

void JointIndex::AppendToJoint(Joint::Id jointId, RoadPoint const & rp)
{
  m_dynamicJoints[jointId].AddPoint(rp);
}

void JointIndex::FindPointsWithCommonFeature(Joint::Id jointId0, Joint::Id jointId1,
                                             vector<pair<RoadPoint, RoadPoint>> & result) const
{
  result.clear();
  ForEachPoint(jointId0, [&](RoadPoint const & rp0) {
    ForEachPoint(jointId1, [&](RoadPoint const & rp1) {
      if (rp0.GetFeatureId() == rp1.GetFeatureId())
        result.emplace_back(rp0, rp1);
    });
  });
}

void JointIndex::Build(RoadIndex const & roadIndex, uint32_t numJoints)
{
  m_dynamicJoints.clear();

  // + 1 is protection for 'End' method from out of bounds.
  // Call End(numJoints-1) requires more size, so add one more item.
  // Therefore m_offsets.size() == numJoints + 1,
  // And m_offsets.back() == m_points.size()
  m_offsets.assign(numJoints + 1, 0);

  // Calculate sizes.
  // Example for numJoints = 6:
  // 2, 5, 3, 4, 2, 3, 0
  roadIndex.ForEachRoad([this, numJoints](uint32_t /* featureId */, RoadJointIds const & road) {
    road.ForEachJoint([this, numJoints](uint32_t /* pointId */, Joint::Id jointId) {
      ASSERT_LESS(jointId, numJoints, ());
      ++m_offsets[jointId];
    });
  });

  // Fill offsets with end bounds.
  // Example: 2, 7, 10, 14, 16, 19, 19
  for (size_t i = 1; i < m_offsets.size(); ++i)
    m_offsets[i] += m_offsets[i - 1];

  m_points.resize(m_offsets.back());

  // Now fill points.
  // Offsets after this operation are begin bounds:
  // 0, 2, 7, 10, 14, 16, 19
  roadIndex.ForEachRoad([this](uint32_t featureId, RoadJointIds const & road) {
    road.ForEachJoint([this, featureId](uint32_t pointId, Joint::Id jointId) {
      uint32_t & offset = m_offsets[jointId];
      --offset;
      m_points[offset] = {featureId, pointId};
    });
  });

  CHECK_EQUAL(m_offsets[0], 0, ());
  CHECK_EQUAL(m_offsets.back(), m_points.size(), ());
}
}  // namespace routing
