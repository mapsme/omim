#pragma once

#include "geometry/point2d.hpp"

#include "std/cstdint.hpp"

class DataSource;

namespace routing
{
extern uint8_t const kNoSpeedCamera;

uint8_t CheckCameraInPoint(m2::PointD const & point, DataSource const & dataSource);
}  // namespace routing
