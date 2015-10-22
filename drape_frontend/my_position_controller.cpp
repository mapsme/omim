#include "my_position_controller.hpp"
#include "visual_params.hpp"
#include "animation/base_interpolator.hpp"
#include "animation/interpolations.hpp"
#include "animation/model_view_animation.hpp"

#include "indexer/mercator.hpp"

#include "3party/Alohalytics/src/alohalytics.h"

namespace df
{

namespace
{

int const POSITION_Y_OFFSET = 75;
double const GPS_BEARING_LIFETIME_S = 5.0;
double const MIN_SPEED_THRESHOLD_MPS = 1.0;

uint16_t SetModeBit(uint32_t mode, uint32_t bit)
{
  return mode | bit;
}

uint16_t ResetModeBit(uint32_t mode, uint32_t bit)
{
  return mode & (~bit);
}

location::EMyPositionMode ResetAllModeBits(uint32_t mode)
{
  return (location::EMyPositionMode)(mode & 0xF);
}

uint16_t ChangeMode(uint32_t mode, location::EMyPositionMode newMode)
{
  return (mode & 0xF0) | newMode;
}

bool TestModeBit(uint32_t mode, uint32_t bit)
{
  return (mode & bit) != 0;
}

} // namespace

class MyPositionController::MyPositionAnim : public BaseInterpolator
{
  using TBase = BaseInterpolator;
public:
  MyPositionAnim(m2::PointD const & startPt, m2::PointD const & endPt, double moveDuration,
                 double startAzimut, double endAzimut, double rotationDuration)
    : TBase(max(moveDuration, rotationDuration))
    , m_startPt(startPt)
    , m_endPt(endPt)
    , m_moveDuration(moveDuration)
    , m_angleInterpolator(startAzimut, endAzimut)
    , m_rotateDuration(rotationDuration)
  {
  }

  m2::PointD GetCurrentPosition() const
  {
    return InterpolatePoint(m_startPt, m_endPt,
                            my::clamp(GetElapsedTime() / m_moveDuration, 0.0, 1.0));
  }

  bool IsMovingActive() const { return m_moveDuration > 0.0; }

  double GetCurrentAzimut() const
  {
    return m_angleInterpolator.Interpolate(my::clamp(GetElapsedTime() / m_rotateDuration, 0.0, 1.0));
  }

  bool IsRotatingActive() const { return m_rotateDuration > 0.0; }

private:
  m2::PointD m_startPt;
  m2::PointD m_endPt;
  double m_moveDuration;

