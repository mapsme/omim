#include "testing/testing.hpp"

#include "routing/road_graph.hpp"

#include "indexer/feature_altitude.hpp"

#include "geometry/point2d.hpp"

#include "base/math.hpp"

#include "std/vector.hpp"

using namespace routing;

namespace
{
vector<Junction> const kPath = {{{0, 0}, 0}, {{0, 1}, 1}, {{1, 1}, 2}};
double constexpr kEpsilon = 10.0;

UNIT_TEST(JunctionsToPointsMTest)
{
  vector<m2::PointD> path;
  JunctionsToPoints(kPath, path);
  vector<m2::PointD> const expectedPath = {{0, 0}, {0, 1}, {1, 1}};
  TEST_EQUAL(path, expectedPath, ());
}

UNIT_TEST(JunctionsToAltitudesMTest)
{
  feature::TAltitudes altitudes;
  JunctionsToAltitudes(kPath, altitudes);
  feature::TAltitudes const expectedAltitudes = {0, 1, 2};
  TEST_EQUAL(altitudes, expectedAltitudes, ());
}

UNIT_TEST(PathLengthMTest)
{
  TEST(my::AlmostEqualAbs(PathLengthM(kPath), 222612.0 /* meters */, kEpsilon), ());
}
}  // namespace
