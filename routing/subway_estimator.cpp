#include "routing/subway_estimator.hpp"

#include "geometry/mercator.hpp"

namespace routing
{
double SubwayEstimator::CalcWeightTheSameLine(m2::PointD const & fromPnt, m2::PointD const & toPnt, double speedKmPerH) const
{
  CHECK_GREATER(speedKmPerH, 0, ());
  double const distMeters = MercatorBounds::DistanceOnEarth(fromPnt, toPnt);
  double const speedMPerS = speedKmPerH / 3.6;

  return distMeters / speedMPerS;
}

double SubwayEstimator::CalcWeightTheSamePoint(SubwayVertex const & from,
                                               SubwayVertex const & to) const
{
  if (from.GetType() == SubwayType::Line && to.GetType() == SubwayType::Change)
    return 120.0;

  return 0.0;
}
}  // namespace routing
