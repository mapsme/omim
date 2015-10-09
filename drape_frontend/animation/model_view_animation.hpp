#pragma once

#include "drape_frontend/animation/base_interpolator.hpp"
#include "drape_frontend/animation/interpolations.hpp"

#include "geometry/any_rect2d.hpp"
#include "geometry/screenbase.hpp"

namespace df
{

enum class ModelViewAnimationType
{
  Default,
  Scale,
  FollowAndRotate,
  KineticScroll
};

class BaseModelViewAnimation : public BaseInterpolator
{
public:
  BaseModelViewAnimation(double duration, double delay = 0) : BaseInterpolator(duration, delay) {}

  virtual ModelViewAnimationType GetType() const = 0;
  virtual m2::AnyRectD GetCurrentRect(ScreenBase const & screen) const = 0;
  virtual m2::AnyRectD GetTargetRect(ScreenBase const & screen) const = 0;
};

class ModelViewAnimation : public BaseModelViewAnimation
{
public:
  static double GetRotateDuration(double startAngle, double endAngle);
  static double GetMoveDuration(m2::PointD const & startPt, m2::PointD const & endPt, ScreenBase const & convertor);
  static double GetScaleDuration(double startSize, double endSize);

  /// aDuration - angleDuration
  /// mDuration - moveDuration
  /// sDuration - scaleDuration
  ModelViewAnimation(m2::AnyRectD const & startRect, m2::AnyRectD const & endRect,
                     double aDuration, double mDuration, double sDuration);

  ModelViewAnimationType GetType() const override { return ModelViewAnimationType::Default; }
  m2::AnyRectD GetCurrentRect(ScreenBase const & screen) const override;
  m2::AnyRectD GetTargetRect(ScreenBase const & screen) const override;

protected:
  m2::AnyRectD GetRect(double elapsedTime) const;

private:
  InerpolateAngle m_angleInterpolator;
  m2::PointD m_startZero, m_endZero;
  m2::RectD m_startRect, m_endRect;

  double m_angleDuration;
  double m_moveDuration;
  double m_scaleDuration;
};

class ScaleAnimation : public ModelViewAnimation
{
public:
  ScaleAnimation(m2::AnyRectD const & startRect, m2::AnyRectD const & endRect,
                 double aDuration, double mDuration, double sDuration,
                 m2::PointD const & globalPoint, m2::PointD const & pixelOffset);

  ModelViewAnimationType GetType() const override { return ModelViewAnimationType::Scale; }
  m2::AnyRectD GetCurrentRect(ScreenBase const & screen) const override;
  m2::AnyRectD GetTargetRect(ScreenBase const & screen) const override;

private:
  void ApplyPixelOffset(ScreenBase const & screen, m2::AnyRectD & rect) const;
  m2::PointD m_globalPoint;
  m2::PointD m_pixelOffset;
};

class FollowAndRotateAnimation : public BaseModelViewAnimation
{
public:
  FollowAndRotateAnimation(m2::AnyRectD const & startRect, m2::PointD const & userPos,
                           double newCenterOffset, double oldCenterOffset,
                           double azimuth, double duration);

  ModelViewAnimationType GetType() const override { return ModelViewAnimationType::FollowAndRotate; }
  m2::AnyRectD GetCurrentRect(ScreenBase const & screen) const override;
  m2::AnyRectD GetTargetRect(ScreenBase const & screen) const override;

private:
  m2::AnyRectD GetRect(double elapsedTime) const;

  InerpolateAngle m_angleInterpolator;
  m2::RectD m_rect;
  m2::PointD m_userPos;
  double m_newCenterOffset;
  double m_oldCenterOffset;
};

} // namespace df
