#include "testing/testing.hpp"

#include "routing/fake_graph.hpp"
#include "routing/segment.hpp"

#include "geometry/point2d.hpp"

#include "base/assert.hpp"
#include "base/logging.hpp"
#include "base/timer.hpp"

#include <algorithm>
#include <cstdint>

using namespace routing;
using namespace std;

namespace
{
NumMwmId constexpr kDummyMwmId = 20;

// Finds Segments in graph for measuring total search time.
void FindNodesTest(FakeGraph<Segment, m2::PointD> const & graph, uint32_t maxIndex,
                   uint32_t iterCount)
{
  for (uint32_t i = 1; i < iterCount; ++i)
  {
    uint32_t const id = i < maxIndex ? i : i % maxIndex + 1;
    auto volatile res = graph.GetVertex({kDummyMwmId, id, id, true /* forward */});
  }
}

// Builds fake graph in the shape of the "square seed".
FakeGraph<Segment, m2::PointD> ConstructSieveGraph(uint32_t startSegmentIndex,
                                                   uint32_t sideOfSquarePoints,
                                                   m2::PointD const & leftLowerPoint)
{
  CHECK_GREATER(sideOfSquarePoints, 1, ());
  FakeGraph<Segment, m2::PointD> fakeGraph;
  auto curId = startSegmentIndex;
  auto const maxSegmentIndex = startSegmentIndex + sideOfSquarePoints;
  bool isFirstRow = true;

  for (auto i = startSegmentIndex; i < maxSegmentIndex; ++i)
  {
    bool isFirstCol = true;
    for (auto j = startSegmentIndex; j < maxSegmentIndex; ++j, ++curId)
    {
      Segment cur(kDummyMwmId, curId, curId, true /* forward */);
      m2::PointD const coord(leftLowerPoint.x + i, leftLowerPoint.y + j);
      if (isFirstCol)
      {
        fakeGraph.AddStandaloneVertex(cur, coord);
        isFirstCol = false;
      }
      else
      {
        auto const prevId = curId - 1;
        Segment prev(kDummyMwmId, prevId, prevId, true /* forward */);
        fakeGraph.AddVertex(prev, cur, coord, true /* isOutgoing */);
        fakeGraph.AddConnection(cur, prev);
      }
      if (!isFirstRow)
      {
        Segment lower(kDummyMwmId, curId - sideOfSquarePoints, curId - sideOfSquarePoints, true /* forward */);
        fakeGraph.AddConnection(cur, lower);
        fakeGraph.AddConnection(lower, cur);
      }
    }
    isFirstRow = false;
  }

  return fakeGraph;
}

// Builds fake graph and calculates the performance of insert/find operations.
UNIT_TEST(HugeFakeGraphTest)
{
  for (auto sideOfSquare : {5, 10, 32, 60, 100, 250, 500, 750, 1000})
  {
    base::HighResTimer timerStart;

    auto const fakeGraph = ConstructSieveGraph(1, sideOfSquare, {1.0, 1.0});

    auto const totalSize = sideOfSquare * sideOfSquare;
    CHECK_EQUAL(fakeGraph.GetSize(), totalSize, ());

    auto const intermediatePoint = timerStart.ElapsedMillis();

    uint32_t constexpr iterCount = 10000;  // Count of cycles for averaging the result time.
    FindNodesTest(fakeGraph, totalSize, iterCount);

    LOG(LINFO, (totalSize, "nodes in fake graph. Construction duration:", intermediatePoint, "ms.",
                iterCount, "iterations of search duration:",
                timerStart.ElapsedMillis() - intermediatePoint, "ms."));
  }
}
}  // namespace
