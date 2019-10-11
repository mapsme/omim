#include "geometry/distance_on_sphere.hpp"

#include "geometry/point3d.hpp"
#include "geometry/mercator.hpp"

#include "base/assert.hpp"
#include "base/math.hpp"

#include <algorithm>
#include <cmath>

using namespace std;

namespace
{
double const kEarthRadiusMetersSquared = ms::kEarthRadiusMeters * ms::kEarthRadiusMeters;
}  // namespace

namespace ms
{
long double DistanceOnSphere(double lat1Deg, double lon1Deg, double lat2Deg, double lon2Deg)
{
  long double const lat1 = base::DegToRad(lat1Deg);
  long double const lat2 = base::DegToRad(lat2Deg);
  long double const dlat = sin((lat2 - lat1) * 0.5);
  long double const dlon = sin((base::DegToRad(lon2Deg) - base::DegToRad(lon1Deg)) * 0.5);
  long double const y = dlat * dlat + dlon * dlon * cos(lat1) * cos(lat2);
  return 2.0 * atan2(sqrt(y), sqrt(max(0.0l, 1.0 - y)));
}

double DistanceOnEarth(double lat1Deg, double lon1Deg, double lat2Deg, double lon2Deg)
{
  return kEarthRadiusMeters * DistanceOnSphere(lat1Deg, lon1Deg, lat2Deg, lon2Deg);
}

double DistanceOnEarth(LatLon const & ll1, LatLon const & ll2)
{
  return DistanceOnEarth(ll1.m_lat, ll1.m_lon, ll2.m_lat, ll2.m_lon);
}

m3::PointD GetPointOnSphere(ms::LatLon const & ll, double sphereRadius)
{
  ASSERT(LatLon::kMinLat <= ll.m_lat && ll.m_lat <= LatLon::kMaxLat, (ll));
  ASSERT(LatLon::kMinLon <= ll.m_lon && ll.m_lon <= LatLon::kMaxLon, (ll));

  // The point (lat=0, lon=0)   translates to (x=1, y=0, z=0).
  // The point (lat=0, lon=+90°) translates to (x=0, y=1, z=0).
  // The point (lat=+90°, lon=0) translates to (x=0, y=0, z=1).

  double const latRad = base::DegToRad(ll.m_lat);
  double const lonRad = base::DegToRad(ll.m_lon);

  double const x = sphereRadius * cos(latRad) * cos(lonRad);
  double const y = sphereRadius * cos(latRad) * sin(lonRad);
  double const z = sphereRadius * sin(latRad);

  return {x, y, z};
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
  return abs(area);
}

// Look to https://en.wikipedia.org/wiki/Solution_of_triangles (Three sides given (spherical SSS).
// Shorlty: we calculate angle ∠(lla, llb, llc) with spherical law of cosines.
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

// Look to http://mathworld.wolfram.com/SphericalPolygon.html for details.
// Shortly:
// We use such formula: S = (θ - (n - 2)π) * R^2
// Where θ - sum of polygon's angles, R - earth radius, n - number of polygon's vertexes.
double AreaOnEarth(std::vector<LatLon> const & latlons)
{
  auto const prevIndex = [n = latlons.size()](size_t i) {
    return i == 0 ? n - 1 : i - 1;
  };

  auto const nextIndex = [n = latlons.size()](size_t i) {
    return i == n - 1 ? 0 : i + 1;
  };

  long double anglesSum = 0.0;
  for (size_t i = 0; i < latlons.size(); i++)
  {
    auto const & prev = latlons[prevIndex(i)];
    auto const & cur = latlons[i];
    auto const & next = latlons[nextIndex(i)];

    anglesSum += AngleOnEarth(prev, cur, next);
  }

  long double const eps =
      anglesSum - static_cast<long double>(latlons.size() - 2) * math::high_accuracy::pi;
  return static_cast<double>(eps * kEarthRadiusMetersSquared);
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
