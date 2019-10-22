#include "testing/testing.hpp"

#include "geometry/area_on_earth.hpp"
#include "geometry/distance_on_sphere.hpp"
#include "geometry/mercator.hpp"

#include "base/math.hpp"

#include <fstream>
#include <iomanip>
#include <iostream>

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


std::vector<m2::PointD> FromLatLons(std::vector<ms::LatLon> const & latlons)
{
  std::vector<m2::PointD> points;
  for (auto const & latlon : latlons)
    points.emplace_back(MercatorBounds::FromLatLon(latlon));
  return points;
}
}  // namespace

UNIT_TEST(AreaOnEarth_Details_DropUniquePoints)
{
  auto const a = m2::PointD(10.025, 110.333);
  auto const b = m2::PointD(11.222, 130.333);
  auto const c = m2::PointD(12.025, 150.333);
  auto const d = m2::PointD(13.025, 160.123);

  {
    std::vector<m2::PointD> points = {a, b, c, a, d, d, b, a};
    points = ms::area_on_sphere_details::DropUniquePoints(points);

    std::vector<m2::PointD> const answer = {a, b, c, d};
    TEST_EQUAL(points, answer, ());
  }

  {
    std::vector<m2::PointD> points = {a, b, c, d};
    points = ms::area_on_sphere_details::DropUniquePoints(points);

    std::vector<m2::PointD> const answer = {a, b, c, d};
    TEST_EQUAL(points, answer, ());
  }

  {
    std::vector<m2::PointD> points = {a, b, c, a};
    points = ms::area_on_sphere_details::DropUniquePoints(points);

    std::vector<m2::PointD> const answer = {a, b, c};
    TEST_EQUAL(points, answer, ());
  }
}

UNIT_TEST(AreaOnEarth_Details_AbleToAddWithoutSelfIntersections)
{
  auto const a = m2::PointD(1.0, 1.0);
  auto const b = m2::PointD(10.0, 1.0);
  auto const c = m2::PointD(10.0, 10.0);
  auto const d = m2::PointD(5.0, 5.0);  // segment: c-d doesn't intersect a-b
  auto const e = m2::PointD(12.0, 6.0);  // segment d-e intersects b-c

  TEST(ms::area_on_sphere_details::AbleToAddWithoutSelfIntersections({a, b, c}, d), ());
  TEST(!ms::area_on_sphere_details::AbleToAddWithoutSelfIntersections({a, b, c, d}, e), ());
}

UNIT_TEST(AreaOnEarth_Details_DropClosePoints_1)
{
  auto const a = m2::PointD(1.0, 1.0);
  auto const b = m2::PointD(2.0, 3.0);
  auto const c = m2::PointD(3.0, 4.0);
  auto const d = m2::PointD(4.0, 3.0);
  auto const e = m2::PointD(3.0, 0.0);
  auto const f = m2::PointD(2.0, 0.0);

  std::vector<m2::PointD> const points = {a, b, c, d, e, f};
  auto dropped =
      ms::area_on_sphere_details::DropClosePoints(points, MercatorBounds::DistanceOnEarth(a, b) + 1.0);
  std::vector<m2::PointD> answer = {a, c, e};
  TEST_EQUAL(dropped, answer, ());

  dropped =
      ms::area_on_sphere_details::DropClosePoints(points, 1.0);
  answer = {a, b, c, d, e, f};
  TEST_EQUAL(dropped, answer, ());
}

UNIT_TEST(AreaOnEarth_Details_DropClosePoints_2)
{
  auto const a = m2::PointD(4.0, 4.0);
  auto const b = m2::PointD(2.0, 2.0);
  auto const c = m2::PointD(6.0, 1.0);
  auto const d = m2::PointD(12.0, 2.0);
  auto const e = m2::PointD(6.0, 2.0);
  auto const f = m2::PointD(8.0, 1.0); // e-f intersects c-d
  auto const g = m2::PointD(12.0, -2.0);

  std::vector<m2::PointD> points = {a, b, c, d, e, f, g};
  points = ms::area_on_sphere_details::DropClosePoints(points, 1.0 /* intervalMeters */);
  std::vector<m2::PointD> answer = {a, b, c, d, f, g};
  TEST_EQUAL(points, answer, ());
}

