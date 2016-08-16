#pragma once

#include "geometry/polyline2d.hpp"

#include "std/map.hpp"
#include "std/string.hpp"
#include "std/vector.hpp"

namespace df
{

enum TrafficSpeedBucket
{
  Normal = 3,
  Slow = 2,
  VerySlow = 1
};

struct TrafficSegmentData
{
  string m_id;
  TrafficSpeedBucket m_speedBucket;
};

class TrafficGenerator final
{
public:
  TrafficGenerator();

  void AddSegment(string const & segmentId, m2::PolylineD const & polyline);

  void SetTrafficData(vector<TrafficSegmentData> const & trafficData);

private:
  using TSegmentCollection = map<string, m2::PolylineD>;

  TSegmentCollection m_segments;
};

} // namespace df
