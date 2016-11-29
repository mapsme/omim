#pragma once

#include "routing/joint.hpp"
#include "routing/routing_serialization.hpp"

#include "std/algorithm.hpp"
#include "std/cstdint.hpp"
#include "std/unordered_map.hpp"
#include "std/utility.hpp"
#include "std/vector.hpp"

namespace routing
{
/// \brief this class describe a two link restriction which is staring on one feature
/// and ending on another one.
class RestrictionPoint final
{
public:
  RestrictionPoint() = default;

  /// \param from feature id and point id of starting restriction segment.
  /// \param to feature id and point id of ending restriction segment.
  /// \param centerId joint id of the restriction point.
  RestrictionPoint(RoadPoint const & from, RoadPoint const & to, Joint::Id centerId)
    : m_from(from), m_to(to), m_centerId(centerId)
  {
  }

  bool operator<(RestrictionPoint const & rhs) const
  {
    if (m_from != rhs.m_from)
      return m_from < rhs.m_from;

    if (m_to != rhs.m_to)
      return m_to < rhs.m_to;

    return m_centerId < rhs.m_centerId;
  }

  bool operator==(RestrictionPoint const & rhs) const { return !(*this < rhs || rhs < *this); }

  bool operator!=(RestrictionPoint const & rhs) const { return !(*this == rhs); }

  RoadPoint m_from;
  RoadPoint m_to;
  Joint::Id m_centerId = Joint::kInvalidId;
};

class RoadJointIds final
{
public:
  void Init(uint32_t maxPointId)
  {
    m_jointIds.clear();
    m_jointIds.reserve(maxPointId + 1);
  }

  Joint::Id GetJointId(uint32_t pointId) const
  {
    if (pointId < m_jointIds.size())
      return m_jointIds[pointId];

    return Joint::kInvalidId;
  }

  void AddJoint(uint32_t pointId, Joint::Id jointId)
  {
    ASSERT_NOT_EQUAL(jointId, Joint::kInvalidId, ());

    if (pointId >= m_jointIds.size())
      m_jointIds.insert(m_jointIds.end(), pointId + 1 - m_jointIds.size(), Joint::kInvalidId);

    ASSERT_EQUAL(m_jointIds[pointId], Joint::kInvalidId, ());
    m_jointIds[pointId] = jointId;
  }

  uint32_t GetJointsNumber() const
  {
    uint32_t count = 0;

    for (Joint::Id const jointId : m_jointIds)
    {
      if (jointId != Joint::kInvalidId)
        ++count;
    }

    return count;
  }

  template <typename F>
  void ForEachJoint(F && f) const
  {
    for (uint32_t pointId = 0; pointId < m_jointIds.size(); ++pointId)
    {
      Joint::Id const jointId = m_jointIds[pointId];
      if (jointId != Joint::kInvalidId)
        f(pointId, jointId);
    }
  }

  pair<Joint::Id, uint32_t> FindNeighbor(uint32_t pointId, bool forward) const
  {
    uint32_t const size = static_cast<uint32_t>(m_jointIds.size());
    pair<Joint::Id, uint32_t> result = make_pair(Joint::kInvalidId, 0);

    if (forward)
    {
      for (uint32_t i = pointId + 1; i < size; ++i)
      {
        Joint::Id const jointId = m_jointIds[i];
        if (jointId != Joint::kInvalidId)
        {
          result = {jointId, i};
          return result;
        }
      }
    }
    else
    {
      for (uint32_t i = min(pointId, size) - 1; i < size; --i)
      {
        Joint::Id const jointId = m_jointIds[i];
        if (jointId != Joint::kInvalidId)
        {
          result = {jointId, i};
          return result;
        }
      }
    }

    return result;
  }

  Joint::Id Front() const { return m_jointIds.front(); }

  Joint::Id Back() const { return m_jointIds.back(); }

  size_t GetSize() const { return m_jointIds.size(); }

  size_t IsEmpty() const { return m_jointIds.empty(); }

  template <class Sink>
  void Serialize(Sink & sink) const
  {
    WriteToSink(sink, static_cast<Joint::Id>(m_jointIds.size()));
    for (Joint::Id jointId : m_jointIds)
      WriteToSink(sink, jointId);
  }

  template <class Source>
  void Deserialize(Source & src)
  {
    m_jointIds.clear();
    Joint::Id const jointsSize = ReadPrimitiveFromSource<Joint::Id>(src);
    m_jointIds.reserve(jointsSize);
    for (Joint::Id i = 0; i < jointsSize; ++i)
    {
      Joint::Id const jointId = ReadPrimitiveFromSource<Joint::Id>(src);
      m_jointIds.emplace_back(jointId);
    }
  }

private:
  // Joint ids indexed by point id.
  // If some point id doesn't match any joint id, this vector contains Joint::kInvalidId.
  vector<Joint::Id> m_jointIds;
};

class RoadIndex final
{
public:
  void Import(vector<Joint> const & joints);

  /// \brief if |featureIdFrom| and |featureIdTo| are adjacent and if they are connected by
  /// ends fills |restrictionPoint| and return true.
  /// Otherwise returns false.
  bool GetAdjacentFtPoint(uint32_t featureIdFrom, uint32_t featureIdTo,
                          RestrictionPoint & restrictionPoint) const;

  void AddJoint(RoadPoint const & rp, Joint::Id jointId)
  {
    m_roads[rp.GetFeatureId()].AddJoint(rp.GetPointId(), jointId);
  }

  RoadJointIds const & GetRoad(uint32_t featureId) const
  {
    auto const & it = m_roads.find(featureId);
    CHECK(it != m_roads.cend(), ());
    return it->second;
  }

  void PushFromSerializer(Joint::Id jointId, RoadPoint const & rp)
  {
    m_roads[rp.GetFeatureId()].AddJoint(rp.GetPointId(), jointId);
  }

  // Find nearest point with normal joint id.
  // If forward == true: neighbor with larger point id (right neighbor)
  // If forward == false: neighbor with smaller point id (left neighbor)
  //
  // If there is no nearest point, return {Joint::kInvalidId, 0}
  pair<Joint::Id, uint32_t> FindNeighbor(RoadPoint const & rp, bool forward) const;

  uint32_t GetSize() const { return m_roads.size(); }

  Joint::Id GetJointId(RoadPoint const & rp) const
  {
    auto const it = m_roads.find(rp.GetFeatureId());
    if (it == m_roads.end())
      return Joint::kInvalidId;

    return it->second.GetJointId(rp.GetPointId());
  }

  template <typename F>
  void ForEachRoad(F && f) const
  {
    for (auto const & it : m_roads)
      f(it.first, it.second);
  }

  template <typename F>
  void ForEachJoint(uint32_t featureId, F && f) const
  {
    auto const it = m_roads.find(featureId);
    if (it != m_roads.cend())
      it->second.ForEachJoint(forward<F>(f));
  }

private:
  // Map from feature id to RoadJointIds.
  unordered_map<uint32_t, RoadJointIds> m_roads;
};

string DebugPrint(RestrictionPoint const & restrictionPoint);
}  // namespace routing
