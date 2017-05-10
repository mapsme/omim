#include "testing/testing.hpp"

#include "indexer/indexer_tests/test_polylines.hpp"

#include "indexer/cell_id.hpp"

#include "coding/point_to_integer.hpp"

#include "base/logging.hpp"

#include <cmath>
#include <utility>

namespace
{
double const g_eps = MercatorBounds::GetCellID2PointAbsEpsilon();
uint32_t const g_coordBits = POINT_COORD_BITS;

void CheckEqualPoints(m2::PointD const & p1, m2::PointD const & p2)
{
  TEST(p1.EqualDxDy(p2, g_eps), (p1, p2));

  TEST_GREATER_OR_EQUAL(p1.x, -180.0, ());
  TEST_GREATER_OR_EQUAL(p1.y, -180.0, ());
  TEST_LESS_OR_EQUAL(p1.x, 180.0, ());
  TEST_LESS_OR_EQUAL(p1.y, 180.0, ());

  TEST_GREATER_OR_EQUAL(p2.x, -180.0, ());
  TEST_GREATER_OR_EQUAL(p2.y, -180.0, ());
  TEST_LESS_OR_EQUAL(p2.x, 180.0, ());
  TEST_LESS_OR_EQUAL(p2.y, 180.0, ());
}
}

UNIT_TEST(PointToInt64_DataSet1)
{
  for (size_t i = 0; i < ARRAY_SIZE(index_test::arr1); ++i)
  {
    m2::PointD const pt(index_test::arr1[i].x, index_test::arr1[i].y);
    int64_t const id = PointToInt64(pt, g_coordBits);
    m2::PointD const pt1 = Int64ToPoint(id, g_coordBits);

    CheckEqualPoints(pt, pt1);

    int64_t const id1 = PointToInt64(pt1, g_coordBits);
    TEST_EQUAL(id, id1, (pt, pt1));
  }
}
