#include "render/animator.hpp"
#include "render/rotate_screen_task.hpp"
#include "render/move_screen_task.hpp"
#include "render/navigator_utils.hpp"

#include "indexer/scales.hpp"

#include "anim/controller.hpp"

#include "geometry/angles.hpp"

namespace rg
{

Animator::Animator(Navigator & navigator, TPushAnimFn const & pushAnimFn, ActiveMapsBridge * activeMaps)
  : m_navigator(navigator)
  , m_pushAnim(pushAnimFn)
  , m_activeMaps(activeMaps)
{
}

void Animator::StopAnimation(shared_ptr<anim::Task> const & task)
{
  if (task)
  {
    task->Lock();

    if (!task->IsEnded()
     && !task->IsCancelled())
      task->Cancel();

    task->Unlock();
  }
}

void Animator::RotateScreen(double startAngle, double endAngle)
{
  if (m_rotateScreenTask)
    m_rotateScreenTask->Lock();

  bool const inProgress =
      m_rotateScreenTask &&
      !m_rotateScreenTask->IsCancelled() &&
      !m_rotateScreenTask->IsEnded();

  if (inProgress)
  {
    m_rotateScreenTask->SetEndAngle(endAngle);
  }
  else
  {
    double const eps = my::DegToRad(1.5);

    if (fabs(ang::GetShortestDistance(startAngle, endAngle)) > eps)
    {
      if (m_rotateScreenTask)
      {
        m_rotateScreenTask->Unlock();
        m_rotateScreenTask.reset();
      }

      m_rotateScreenTask.reset(new RotateScreenTask(m_navigator,
                                                    startAngle,
                                                    endAngle,
                                                    GetRotationSpeed()));

      m_pushAnim(m_rotateScreenTask);
      return;
    }
  }

  if (m_rotateScreenTask)
    m_rotateScreenTask->Unlock();
}

void Animator::StopRotation()
{
  StopAnimation(m_rotateScreenTask);
  m_rotateScreenTask.reset();
}

shared_ptr<MoveScreenTask> const & Animator::MoveScreen(m2::PointD const & startPt,
                                                        m2::PointD const & endPt)
{
  return MoveScreen(startPt, endPt, m_navigator.ComputeMoveSpeed(startPt, endPt));
}

shared_ptr<MoveScreenTask> const & Animator::MoveScreen(m2::PointD const & startPt,
                                                        m2::PointD const & endPt,
                                                        double speed)
{
  StopMoveScreen();

  m_moveScreenTask.reset(new MoveScreenTask(m_navigator,
                                            startPt,
                                            endPt,
                                            speed));

  m_pushAnim(m_moveScreenTask);

  return m_moveScreenTask;
}

void Animator::StopMoveScreen()
{
  StopAnimation(m_moveScreenTask);
  m_moveScreenTask.reset();
}

void Animator::ShowRectVisibleScale(m2::RectD const & rect, int scale)
{
  m2::RectD r = rect;
  CheckMinMaxVisibleRect(r, scale);
  SetRectFixedAR(ToRotated(r, m_navigator), m_navigator);
}

void Animator::SetViewportCenter(m2::PointD const & centerPt)
{
  m_navigator.CenterViewport(centerPt);
}

void Animator::SetAngle(double angle)
{
  m_navigator.SetAngle(angle);
}

void Animator::CheckMinMaxVisibleRect(m2::RectD & rect, int maxScale)
{
  CheckMinGlobalRect(rect, m_navigator.GetScaleProcessor());

  m2::PointD const c = rect.Center();
  int const worldS = scales::GetUpperWorldScale();
  ScalesProcessor const & scales = m_navigator.GetScaleProcessor();

  int scale = scales.GetDrawTileScale(rect);
  if (scale > worldS && !m_activeMaps->IsCountryLoaded(c))
  {
    // country is not loaded - limit on world scale
    rect = scales.GetRectForDrawScale(worldS, c);
    scale = worldS;
  }

  if (maxScale != -1 && scale > maxScale)
  {
    // limit on passed maximal scale
    rect = scales.GetRectForDrawScale(maxScale, c);
  }
}

double Animator::GetRotationSpeed() const
{
  // making full circle in ~1 seconds.
  return 6.0;
}

} // namespace rg
