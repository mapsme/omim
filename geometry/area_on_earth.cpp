#include "geometry/area_on_earth.hpp"

#include "geometry/distance_on_sphere.hpp"
#include "geometry/point3d.hpp"

#include "base/assert.hpp"

#include <algorithm>
#include <cmath>

namespace
{
double const kEarthRadiusMetersSquared = ms::kEarthRadiusMeters * ms::kEarthRadiusMeters;

m3::PointD GetPointOnSphere(ms::LatLon const & ll, double sphereRadius)
{
  ASSERT(ms::LatLon::kMinLat <= ll.m_lat && ll.m_lat <= ms::LatLon::kMaxLat, (ll));
  ASSERT(ms::LatLon::kMinLon <= ll.m_lon && ll.m_lon <= ms::LatLon::kMaxLon, (ll));

  double const latRad = base::DegToRad(ll.m_lat);
  double const lonRad = base::DegToRad(ll.m_lon);

  double const x = sphereRadius * cos(latRad) * cos(lonRad);
  double const y = sphereRadius * cos(latRad) * sin(lonRad);
  double const z = sphereRadius * sin(latRad);

  return {x, y, z};
}
}  // namespace

namespace ms
{
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
  return fabs(area);
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
