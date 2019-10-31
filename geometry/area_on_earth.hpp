#pragma once

#include "geometry/latlon.hpp"
#include "geometry/point2d.hpp"
#include "geometry/rect2d.hpp"

#include <vector>

namespace ms
{
namespace area_on_sphere_details
{
// Returns angle in rads at ∠(lla, llb, llc).
long double AngleOnEarth(LatLon const & lla, LatLon const & llb, LatLon const & llc);

std::vector<m2::PointD> DropUniquePoints(std::vector<m2::PointD> const & points);

// Checks that if we add |newPoint| to |polyline| at the end, |polyline| won't be self intersected.
bool AbleToAddWithoutSelfIntersections(std::vector<m2::PointD> const & polyline,
                                       m2::PointD const & newPoint);

// Drop some points from |points| so that in the result vector:
//
// dist(result[0], result[1]) >= intervalMeters,
// dist(result[1], result[2]) >= intervalMeters,
// ...
// dist(result[n - 2], result[n - 1]) >= intervalMeters
//
// And there is no self intersection in the results vector after dropping such points.
std::vector<m2::PointD> DropClosePoints(std::vector<m2::PointD> const & points,
                                        double intervalMeters);

// Drop such points that lie almost at one line, for example:
// If we had such triple: ... (0,0), (1,0), (2,0), ...
// Point (1,0) will be dropped.
std::vector<m2::PointD> DropAlmostAtOneLinePointsEps(std::vector<m2::PointD> const & points,
                                                     double epsRad);

// Returns polygon area on earth. Works bad if we had close to each
// other points or triples of points: A, B, C such they almost lie at one line.
double AreaOnEarth(std::vector<ms::LatLon> const & points);

// Builds convex hull from |points| and calculates area by triangulating the convex hull.
double AreaOnEarthWithConvexHull(std::vector<m2::PointD> const & points);

std::vector<m2::PointD> SimplifyPolygon(std::vector<m2::PointD> const & points);
}  // namespace area_on_sphere_details

// Calculation area on earth is quite complicated task. The problem with small distances and
// double precision. Because of that function simplify polygon at first, than tries to calculate
// area with such formula: S = (θ - (n - 2)π) * R^2 (http://mathworld.wolfram.com/SphericalPolygon.html)
// Where θ is the sum of the radian angles of a spherical polygon on a sphere of radius R.
// If ares is less than zero (means polygon is small, for example with perimeter 20km),
// the area of convex hull will be returned. The last one could be calculated accurately with
// AreaOnEarth(LatLon const &, LatLon const &, LatLon const &) function.
double ApproximatePolygonAreaOnEarth(std::vector<m2::PointD> const & points);

// Returns area of triangle on earth.
double AreaOnEarth(LatLon const & ll1, LatLon const & ll2, LatLon const & ll3);

// Area of the spherical cap that contains all points
// within the distance |radius| from an arbitrary fixed point, measured
// along the Earth surface.
// In particular, the smallest cap spanning the whole Earth results
// from radius = pi*EarthRadius.
// For small enough radiuses, returns the value close to pi*|radius|^2.
double CircleAreaOnEarth(double radius);
}  // namespace ms
