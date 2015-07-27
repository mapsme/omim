#pragma once

#include "render/active_maps_bridge.hpp"
#include "render/navigator.hpp"

#include "anim/task.hpp"

#include "geometry/point2d.hpp"
#include "geometry/any_rect2d.hpp"

#include "std/shared_ptr.hpp"

namespace rg
{

class RotateScreenTask;
class MoveScreenTask;
/// Class, which is responsible for
/// tracking all map animations.
class Animator
{
public:
  using TPushAnimFn = function<void (shared_ptr<anim::Task> const &)>;

  Animator(Navigator & navigator, TPushAnimFn const & pushAnimFn, ActiveMapsBridge * activeMaps);
  /// rotate screen by shortest path.
  void RotateScreen(double startAngle,
                    double endAngle);
  /// stopping screen rotation
  void StopRotation();

  /// get screen rotation speed
  double GetRotationSpeed() const;
  /// move screen from one point to another
  shared_ptr<MoveScreenTask> const & MoveScreen(m2::PointD const & startPt,
                                                m2::PointD const & endPt);
  shared_ptr<MoveScreenTask> const & MoveScreen(m2::PointD const & startPt,
                                                m2::PointD const & endPt,
                                                double speed);
  /// stopping screen movement
  void StopMoveScreen();

  void ShowRectVisibleScale(m2::RectD const & rect, int scale);
  void SetViewportCenter(m2::PointD const & centerPt);
  void SetAngle(double angle);

  Navigator const & GetNavigator() const { return m_navigator; }

private:
  void CheckMinMaxVisibleRect(m2::RectD & r, int maxScale);
  void StopAnimation(shared_ptr<anim::Task> const & task);

private:
  shared_ptr<RotateScreenTask> m_rotateScreenTask;
  shared_ptr<MoveScreenTask> m_moveScreenTask;

  rg::Navigator & m_navigator;
  TPushAnimFn m_pushAnim;
  ActiveMapsBridge * m_activeMaps;
};

}
