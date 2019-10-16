#include "geometry/distance_on_sphere.hpp"

#include "geometry/point3d.hpp"
#include "geometry/mercator.hpp"

#include "base/assert.hpp"
#include "base/math.hpp"

#include <algorithm>
#include <cmath>

using namespace std;

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
}  // namespace ms
