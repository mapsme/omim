#include "render/move_screen_task.hpp"

namespace rg
{

MoveScreenTask::MoveScreenTask(Navigator & navigator,
                               m2::PointD const & startPt,
                               m2::PointD const & endPt,
                               double interval)
  : anim::SegmentInterpolation(startPt,
                               endPt,
                               interval,
                               m_outPt),
    m_navigator(navigator)
{}

void MoveScreenTask::OnStep(double ts)
{
  m2::PointD oldPt = m_outPt;
  anim::SegmentInterpolation::OnStep(ts);
  m_navigator.SetOrg(m_navigator.Screen().GetOrg() + m_outPt - oldPt);
}

void MoveScreenTask::OnEnd(double ts)
{
  anim::SegmentInterpolation::OnEnd(ts);
  m_navigator.SetOrg(m_outPt);
}

bool MoveScreenTask::IsVisual() const
{
  return true;
}

}
