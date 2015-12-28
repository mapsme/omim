#pragma once

#include "geometry/distance.hpp"
#include "geometry/point2d.hpp"

#include "std/vector.hpp"

namespace search
{
struct ProjectionOnStreet
{
  ProjectionOnStreet();

  // Nearest point on the street to a given point.
  m2::PointD m_proj;

  // Distance in meters from a given point to |m_proj|.
  double m_distMeters;

  // Index of the street segment that |m_proj| belongs to.
  size_t m_segIndex;

  // When true, the point is located to the right from the projection
  // segment, otherwise, the point is located to the left from the
  // projection segment.
  bool m_projSign;
};

class ProjectionOnStreetCalculator
{
public:
  static int constexpr kDefaultMaxDistMeters = 200;

  ProjectionOnStreetCalculator(vector<m2::PointD> const & points, double maxDistMeters);

  // Finds nearest point on the street to the |point|. If such point
  // is located within |m_maxDistMeters|, stores projection in |proj|
  // and returns true. Otherwise, returns false and does not modify
  // |proj|.
  bool GetProjection(m2::PointD const & point, ProjectionOnStreet & proj);

private:
  vector<m2::PointD> const & m_points;
  vector<m2::ProjectionToSection<m2::PointD>> m_segProjs;
  double const m_maxDistMeters;
};
}  // namespace search
