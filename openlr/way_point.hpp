#pragma once

#include "openlr/openlr_model.hpp"

#include "geometry/mercator.hpp"
#include "geometry/point2d.hpp"

#include <cstdint>

namespace openlr
{
struct WayPoint final
{
  WayPoint() = default;

  WayPoint(openlr::LocationReferencePoint const & lrp)
    : m_point(MercatorBounds::FromLatLon(lrp.m_latLon))
    , m_distanceToNextPointM(lrp.m_distanceToNextPoint)
    , m_bearing(lrp.m_bearing)
    , m_lfrcnp(lrp.m_functionalRoadClass)
  {
  }

  m2::PointD m_point = m2::PointD::Zero();
  double m_distanceToNextPointM = 0.0;
  uint8_t m_bearing = 0;
  openlr::FunctionalRoadClass m_lfrcnp = openlr::FunctionalRoadClass::NotAValue;
};
}  // namespace openlr
