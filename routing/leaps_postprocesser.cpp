#include "routing/leaps_postprocesser.hpp"

#include "coding/not_intersecting_intervals.hpp"

#include "base/assert.hpp"
#include "base/logging.hpp"
#include "base/scope_guard.hpp"
#include "base/timer.hpp"

#include <algorithm>
#include <iterator>

namespace
{
using namespace routing;
using namespace coding;

class NotIntersectingSegmentPaths : public NotIntersectingIntervals
{
public:
  void AddInterval(LeapsPostProcessor::PathInterval const & pathInterval)
  {
    if (!NotIntersectingIntervals::AddInterval(pathInterval.m_left, pathInterval.m_right))
      return;

    m_intervals.emplace_back(pathInterval);
  }

  std::vector<LeapsPostProcessor::PathInterval> && StealIntervals() { return std::move(m_intervals); }

private:
  std::vector<LeapsPostProcessor::PathInterval> m_intervals;
};
}  // namespace

namespace routing
{
LeapsPostProcessor::LeapsPostProcessor(std::vector<Segment> const & path, IndexGraphStarter & starter)
  : m_path(path),
    m_starter(starter),
    m_bfs(starter),
    m_prefixSumETA(path.size(), 0.0)
{
  for (size_t i = 1; i < path.size(); ++i)
  {
    auto const & segment = path[i];
    m_prefixSumETA[i] = m_prefixSumETA[i - 1] + starter.CalcSegmentETA(segment);

    CHECK_EQUAL(m_segmentToIndex.count(segment), 0, ());
    m_segmentToIndex[segment] = i;
  }
}

std::vector<Segment> LeapsPostProcessor::GetProcessedPath()
{
  base::HighResTimer timer;
  SCOPE_GUARD(timerGuard, [&timer]() {
    LOG(LINFO, ("Time for LeapsPostProcessor() is:", timer.ElapsedNano() / 1e6, "ms"));
  });

  std::set<PathInterval, std::greater<>> intervalsToRelax =
      CalculateIntervalsToRelax();

  NotIntersectingSegmentPaths toReplace;
  for (auto const & interval : intervalsToRelax)
    toReplace.AddInterval(interval);

  std::vector<PathInterval> intervals = toReplace.StealIntervals();
  std::sort(intervals.begin(), intervals.end());

  std::vector<Segment> output;
  size_t prevIndex = 0;
  for (auto & interval : intervals)
  {
    CHECK(m_path.begin() + prevIndex <= m_path.begin() + interval.m_left, ());
    output.insert(output.end(), m_path.begin() + prevIndex, m_path.begin() + interval.m_left);

    auto leftIt = std::make_move_iterator(interval.m_path.begin());
    auto rightIt = std::make_move_iterator(interval.m_path.end());
    output.insert(output.end(), leftIt, rightIt);

    prevIndex = interval.m_right + 1;
  }

  output.insert(output.end(), m_path.begin() + prevIndex, m_path.end());
  return output;
}

std::set<LeapsPostProcessor::PathInterval, std::greater<>>
LeapsPostProcessor::CalculateIntervalsToRelax()
{
  std::set<PathInterval, std::greater<>> result;
  for (size_t right = kMaxStep; right < m_path.size(); ++right)
  {
    std::map<Segment, SegmentData> segmentsData;
    auto const & segment = m_path[right];
    segmentsData.emplace(segment, SegmentData(0, m_starter.CalcSegmentETA(segment)));

    FillIngoingPaths(segment, segmentsData);

    for (auto const & item : segmentsData)
    {
      Segment const & visited = item.first;
      SegmentData const & data = item.second;

      auto const it = m_segmentToIndex.find(visited);
      if (it == m_segmentToIndex.cend())
        continue;

      size_t left = it->second;
      if (left >= right || left == 0)
        continue;

      auto const prevWeight = m_prefixSumETA[right] - m_prefixSumETA[left - 1];
      auto const curWeight = data.m_summaryETA;
      if (prevWeight - PathInterval::kWeightEps <= curWeight)
        continue;

      double const winWeight = prevWeight - curWeight;
      result.emplace(winWeight, left, right, m_bfs.ReconstructPath(visited));
    }
  }

  return result;
}

void LeapsPostProcessor::FillIngoingPaths(
    Segment const & start, map<Segment, LeapsPostProcessor::SegmentData> & segmentsData)
{
  m_bfs.Run(start, false /* isOutgoing */, [&](BFS<IndexGraphStarter>::State const & state)
  {
    if (segmentsData.count(state.vertex) != 0)
      return false;

    auto & parent = segmentsData[state.parent];
    if (parent.m_steps == kMaxStep)
      return false;

    auto & current = segmentsData[state.vertex];
    current.m_summaryETA = parent.m_summaryETA + m_starter.CalcSegmentETA(state.vertex);
    current.m_steps = parent.m_steps + 1;

    return true;
  });
}

LeapsPostProcessor::SegmentData::SegmentData(size_t steps, double eta)
    : m_steps(steps), m_summaryETA(eta) {}

LeapsPostProcessor::PathInterval::PathInterval(double weight, size_t left, size_t right,
                                               std::vector<Segment> && path)
  : m_winWeight(weight), m_left(left), m_right(right), m_path(std::move(path)) {}

bool LeapsPostProcessor::PathInterval::operator>(PathInterval const & rhs) const
{
  if (base::AlmostEqualAbs(m_winWeight, rhs.m_winWeight, kWeightEps))
    return m_right - m_left > rhs.m_right - rhs.m_left;

  return m_winWeight > rhs.m_winWeight;
}

bool LeapsPostProcessor::PathInterval::operator<(PathInterval const & rhs) const
{
  CHECK(m_left > rhs.m_right || m_right < rhs.m_left, ("Intervals shouldn't intersect."));

  return m_right < rhs.m_left;
}

}  // namespace routing
