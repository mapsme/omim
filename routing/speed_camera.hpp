#pragma once

#include "geometry/point2d.hpp"

#include <cstdint>

class Index;

namespace routing
{
extern uint8_t const kNoSpeedCamera;

uint8_t CheckCameraInPoint(m2::PointD const & point, Index const & index);
}  // namespace routing
