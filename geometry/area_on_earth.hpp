#pragma once

#include "geometry/latlon.hpp"
#include "geometry/point2d.hpp"

#include <vector>

namespace ms
{
// Note: This function is not implemented yet.
// Returns polygon area on earth.
double AreaOnEarth(std::vector<m2::PointD> const & points);
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