UNIT_TEST(AreaOnEarth_Details_DropClosePoints_3)
{
  auto const a = m2::PointD(2.0, 7.0);
  auto const b = m2::PointD(2.0, 1.0);
  auto const c = m2::PointD(10.0, 1.0);
  auto const d = m2::PointD(10.0, 7.0);
  auto const e = m2::PointD(6.0, 4.0);
  auto const f = m2::PointD(11.0, 4.0); // e-f intersects c-d and |f| is too close to |d| and too close to |c|
  auto const g = m2::PointD(12.0, 4.0);
  auto const h = m2::PointD(20.0, 4.0);

  double const deDist = MercatorBounds::DistanceOnEarth(d, e) - 1.0;
  std::vector<m2::PointD> points = {a, b, c, d, e, f, g, h};
  points = ms::area_on_sphere_details::DropClosePoints(points, deDist);
  std::vector<m2::PointD> answer = {a, b, f, h};
  TEST_EQUAL(points, answer, ());
}

UNIT_TEST(AreaOnEarth_Details_DropClosePoints_4)
{
  auto const a = m2::PointD(2.0, 6.0);
  auto const b = m2::PointD(2.0, 1.0);
  auto const c = m2::PointD(7.0, 1.0);
  auto const d = m2::PointD(7.0, 6.0);
  auto const e = m2::PointD(4.0, 6.0); // far from |d|, but close to |a|

  double const deDist = MercatorBounds::DistanceOnEarth(d, e) - 1.0;
  std::vector<m2::PointD> points = {a, b, c, d, e};
  points = ms::area_on_sphere_details::DropClosePoints(points, deDist);
  std::vector<m2::PointD> answer = {a, b, c, d};
  TEST_EQUAL(points, answer, ());
}

UNIT_TEST(AreaOnEarth_Details_DropAlmostAtOneLinePointsEps_1)
{
  auto const a = m2::PointD(0.0, 0.0);
  auto const b = m2::PointD(1.0, 0.0);
  auto const c = m2::PointD(2.0, 0.0);

  std::vector<m2::PointD> points = {a, b, c};
  std::vector<m2::PointD> answer = {a, c};
  TEST_EQUAL(answer, ms::area_on_sphere_details::DropAlmostAtOneLinePointsEps(points, 1e-3), ());
}

UNIT_TEST(AreaOnEarth_Details_DropAlmostAtOneLinePointsEps_2)
{
  auto const a = m2::PointD(1.0, 1.0);
  auto const b = m2::PointD(4.0, 1.0);
  auto const c = m2::PointD(7.0, 2.0);
  auto const d = m2::PointD(9.0, 5.0);
  auto const e = m2::PointD(10.0, 8.0);

  std::vector<m2::PointD> points = {a, b, c, d, e};
  std::vector<m2::PointD> answer = {a, c, e};
  double const kEpsRad = asin(1.0 / sqrt(10.0));
  TEST_EQUAL(answer, ms::area_on_sphere_details::DropAlmostAtOneLinePointsEps(points, kEpsRad), ());
}

UNIT_TEST(AreaOnEarth_Details_DropAlmostAtOneLinePointsEps_3)
{
  auto const a = m2::PointD(1.0, 1.0);
  auto const b = m2::PointD(4.0, 1.0);
  auto const c = m2::PointD(7.0, 2.0);
  auto const d = m2::PointD(9.0, 5.0);
  auto const e = m2::PointD(10.0, 8.0);
  auto const f = m2::PointD(8.0, 10.0);
  auto const g = m2::PointD(3.0, 8.0);
  auto const h = m2::PointD(2.0, 4.0);

  std::vector<m2::PointD> points = {a, b, c, d, e, f, g, h};
  std::vector<m2::PointD> answer = {a, c, e, f, g};
  double const kEpsRad = asin(1.0 / sqrt(10.0));
  TEST_EQUAL(answer, ms::area_on_sphere_details::DropAlmostAtOneLinePointsEps(points, kEpsRad), ());
}

UNIT_TEST(AreaOnEarth_Circle)
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

  TEST_ALMOST_EQUAL_ABS(ms::AreaOnEarth({90.0, 0.0}, {0.0, 90.0}, {0.0, -90.0}),
                        kEarthSurfaceArea / 4.0,
                        1e-1, ());
}

