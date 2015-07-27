#pragma once

#include "anim/angle_interpolation.hpp"
#include "render/navigator.hpp"

namespace rg
{

class RotateScreenTask : public anim::AngleInterpolation
{
private:

  Navigator & m_navigator;
  double m_outAngle;

public:

  RotateScreenTask(Navigator & navigator,
                   double startAngle,
                   double endAngle,
                   double speed);

  void OnStep(double ts);
  void OnEnd(double ts);

  bool IsVisual() const;
};

} // namespace rg
