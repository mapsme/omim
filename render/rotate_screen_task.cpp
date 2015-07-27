#include "render/rotate_screen_task.hpp"

namespace rg
{

RotateScreenTask::RotateScreenTask(Navigator & navigator,
                                   double startAngle,
                                   double endAngle,
                                   double speed)
  : anim::AngleInterpolation(startAngle,
                             endAngle,
                             speed,
                             m_outAngle),
    m_navigator(navigator)
{
}

void RotateScreenTask::OnStep(double ts)
{
  double prevAngle = m_outAngle;
  anim::AngleInterpolation::OnStep(ts);
  m_navigator.SetAngle(m_navigator.Screen().GetAngle() + m_outAngle - prevAngle);
}

void RotateScreenTask::OnEnd(double ts)
{
  anim::AngleInterpolation::OnEnd(ts);
  m_navigator.SetAngle(m_outAngle);
}

bool RotateScreenTask::IsVisual() const
{
  return true;
}

}
