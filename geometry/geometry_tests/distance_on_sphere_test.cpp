#include "testing/testing.hpp"

#include "geometry/distance_on_sphere.hpp"

#include "base/math.hpp"

namespace
{
double CalculateEarthAreaForConvexPolygon(std::vector<ms::LatLon> const & latlons)
{
  double area = 0.0;
  auto const & base = latlons.front();
  for (size_t i = 1; i < latlons.size() - 1; ++i)
    area += ms::AreaOnEarth(base, latlons[i], latlons[i + 1]);

  return area;
}
}  // namespace

UNIT_TEST(DistanceOnSphere)
{
  TEST_LESS(fabs(ms::DistanceOnSphere(0, -180, 0, 180)), 1.0e-6, ());
  TEST_LESS(fabs(ms::DistanceOnSphere(30, 0, 30, 360)), 1.0e-6, ());
  TEST_LESS(fabs(ms::DistanceOnSphere(-30, 23, -30, 23)), 1.0e-6, ());
  TEST_LESS(fabs(ms::DistanceOnSphere(90, 0, 90, 120)), 1.0e-6, ());
  TEST_LESS(fabs(ms::DistanceOnSphere(0, 0, 0, 180) - math::pi), 1.0e-6, ());
  TEST_LESS(fabs(ms::DistanceOnSphere(90, 0, -90, 120) - math::pi), 1.0e-6, ());
}

UNIT_TEST(DistanceOnEarth)
{
  TEST_LESS(fabs(ms::DistanceOnEarth(30, 0, 30, 180) * 0.001 - 13358), 1, ());
  TEST_LESS(fabs(ms::DistanceOnEarth(30, 0, 30, 45) * 0.001 - 4309), 1, ());
  TEST_LESS(fabs(ms::DistanceOnEarth(-30, 0, -30, 45) * 0.001 - 4309), 1, ());
  TEST_LESS(fabs(ms::DistanceOnEarth(47.37, 8.56, 53.91, 27.56) * 0.001 - 1519), 1, ());
  TEST_LESS(fabs(ms::DistanceOnEarth(43, 132, 38, -122.5) * 0.001 - 8302), 1, ());
}

UNIT_TEST(CircleAreaOnEarth)
{
  double const kEarthSurfaceArea = 4.0 * math::pi * ms::kEarthRadiusMeters * ms::kEarthRadiusMeters;
  TEST_ALMOST_EQUAL_ABS(ms::CircleAreaOnEarth(math::pi * ms::kEarthRadiusMeters),
                        kEarthSurfaceArea,
                        1e-1, ());

  TEST_ALMOST_EQUAL_ABS(ms::CircleAreaOnEarth(2.0 /* radiusMeters */),
                        math::pi * 2.0 * 2.0,
                        1e-2, ());

  TEST_ALMOST_EQUAL_ABS(ms::CircleAreaOnEarth(2000.0 /* radiusMeters */),
                        math::pi * 2000.0 * 2000.0,
                        1.0, ());
}

UNIT_TEST(AreaOnEarth_ThreePoints)
{
  double const kEarthSurfaceArea = 4.0 * math::pi * ms::kEarthRadiusMeters * ms::kEarthRadiusMeters;

  TEST_ALMOST_EQUAL_ABS(ms::AreaOnEarth({90.0, 0.0}, {0.0, 0.0}, {0.0, 90.0}),
                        kEarthSurfaceArea / 8.0,
                        1e-2, ());

  TEST_ALMOST_EQUAL_ABS(ms::AreaOnEarth({{90.0, 0.0}, {0.0, 0.0}, {0.0, 90.0}}),
                        kEarthSurfaceArea / 8.0,
                        1.0, ());

  TEST_ALMOST_EQUAL_ABS(ms::AreaOnEarth({{90.0, 0.0}, {0.0, 0.0}, {0.0, 90.0}}),
                        kEarthSurfaceArea / 8.0,
                        1.0, ());

  TEST_ALMOST_EQUAL_ABS(ms::AreaOnEarth({90.0, 0.0}, {0.0, 90.0}, {0.0, -90.0}),
                        kEarthSurfaceArea / 4.0,
                        1e-1, ());
}

