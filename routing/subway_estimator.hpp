#pragma once

#include "routing/subway_vertex.hpp"

#include "routing_common/vehicle_model.hpp"

#include "geometry/point2d.hpp"

#include <memory>

namespace routing
{
class SubwayEstimator final
{
public:
  explicit SubwayEstimator(std::shared_ptr<VehicleModelFactory> modelFactory)
    : m_modelFactory(modelFactory)
  {
  }

  /// \returns weight in seconds.
  double CalcWeightTheSameLine(SubwayType type,
                               m2::PointD const & fromPnt,
                               m2::PointD const & toPnt,
                               double speedKmPerH) const;
  double CalcWeightTheSamePoint(SubwayVertex const & from, SubwayVertex const & to) const;

private:
  std::shared_ptr<VehicleModelFactory> m_modelFactory;
};
}  // namespace routing