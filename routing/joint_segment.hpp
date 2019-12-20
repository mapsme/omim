#pragma once

#include "routing/road_point.hpp"
#include "routing/route_weight.hpp"
#include "routing/segment.hpp"

#include "base/assert.hpp"

#include <cstdint>
#include <limits>
#include <optional>
#include <string>

namespace routing
{
class JointSegment
{
public:
  static uint32_t constexpr kInvalidId = std::numeric_limits<uint32_t>::max();

  JointSegment() = default;
  JointSegment(Segment const & from, Segment const & to);

  void SetFeatureId(uint32_t id) { m_featureId = id; }
  uint32_t GetFeatureId() const { return m_featureId; }
  NumMwmId GetMwmId() const { return m_numMwmId; }
  void SetMwmId(NumMwmId id) { m_numMwmId = id; }
  uint32_t GetStartSegmentId() const { return m_startSegmentId; }
  uint32_t GetEndSegmentId() const { return m_endSegmentId; }
  uint32_t GetSegmentId(bool start) const { return start ? m_startSegmentId : m_endSegmentId; }
  bool IsForward() const { return m_forward; }

  void ToFake(uint32_t fakeId);
  bool IsFake() const;
  bool IsRealSegment() const { return !IsFake(); }

  Segment GetSegment(bool start) const;
  size_t GetHash() const;

  bool operator<(JointSegment const & rhs) const;
  bool operator==(JointSegment const & rhs) const;
  bool operator!=(JointSegment const & rhs) const;

  struct Hash
  {
    size_t operator()(JointSegment const & jointSegment) const { return jointSegment.GetHash(); }
  };

  inline static uint64_t kAll = 0;
  inline static uint64_t kCompute = 0;
private:
  uint32_t m_featureId = kInvalidId;
  uint32_t m_startSegmentId = kInvalidId;
  uint32_t m_endSegmentId = kInvalidId;
  NumMwmId m_numMwmId = kFakeNumMwmId;
  bool m_forward = false;
  mutable std::optional<size_t> m_hash;
};

class JointEdge
{
public:
  JointEdge(JointSegment const & target, RouteWeight const & weight)
    : m_target(target), m_weight(weight) {}

  JointSegment const & GetTarget() const { return m_target; }
  JointSegment & GetTarget() { return m_target; }
  RouteWeight & GetWeight() { return m_weight; }
  RouteWeight const & GetWeight() const { return m_weight; }

private:
  JointSegment m_target;
  RouteWeight m_weight;
};

std::string DebugPrint(JointSegment const & jointSegment);
}  // namespace routing
