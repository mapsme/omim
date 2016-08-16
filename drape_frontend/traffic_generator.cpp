#include "drape_frontend/traffic_generator.hpp"

#include "base/logging.hpp"

#include "std/algorithm.hpp"

namespace df
{

TrafficGenerator::TrafficGenerator()
{
}

void TrafficGenerator::AddSegment(string const & segmentId, m2::PolylineD const & polyline)
{
  m_segments.insert(make_pair(segmentId, polyline));
}

void TrafficGenerator::SetTrafficData(vector<TrafficSegmentData> const & trafficData)
{

}

} // namespace df

