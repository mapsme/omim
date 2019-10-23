#include "geometry/area_on_earth.hpp"

#include "geometry/convex_hull.hpp"
#include "geometry/distance_on_sphere.hpp"
#include "geometry/mercator.hpp"
#include "geometry/point3d.hpp"
#include "geometry/polygon.hpp"
#include "geometry/segment2d.hpp"

#include "base/assert.hpp"

#include <algorithm>
#include <iomanip>
#include <fstream>
#include <cmath>

namespace
{
double const kEarthRadiusMetersSquared = ms::kEarthRadiusMeters * ms::kEarthRadiusMeters;
}  // namespace

namespace ms
{
namespace area_on_sphere_details
{
// Look to https://en.wikipedia.org/wiki/Solution_of_triangles (Three sides given (spherical SSS)).
// Shortly: we calculate angle ∠(lla, llb, llc) with spherical law of cosines.
long double AngleOnEarth(LatLon const & lla, LatLon const & llb, LatLon const & llc)
{
  auto const pa = MercatorBounds::FromLatLon(lla);
  auto const pb = MercatorBounds::FromLatLon(llb);
  auto const pc = MercatorBounds::FromLatLon(llc);

  m2::Point<long double> vec1(pb.x - pa.x, pb.y - pa.y);
  m2::Point<long double> vec2(pc.x - pb.x, pc.y - pb.y);

  bool const isConcaveAngle = m2::CrossProduct(vec1, vec2) < 0;

  long double const a = DistanceOnSphere(llb.m_lat, llb.m_lon, llc.m_lat, llc.m_lon);
  long double const b = DistanceOnSphere(llc.m_lat, llc.m_lon, lla.m_lat, lla.m_lon);
  long double const c = DistanceOnSphere(lla.m_lat, lla.m_lon, llb.m_lat, llb.m_lon);

  long double cosValue = (cos(b) - cos(c) * cos(a)) / (sin(c) * sin(a));
  cosValue = base::Clamp(cosValue, -1.0L, 1.0L);

  long double betta = acos(cosValue);
  if (isConcaveAngle)
    betta = 2.0L * math::high_accuracy::pi - betta;
  return betta;
}

std::vector<m2::PointD> DropUniquePoints(std::vector<m2::PointD> const & points)
{
  std::unordered_set<m2::PointD, m2::PointD::Hash> used;
  std::vector<m2::PointD> result;
  for (auto const & point : points)
  {
    if (used.count(point) == 0)
      result.emplace_back(point);

    used.emplace(point);
  }

  return result;
}

bool AbleToAddWithoutSelfIntersections(std::vector<m2::PointD> const & polyline,
                                       m2::PointD const & newPoint)
{
  if (polyline.size() < 3)
    return true;

  m2::PointD const & prevPoint = polyline.back();
  for (size_t i = 0; i < polyline.size() - 2; ++i)
  {
    if (m2::SegmentsIntersect(polyline[i], polyline[i + 1], prevPoint, newPoint))
      return false;
  }

  return true;
}

std::vector<m2::PointD> DropClosePoints(std::vector<m2::PointD> const & points,
                                        double intervalMeters)
{
  if (points.size() < 2)
    return points;

  std::vector<m2::PointD> result;
  result.emplace_back(points[0]);

  for (size_t i = 1; i < points.size(); ++i)
  {
    bool mustAdd = false;
    while (!result.empty())
    {
      auto const & point = points[i];
      double const distFromLastAddedToCurM = MercatorBounds::DistanceOnEarth(result.back(), point);
      if (distFromLastAddedToCurM < intervalMeters)
      {
        if (mustAdd)
        {
          result.pop_back();
          continue;
        }
        else
        {
          break;
        }
      }

      mustAdd = true;
      if (AbleToAddWithoutSelfIntersections(result, point))
      {
        result.emplace_back(point);
        break;
      }
      else
      {
        result.pop_back();
      }
    }
  }

  if (result.size() > 2)
  {
    double distFromLastAndFirstPoint =
        MercatorBounds::DistanceOnEarth(result.back(), result.front());

    while (distFromLastAndFirstPoint < intervalMeters && result.size() > 2)
    {
      result.pop_back();
      distFromLastAndFirstPoint = MercatorBounds::DistanceOnEarth(result.back(), result.front());
    }
  }

  return result;
}

bool AlmostAtOneLineEps(m2::PointD const & p1, m2::PointD const & p2, m2::PointD const & p3,
                        double epsRad)
{
  auto a = p2 - p1;
  auto b = p3 - p2;
  a = a.Normalize();
  b = b.Normalize();
  double const oneLineCondition = std::abs(m2::CrossProduct(a, b));
  return std::abs(oneLineCondition) < epsRad;
}

std::vector<m2::PointD> DropAlmostAtOneLinePointsEps(std::vector<m2::PointD> const & points,
                                                     double epsRad)
{
  std::unordered_set<size_t> dropped;
  size_t pointsLeft = points.size();
  auto const nextNotDroppedIdx = [n = points.size(), &dropped](size_t i) {
    ++i;
    for (size_t k = 0; k < n; ++k)
    {
      size_t const idx = (i + k) % n;
      if (dropped.count(idx) == 0)
        return idx;
    }
    UNREACHABLE();
  };


  size_t prevIdx = 0;
  size_t curIdx = prevIdx + 1;
  size_t nextIdx = curIdx + 1;
  for (size_t i = 0; i < points.size(); ++i)
  {
    if (pointsLeft < 3)
      break;

    if (AlmostAtOneLineEps(points[prevIdx], points[curIdx], points[nextIdx], epsRad))
    {
      --pointsLeft;
      dropped.emplace(curIdx);
    }
    else
      prevIdx = curIdx;

    curIdx = nextIdx;
    nextIdx = nextNotDroppedIdx(nextIdx);
  }

  std::vector<m2::PointD> result;
  for (size_t i = 0; i < points.size(); ++i)
  {
    if (dropped.count(i) == 0)
      result.emplace_back(points[i]);
  }

  return result;
}

// Look to http://mathworld.wolfram.com/SphericalPolygon.html for details.
// Shortly:
// We use such formula: S = (θ - (n - 2)π) * R^2
// Where θ - sum of polygon's angles, R - earth radius, n - number of polygon's vertexes.
double AreaOnEarth(std::vector<LatLon> const & latlons)
{
  auto const prevIndex = [n = latlons.size()](size_t i) { return i == 0 ? n - 1 : i - 1; };
  auto const nextIndex = [n = latlons.size()](size_t i) { return i == n - 1 ? 0 : i + 1; };

  long double anglesSum = 0.0;
  for (size_t i = 0; i < latlons.size(); i++)
  {
    auto const & prev = latlons[prevIndex(i)];
    auto const & cur = latlons[i];
    auto const & next = latlons[nextIndex(i)];

    anglesSum += area_on_sphere_details::AngleOnEarth(prev, cur, next);
  }

  long double const eps =
      anglesSum - static_cast<long double>(latlons.size() - 2) * math::high_accuracy::pi;
  return static_cast<double>(eps * kEarthRadiusMetersSquared);
}

double AreaOnEarthWithConvexHull(std::vector<m2::PointD> const & points)
{
  if (points.size() < 3)
    return 0.0;

  double constexpr kConvexHullEps = 1e-9;
  m2::ConvexHull convexHull(points, kConvexHullEps);
  auto const & base = convexHull.Points().front();
  double area = 0.0;
  for (size_t i = 1; i < convexHull.Points().size() - 1; ++i)
  {
    auto const ll1 = MercatorBounds::ToLatLon(base);
    auto const ll2 = MercatorBounds::ToLatLon(convexHull.Points()[i]);
    auto const ll3 = MercatorBounds::ToLatLon(convexHull.Points()[i + 1]);

    area += ms::AreaOnEarth(ll1, ll2, ll3);
  }

  return area;
}
}  // namespace area_on_sphere_details

uint64_t GetId()
{
  bool hasId = std::getenv("DEBUG") && !std::string(std::getenv("DEBUG")).empty();
  if (!hasId)
    return 0;

  uint64_t id;
  std::stringstream ss;
  ss << std::string(std::getenv("DEBUG"));
  ss >> id;
  return id;
}

void PushDebugLine(std::vector<m2::PointD> const & points)
{
  std::ofstream point_fh("/tmp/cpp_points", std::ofstream::app);
  point_fh << std::setprecision(20);
  point_fh << points.size() + 1 << std::endl;
  for (auto const & point : points)
  {
    auto const latlon = MercatorBounds::ToLatLon(point);
    point_fh << latlon.m_lat << " " << latlon.m_lon << std::endl;
  }

  auto const latlon = MercatorBounds::ToLatLon(points.front());
  point_fh << latlon.m_lat << " " << latlon.m_lon << std::endl;
}

double AreaOnEarth(std::vector<m2::PointD> const & points, uint64_t id)
{
  std::vector<m2::PointD> pointsCopy = area_on_sphere_details::DropUniquePoints(points);
  size_t beforeFilter = 0;
  size_t afterFilter = 0;
  do
  {
    beforeFilter = pointsCopy.size();
    double constexpr kIntervalBetweenPointsMeters = 2000.0;
    double constexpr kAlmostOneLineEpsRad = 0.01;
    pointsCopy = area_on_sphere_details::DropClosePoints(pointsCopy, kIntervalBetweenPointsMeters);
    pointsCopy = area_on_sphere_details::DropAlmostAtOneLinePointsEps(pointsCopy, kAlmostOneLineEpsRad);
    afterFilter = pointsCopy.size();
  } while (beforeFilter != afterFilter);

  static uint64_t debugId = GetId();
  if (id != 0 && id == debugId)
  {
    PushDebugLine(pointsCopy);
  }

  if (pointsCopy.size() > 4)
  {
    if (!IsPolygonCCW(pointsCopy.begin(), pointsCopy.end()))
      std::reverse(pointsCopy.begin(), pointsCopy.end());

    std::vector<ms::LatLon> latlons;
    latlons.reserve(pointsCopy.size());
    for (auto const & point : pointsCopy)
      latlons.emplace_back(MercatorBounds::ToLatLon(point));

    return area_on_sphere_details::AreaOnEarth(latlons);
  }

  return area_on_sphere_details::AreaOnEarthWithConvexHull(points);
}

// Look to https://en.wikipedia.org/wiki/Solid_angle for more details.
// Shortly:
// It's possible to calculate area of triangle on sphere with it's solid angle.
// Ω = A / R^2
// Where Ω is solid angle of triangle, R - sphere radius and A - area of triangle on sphere.
// So A = Ω * R^2
double AreaOnEarth(LatLon const & ll1, LatLon const & ll2, LatLon const & ll3)
{
  m3::PointD const a = GetPointOnSphere(ll1, 1.0 /* sphereRadius */);
  m3::PointD const b = GetPointOnSphere(ll2, 1.0 /* sphereRadius */);
  m3::PointD const c = GetPointOnSphere(ll3, 1.0 /* sphereRadius */);

  double const triple = m3::DotProduct(a, m3::CrossProduct(b, c));

  ASSERT(base::AlmostEqualAbs(a.Length(), 1.0, 1e-5), ());
  ASSERT(base::AlmostEqualAbs(b.Length(), 1.0, 1e-5), ());
  ASSERT(base::AlmostEqualAbs(c.Length(), 1.0, 1e-5), ());

  double constexpr lengthMultiplication = 1.0;  // a.Length() * b.Length() * c.Length()
  double const abc = m3::DotProduct(a, b);  // * c.Length() == 1
  double const acb = m3::DotProduct(a, c);  // * b.Length() == 1
  double const bca = m3::DotProduct(b, c);  // * a.Length() == 1

  double const tanFromHalfSolidAngle = triple / (lengthMultiplication + abc + acb + bca);
  double const halfSolidAngle = atan(tanFromHalfSolidAngle);
  double const solidAngle = halfSolidAngle * 2.0;
  double const area = solidAngle * kEarthRadiusMetersSquared;
  return std::abs(area);
}

// Look to https://en.wikipedia.org/wiki/Solid_angle for details.
// Shortly:
// Ω = A / R^2
// A is the spherical surface area which confined by solid angle.
// R - sphere radius.
// For circle: Ω = 2π(1 - cos(θ / 2)), where θ - is the cone apex angle.
double CircleAreaOnEarth(double distanceOnSphereRadius)
{
  double const theta = 2.0 * distanceOnSphereRadius / ms::kEarthRadiusMeters;
  static double const kConst = 2 * math::pi * kEarthRadiusMetersSquared;
  double const sinValue = sin(theta / 4);
  // 1 - cos(θ / 2) = 2sin^2(θ / 4)
  return kConst * 2 * sinValue * sinValue;
}
}  // namespace ms