UNIT_TEST(AngleOnEarth)
{
  TEST_ALMOST_EQUAL_ABS(ms::area_on_sphere_details::AngleOnEarth({0, 0}, {0, 90}, {90, 0}),
                        math::high_accuracy::pi2,
                        1e-5L, ());

  TEST_ALMOST_EQUAL_ABS(ms::area_on_sphere_details::AngleOnEarth({0, 45}, {90, 0}, {0, 0}),
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

  std::vector<ms::LatLon> const latlons = {a, b, c, d, e};
  std::vector<m2::PointD> const points = FromLatLons(latlons);

  double areaTriangulated =
      ms::AreaOnEarth(a, b, c) + ms::AreaOnEarth(a, c, d) + ms::AreaOnEarth(a, d, e);

  TEST(base::AlmostEqualRel(areaTriangulated,
                            ms::AreaOnEarth(points),
                            1e-6), ());
}

UNIT_TEST(AreaOnEarth_Convex_Polygon_2)
{
  std::vector<ms::LatLon> const latlons = {
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

  std::vector<m2::PointD> const points = FromLatLons(latlons);

  TEST(base::AlmostEqualRel(CalculateEarthAreaForConvexPolygon(latlons),
                            ms::AreaOnEarth(points),
                            1e-6), ());
}

UNIT_TEST(AreaOnEarth_Concave_Polygon_1)
{
  auto const a = ms::LatLon(55.9536583, 37.9671845);
  auto const b = ms::LatLon(55.920022, 37.9639131);
  auto const c = ms::LatLon(55.9163556, 38.0921987);
  auto const d = ms::LatLon(55.9578445, 38.0861232);
  auto const e = ms::LatLon(55.9379563, 38.0342482);

  std::vector<ms::LatLon> const latlons = {a, b, c, d, e};
  std::vector<m2::PointD> const points = FromLatLons(latlons);

  double areaTriangulated =
      ms::AreaOnEarth(a, b, e) + ms::AreaOnEarth(b, c, e) + ms::AreaOnEarth(c, d, e);

  TEST(base::AlmostEqualRel(areaTriangulated,
                            ms::AreaOnEarth(points),
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

  std::vector<ms::LatLon> const latlons = {a, b, c, d, e, f, g, h, i, j};
  std::vector<m2::PointD> const points = FromLatLons(latlons);

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
                            ms::AreaOnEarth(points),
                            1e-6), ());
}

UNIT_TEST(AreaOnEarth_WithFiltering)
{
  auto const a = ms::LatLon(-11.5135041, 33.9359112);
  auto const b = ms::LatLon(-11.4664859, 33.9669364);
  auto const c = ms::LatLon(-11.4179594, 33.9985555);
  auto const d = ms::LatLon(-11.3461837, 33.9852436);
  auto const e = ms::LatLon(-11.4183431, 33.9057636);
  auto const f = ms::LatLon(-11.4912516, 33.8854042);

  std::vector<ms::LatLon> const latlons = {a, b, c, d, e, f};
  std::vector<m2::PointD> const points = FromLatLons(latlons);

  std::vector<ms::LatLon> const latlonsAfterFilter = {a, c, d, e, f};

  TEST_ALMOST_EQUAL_ABS(CalculateEarthAreaForConvexPolygon(latlonsAfterFilter),
                        ms::AreaOnEarth(points),
                        1.0, ());
}

UNIT_TEST(AreaOnEarth_FromFile)
{
  CHECK(std::getenv("FILE_PATH") && !std::string(std::getenv("FILE_PATH")).empty(), ());
  std::string dirPath = std::string(std::getenv("FILE_PATH"));

  for (size_t i = 0; i < 34492; i++)
  {
    if (i % 1000 == 0)
      LOG(LINFO, (i));
    std::string path = dirPath + "/" + std::to_string(i) + ".points";
    std::ifstream input(path);
    std::vector<m2::PointD> points;
    size_t n;
    input >> n;
    points.resize(n);
    for (size_t i = 0; i < n; ++i)
    {
      double lat, lon;
      input >> lat >> lon;
      points[i] = MercatorBounds::FromLatLon(lat, lon);
    }

    double area = ms::AreaOnEarth(points, 1);
//    std::cout << std::setprecision(20);
//    std::cout << area << std::endl;
    CHECK_GREATER(area, 0, ());
  }
}