UNIT_TEST(AngleOnEarth)
{
  TEST_ALMOST_EQUAL_ABS(ms::AngleOnEarth({0, 0}, {0, 90}, {90, 0}),
                        math::high_accuracy::pi2,
                        1e-5L, ());

  TEST_ALMOST_EQUAL_ABS(ms::AngleOnEarth({0, 45}, {90, 0}, {0, 0}),
                        math::high_accuracy::pi4,
                        1e-5L, ());
}

UNIT_TEST(AreaOnEarth_Convex_Polygon_1)
{
  auto const a = ms::LatLon(55.8034965, 37.696754);
  auto const b = ms::LatLon(55.7997909, 37.7830427);
  auto const c = ms::LatLon(55.8274225, 37.8150381);
  auto const d = ms::LatLon(55.843037, 37.7401892);
  auto const e = ms::LatLon(55.8452096, 37.7019668);

  std::vector<ms::LatLon> latlons = {a, b, c, d, e};
  double areaTriangulated =
      ms::AreaOnEarth(a, b, c) + ms::AreaOnEarth(a, c, d) + ms::AreaOnEarth(a, d, e);

  TEST(base::AlmostEqualRel(areaTriangulated,
                            ms::AreaOnEarth(latlons),
                            1e-6), ());
}

UNIT_TEST(AreaOnEarth_Convex_Polygon_2)
{
  std::vector<ms::LatLon> latlons = {
      {55.6348484, 36.025526},
      {55.0294112, 36.8959709},
      {54.9262448, 38.3719426},
      {55.3561515, 39.3275397},
      {55.7548279, 39.4458067},
      {56.3020039, 39.3322704},
      {56.5140099, 38.6368606},
      {56.768935, 37.0473526},
      {56.4330113, 35.6234183},
  };

  TEST(base::AlmostEqualRel(CalculateEarthAreaForConvexPolygon(latlons),
                            ms::AreaOnEarth(latlons),
                            1e-6), ());
}

UNIT_TEST(AreaOnEarth_Concave_Polygon_1)
{
  auto const a = ms::LatLon(55.9536583, 37.9671845);
  auto const b = ms::LatLon(55.920022, 37.9639131);
  auto const c = ms::LatLon(55.9163556, 38.0921987);
  auto const d = ms::LatLon(55.9578445, 38.0861232);
  auto const e = ms::LatLon(55.9379563, 38.0342482);
  std::vector<ms::LatLon> latlons = {a, b, c, d, e};

  double areaTriangulated =
      ms::AreaOnEarth(a, b, e) + ms::AreaOnEarth(b, c, e) + ms::AreaOnEarth(c, d, e);

  TEST(base::AlmostEqualRel(areaTriangulated,
                            ms::AreaOnEarth(latlons),
                            1e-6), ());
}

UNIT_TEST(AreaOnEarth_Concave_Polygon_2)
{
  auto const a = ms::LatLon(40.3429746, -7.6513617);
  auto const b = ms::LatLon(40.0226711, -7.7356029);
  auto const c = ms::LatLon(39.7887079, -7.0626206);
  auto const d = ms::LatLon(40.1038622, -7.0394143);
  auto const e = ms::LatLon(40.0759637, -6.7145263);
  auto const f = ms::LatLon(40.2861884, -6.8637096);
  auto const g = ms::LatLon(40.4175634, -6.5123);
  auto const h = ms::LatLon(40.4352289, -6.9101221);
  auto const i = ms::LatLon(40.6040786, -7.0825117);
  auto const j = ms::LatLon(40.4301821, -7.2482709);

  std::vector<ms::LatLon> latlons = {a, b, c, d, e, f, g, h, i, j};
  double areaTriangulated =
      ms::AreaOnEarth(a, b, c) +
      ms::AreaOnEarth(a, c, d) +
      ms::AreaOnEarth(a, d, f) +
      ms::AreaOnEarth(d, e, f) +
      ms::AreaOnEarth(a, f, j) +
      ms::AreaOnEarth(f, h, j) +
      ms::AreaOnEarth(f, g, h) +
      ms::AreaOnEarth(h, i, j);

  TEST(base::AlmostEqualRel(areaTriangulated,
                            ms::AreaOnEarth(latlons),
                            1e-6), ());
}