  InerpolateAngle m_angleInterpolator;
  double m_rotateDuration;
};

MyPositionController::MyPositionController(location::EMyPositionMode initMode)
  : m_modeInfo(location::MODE_PENDING_POSITION)
  , m_afterPendingMode(location::MODE_FOLLOW)
  , m_errorRadius(0.0)
  , m_position(m2::PointD::Zero())
  , m_drawDirection(0.0)
  , m_lastGPSBearing(false)
  , m_isVisible(false)
  , m_isDirtyViewport(false)
{
  if (initMode > location::MODE_UNKNOWN_POSITION)
    m_afterPendingMode = initMode;
  else
    m_modeInfo = location::MODE_UNKNOWN_POSITION;
}

MyPositionController::~MyPositionController()
{
  m_anim.reset();
}

void MyPositionController::SetPixelRect(m2::RectD const & pixelRect)
{
  m_pixelRect = pixelRect;
  Follow();
}

void MyPositionController::SetListener(ref_ptr<MyPositionController::Listener> listener)
{
  m_listener = listener;
}

m2::PointD const & MyPositionController::Position() const
{
  return m_position;
}

double MyPositionController::GetErrorRadius() const
{
  return m_errorRadius;
}

bool MyPositionController::IsModeChangeViewport() const
{
  return GetMode() >= location::MODE_FOLLOW;
}

bool MyPositionController::IsModeHasPosition() const
{
  return GetMode() >= location::MODE_NOT_FOLLOW;
}

void MyPositionController::DragStarted()
{
  SetModeInfo(SetModeBit(m_modeInfo, BlockAnimation));
}

void MyPositionController::DragEnded(m2::PointD const & distance)
{
  SetModeInfo(ResetModeBit(m_modeInfo, BlockAnimation));
  if (distance.Length() > 0.2 * min(m_pixelRect.SizeX(), m_pixelRect.SizeY()))
    StopLocationFollow();

  Follow();
}

void MyPositionController::ScaleStarted()
{
  SetModeInfo(SetModeBit(m_modeInfo, BlockAnimation));
}

void MyPositionController::Rotated()
{
  location::EMyPositionMode mode = GetMode();
  if (mode == location::MODE_ROTATE_AND_FOLLOW)
    SetModeInfo(SetModeBit(m_modeInfo, StopFollowOnActionEnd));
}

void MyPositionController::CorrectScalePoint(m2::PointD & pt) const
{
  if (IsModeChangeViewport())
    pt = GetCurrentPixelBinding();
}

void MyPositionController::CorrectScalePoint(m2::PointD & pt1, m2::PointD & pt2) const
{
  if (IsModeChangeViewport())
  {
    m2::PointD const ptDiff = GetCurrentPixelBinding() - ((pt1 + pt2) * 0.5);
    pt1 += ptDiff;
    pt2 += ptDiff;
  }
}

void MyPositionController::CorrectGlobalScalePoint(m2::PointD & pt) const
{
  if (IsModeChangeViewport())
    pt = m_position;
}

void MyPositionController::ScaleEnded()
{
  SetModeInfo(ResetModeBit(m_modeInfo, BlockAnimation));
  if (TestModeBit(m_modeInfo, StopFollowOnActionEnd))
  {
    SetModeInfo(ResetModeBit(m_modeInfo, StopFollowOnActionEnd));
    StopLocationFollow();
  }
  Follow();
}

void MyPositionController::SetRenderShape(drape_ptr<MyPosition> && shape)
{
  m_shape = move(shape);
}

void MyPositionController::SetFixedZoom()
{
  SetModeInfo(SetModeBit(m_modeInfo, FixedZoomBit));
}

void MyPositionController::NextMode()
{
  string const kAlohalyticsClickEvent = "$onClick";
  location::EMyPositionMode currentMode = GetMode();
  location::EMyPositionMode newMode = currentMode;

  if (!IsInRouting())
  {
    switch (currentMode)
    {
    case location::MODE_UNKNOWN_POSITION:
      alohalytics::LogEvent(kAlohalyticsClickEvent, "@UnknownPosition");
      newMode = location::MODE_PENDING_POSITION;
      break;
    case location::MODE_PENDING_POSITION:
      alohalytics::LogEvent(kAlohalyticsClickEvent, "@PendingPosition");
      newMode = location::MODE_UNKNOWN_POSITION;
      m_afterPendingMode = location::MODE_FOLLOW;
      break;
    case location::MODE_NOT_FOLLOW:
      alohalytics::LogEvent(kAlohalyticsClickEvent, "@NotFollow");
      newMode = location::MODE_FOLLOW;
      break;
    case location::MODE_FOLLOW:
      alohalytics::LogEvent(kAlohalyticsClickEvent, "@Follow");
      if (IsRotationActive())
        newMode = location::MODE_ROTATE_AND_FOLLOW;
      else
      {
        newMode = location::MODE_UNKNOWN_POSITION;
        m_afterPendingMode = location::MODE_FOLLOW;
      }
      break;
    case location::MODE_ROTATE_AND_FOLLOW:
      alohalytics::LogEvent(kAlohalyticsClickEvent, "@RotateAndFollow");
      newMode = location::MODE_UNKNOWN_POSITION;
      m_afterPendingMode = location::MODE_FOLLOW;
      break;
    }
  }
  else
  {
    newMode = IsRotationActive() ? location::MODE_ROTATE_AND_FOLLOW : location::MODE_FOLLOW;
  }

  SetModeInfo(ChangeMode(m_modeInfo, newMode));
  Follow();
}

void MyPositionController::TurnOff()
{
  StopLocationFollow();
  SetModeInfo(location::MODE_UNKNOWN_POSITION);
  SetIsVisible(false);
}

void MyPositionController::Invalidate()
{
  location::EMyPositionMode currentMode = GetMode();
  if (currentMode > location::MODE_PENDING_POSITION)
  {
    SetModeInfo(ChangeMode(m_modeInfo, location::MODE_UNKNOWN_POSITION));
    SetModeInfo(ChangeMode(m_modeInfo, location::MODE_PENDING_POSITION));
    m_afterPendingMode = currentMode;
    SetIsVisible(true);
  }
  else if (currentMode == location::MODE_UNKNOWN_POSITION)
  {
    m_afterPendingMode = location::MODE_FOLLOW;
    SetIsVisible(false);
  }
}

void MyPositionController::OnLocationUpdate(location::GpsInfo const & info, bool isNavigable,
                                            ScreenBase const & screen)
{
  Assign(info, isNavigable, screen);

  SetIsVisible(true);

  if (GetMode() == location::MODE_PENDING_POSITION)
  {
    SetModeInfo(ChangeMode(m_modeInfo, m_afterPendingMode));
    m_afterPendingMode = location::MODE_FOLLOW;
  }
}

void MyPositionController::OnCompassUpdate(location::CompassInfo const & info,
                                           ScreenBase const & screen)
{
  Assign(info, screen);
}

void MyPositionController::SetModeListener(location::TMyPositionModeChanged const & fn)
{
  m_modeChangeCallback = fn;
  CallModeListener(m_modeInfo);
}

void MyPositionController::Render(uint32_t renderMode, ScreenBase const & screen, ref_ptr<dp::GpuProgramManager> mng,
                                  dp::UniformValuesStorage const  & commonUniforms)
{
  location::EMyPositionMode currentMode = GetMode();
  if (m_shape != nullptr && IsVisible() && currentMode > location::MODE_PENDING_POSITION)
  {
    if (m_isDirtyViewport && !TestModeBit(m_modeInfo, BlockAnimation))
    {
      Follow();
      m_isDirtyViewport = false;
    }

    m_shape->SetPosition(GetDrawablePosition());
    m_shape->SetAzimuth(GetDrawableAzimut());
    m_shape->SetIsValidAzimuth(IsRotationActive());
    m_shape->SetAccuracy(m_errorRadius);
    m_shape->SetRoutingMode(IsInRouting());

    if ((renderMode & RenderAccuracy) != 0)
      m_shape->RenderAccuracy(screen, mng, commonUniforms);

    if ((renderMode & RenderMyPosition) != 0)
      m_shape->RenderMyPosition(screen, mng, commonUniforms);
  }

  CheckAnimFinished();
}

void MyPositionController::AnimateStateTransition(location::EMyPositionMode oldMode, location::EMyPositionMode newMode)
{
  if (oldMode == location::MODE_PENDING_POSITION && newMode == location::MODE_FOLLOW)
  {
    if (!TestModeBit(m_modeInfo, FixedZoomBit))
    {
      m2::PointD const size(m_errorRadius, m_errorRadius);
      ChangeModelView(m2::RectD(m_position - size, m_position + size));
    }
  }
  else if (newMode == location::MODE_ROTATE_AND_FOLLOW)
  {
    Follow();
  }
  else if (oldMode == location::MODE_ROTATE_AND_FOLLOW && newMode == location::MODE_UNKNOWN_POSITION)
  {
    ChangeModelView(0.0);
    ChangeModelView(m_position);
  }
}

void MyPositionController::Assign(location::GpsInfo const & info, bool isNavigable, ScreenBase const & screen)
{
  m2::PointD oldPos = GetDrawablePosition();
  double oldAzimut = GetDrawableAzimut();

  m2::RectD rect = MercatorBounds::MetresToXY(info.m_longitude,
                                              info.m_latitude,
                                              info.m_horizontalAccuracy);
  m_position = rect.Center();
  m_errorRadius = rect.SizeX() / 2;

  bool const hasBearing = info.HasBearing();
  if ((isNavigable && hasBearing) ||
      (!isNavigable && hasBearing && info.HasSpeed() && info.m_speed > MIN_SPEED_THRESHOLD_MPS))
  {
    SetDirection(my::DegToRad(info.m_bearing));
    m_lastGPSBearing.Reset();
  }

  if (m_listener)
    m_listener->PositionChanged(Position());

  CreateAnim(oldPos, oldAzimut, screen);

  m_isDirtyViewport = true;
}

void MyPositionController::Assign(location::CompassInfo const & info, ScreenBase const & screen)
{
  m2::PointD oldPos = GetDrawablePosition();
  double oldAzimut = GetDrawableAzimut();

  if ((IsInRouting() && GetMode() >= location::MODE_FOLLOW) ||
      (m_lastGPSBearing.ElapsedSeconds() < GPS_BEARING_LIFETIME_S))
  {
    return;
  }

  SetDirection(info.m_bearing);
  CreateAnim(oldPos, oldAzimut, screen);

  m_isDirtyViewport = true;
}

void MyPositionController::SetDirection(double bearing)
{
  m_drawDirection = bearing;
  SetModeInfo(SetModeBit(m_modeInfo, KnownDirectionBit));
}

void MyPositionController::SetModeInfo(uint32_t modeInfo, bool force)
{
  location::EMyPositionMode const newMode = ResetAllModeBits(modeInfo);
  location::EMyPositionMode const oldMode = GetMode();
  m_modeInfo = modeInfo;
  if (newMode != oldMode || force)
  {
    AnimateStateTransition(oldMode, newMode);
    CallModeListener(newMode);
  }
}

location::EMyPositionMode MyPositionController::GetMode() const
{
  return ResetAllModeBits(m_modeInfo);
}

void MyPositionController::CallModeListener(uint32_t mode)
{
  if (m_modeChangeCallback != nullptr)
    m_modeChangeCallback(ResetAllModeBits(mode));
}

bool MyPositionController::IsInRouting() const
{
  return TestModeBit(m_modeInfo, RoutingSessionBit);
}

bool MyPositionController::IsRotationActive() const
{
  return TestModeBit(m_modeInfo, KnownDirectionBit);
}

void MyPositionController::StopLocationFollow()
{
  location::EMyPositionMode currentMode = GetMode();
  if (currentMode > location::MODE_NOT_FOLLOW)
    SetModeInfo(ChangeMode(m_modeInfo, location::MODE_NOT_FOLLOW));
  else if (currentMode == location::MODE_PENDING_POSITION)
    m_afterPendingMode = location::MODE_NOT_FOLLOW;
}

void MyPositionController::StopCompassFollow()
{
  if (GetMode() != location::MODE_ROTATE_AND_FOLLOW)
    return;

  SetModeInfo(ChangeMode(m_modeInfo, location::MODE_FOLLOW));
  Follow();
}

void MyPositionController::ChangeModelView(m2::PointD const & center)
{
  if (m_listener)
    m_listener->ChangeModelView(center);
}

void MyPositionController::ChangeModelView(double azimuth)
{
  if (m_listener)
    m_listener->ChangeModelView(azimuth);
}

void MyPositionController::ChangeModelView(m2::RectD const & rect)
{
  if (m_listener)
    m_listener->ChangeModelView(rect);
}

void MyPositionController::ChangeModelView(m2::PointD const & userPos, double azimuth,
                                           m2::PointD const & pxZero)
{
  if (m_listener)
    m_listener->ChangeModelView(userPos, azimuth, pxZero);
}

void MyPositionController::Follow()
{
  location::EMyPositionMode currentMode = GetMode();
  if (currentMode == location::MODE_FOLLOW)
    ChangeModelView(m_position);
  else if (currentMode == location::MODE_ROTATE_AND_FOLLOW)
    ChangeModelView(m_position, m_drawDirection, GetRaFPixelBinding());
}

m2::PointD MyPositionController::GetRaFPixelBinding() const
{
  return m2::PointD (m_pixelRect.Center().x,
                     m_pixelRect.maxY() - POSITION_Y_OFFSET * VisualParams::Instance().GetVisualScale());
}

m2::PointD MyPositionController::GetCurrentPixelBinding() const
{
  location::EMyPositionMode mode = GetMode();
  if (mode == location::MODE_FOLLOW)
    return m_pixelRect.Center();
  else if (mode == location::MODE_ROTATE_AND_FOLLOW)
    return GetRaFPixelBinding();
  else
    ASSERT(false, ());

  return m2::PointD::Zero();
}

m2::PointD MyPositionController::GetDrawablePosition() const
{
  if (m_anim && m_anim->IsMovingActive())
    return m_anim->GetCurrentPosition();

  return Position();
}

double MyPositionController::GetDrawableAzimut() const
{
  if (m_anim && m_anim->IsRotatingActive())
    return m_anim->GetCurrentAzimut();

  return m_drawDirection;
}

void MyPositionController::CheckAnimFinished() const
{
  if (m_anim && m_anim->IsFinished())
    m_anim.reset();
}

void MyPositionController::CreateAnim(m2::PointD const & oldPos, double oldAzimut, ScreenBase const & screen)
{
  double moveDuration = ModelViewAnimation::GetMoveDuration(oldPos, m_position, screen);
  double rotateDuration = ModelViewAnimation::GetRotateDuration(oldAzimut, m_drawDirection);
  double maxDuration = max(moveDuration, rotateDuration);
  double kMaxMyPositionDuration = 2.0; // in seconds
  if (maxDuration > 0.0 && maxDuration < kMaxMyPositionDuration)
    m_anim.reset(new MyPositionAnim(oldPos, m_position, moveDuration, oldAzimut, m_drawDirection, rotateDuration));
}

void MyPositionController::ActivateRouting()
{
  if (!IsInRouting())
  {
    ASSERT(IsModeHasPosition(), ());
    SetModeInfo(ChangeMode(SetModeBit(m_modeInfo, RoutingSessionBit), location::MODE_NOT_FOLLOW));
  }
}

void MyPositionController::DeactivateRouting()
{
  if (IsInRouting())
  {
    SetModeInfo(ResetModeBit(m_modeInfo, RoutingSessionBit));

    location::EMyPositionMode currentMode = GetMode();
    if (currentMode == location::MODE_ROTATE_AND_FOLLOW)
      SetModeInfo(ChangeMode(m_modeInfo, location::MODE_FOLLOW));

    ChangeModelView(0.0);
  }
}

}
