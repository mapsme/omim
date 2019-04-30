#include "testing/testing.hpp"

#include "coding/not_intersecting_intervals.hpp"

using namespace coding;

UNIT_TEST(NotIntersectingIntervals_1)
{
  NotIntersectingIntervals intervals;

  TEST(intervals.AddInterval(1, 10), ());
  TEST(intervals.AddInterval(11, 15), ());
  // overlap with [1, 10]
  TEST(!intervals.AddInterval(1, 20), ());

  // overlap with [11, 15]
  TEST(!intervals.AddInterval(13, 20), ());

  // overlap with [1, 10] and [11, 15]
  TEST(!intervals.AddInterval(0, 100), ());

  TEST(intervals.AddInterval(100, 150), ());

  // overlap with [100, 150]
  TEST(!intervals.AddInterval(90, 200), ());
}

UNIT_TEST(NotIntersectingIntervals_2)
{
  NotIntersectingIntervals intervals;

  TEST(intervals.AddInterval(1, 10), ());
  // overlap with [1, 10]
  TEST(!intervals.AddInterval(2, 9), ());
}

UNIT_TEST(NotIntersectingIntervals_3)
{
  NotIntersectingIntervals intervals;

  TEST(intervals.AddInterval(1, 10), ());
  // overlap with [1, 10]
  TEST(!intervals.AddInterval(0, 20), ());
}

UNIT_TEST(NotIntersectingIntervals_4)
{
  NotIntersectingIntervals intervals;

  TEST(intervals.AddInterval(1, 10), ());
  // overlap with [1, 10]
  TEST(!intervals.AddInterval(10, 20), ());
  TEST(!intervals.AddInterval(0, 1), ());
}
