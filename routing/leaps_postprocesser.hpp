#pragma once

#include "routing/base/bfs.hpp"

#include "routing/index_graph_starter.hpp"
#include "routing/segment.hpp"

#include "base/math.hpp"

#include <map>
#include <vector>

namespace routing
{
class LeapsPostProcessor
{
public:
  LeapsPostProcessor(std::vector<Segment> const & path, IndexGraphStarter & starter);

  struct PathInterval
  {
    static double constexpr kWeightEps = 1;

    PathInterval(double weight, size_t left, size_t right, std::vector<Segment> && path);
    bool operator>(PathInterval const & rhs) const;
    bool operator<(PathInterval const & rhs) const;

    double m_winWeight = 0.0;
    size_t m_left = 0;
    size_t m_right = 0;
    std::vector<Segment> m_path;
  };

  std::vector<Segment> GetProcessedPath();

private:
  static size_t constexpr kMaxStep = 5;

  struct SegmentData
  {
    SegmentData() = default;
    SegmentData(size_t steps, double eta);
    size_t m_steps = 0;
    double m_summaryETA = 0.0;
  };

  std::set<PathInterval, std::greater<>> CalculateIntervalsToRelax();
  void FillIngoingPaths(Segment const & start, std::map<Segment, SegmentData> & segmentsData);

  std::vector<Segment> const & m_path;
  IndexGraphStarter & m_starter;

  BFS<IndexGraphStarter> m_bfs;
  std::map<Segment, size_t> m_segmentToIndex;
  std::vector<double> m_prefixSumETA;
};
}  // namespace routing
