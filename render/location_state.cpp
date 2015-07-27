#include "render/location_state.hpp"
#include "render/navigator.hpp"

#include "graphics/display_list.hpp"
#include "graphics/icon.hpp"
#include "graphics/depth_constants.hpp"
#include "graphics/screen.hpp"

#include "anim/controller.hpp"
#include "anim/task.hpp"
#include "anim/angle_interpolation.hpp"
#include "anim/segment_interpolation.hpp"

#include "gui/controller.hpp"

#include "indexer/mercator.hpp"
#include "indexer/scales.hpp"

#include "platform/location.hpp"
#include "platform/settings.hpp"

#include "geometry/rect2d.hpp"
#include "geometry/transformations.hpp"
#include "3party/Alohalytics/src/alohalytics.h"

namespace rg
{

namespace
{

static const int POSITION_Y_OFFSET = 120;
static const double POSITION_TOLERANCE = 1.0E-6;  // much less than coordinates coding error
static const double ANGLE_TOLERANCE = my::DegToRad(3.0);
static const double GPS_BEARING_LIFETIME_S = 5.0;


uint16_t IncludeModeBit(uint16_t mode, uint16_t bit)
{
  return mode | bit;
}

uint16_t ExcludeModeBit(uint16_t mode, uint16_t bit)
{
  return mode & (~bit);
}

location::EMyPositionMode ExcludeAllBits(uint16_t mode)
{
  return static_cast<location::EMyPositionMode>(mode & 0xF);
}

uint16_t ChangeMode(uint16_t mode, location::EMyPositionMode newMode)
{
  return (mode & 0xF0) | newMode;
}

bool TestModeBit(uint16_t mode, uint16_t bit)
{
  return (mode & bit) != 0;
}

class RotateAndFollowAnim : public anim::Task
{
public:
  RotateAndFollowAnim(Animator * animator,
                      m2::PointD const & srcPos,
                      double srcAngle,
                      m2::PointD const & srcPixelBinding,
                      m2::PointD const & dstPixelbinding)
    : m_hasPendingAnimation(false)
    , m_animator(animator)
  {
    m_angleAnim.reset(new anim::SafeAngleInterpolation(srcAngle, srcAngle, 1.0));
    m_posAnim.reset(new anim::SafeSegmentInterpolation(srcPos, srcPos, 1.0));
    m2::PointD const srcInverted = InvertPxBinding(srcPixelBinding);
    m2::PointD const dstInverted = InvertPxBinding(dstPixelbinding);
    m_pxBindingAnim.reset(new anim::SafeSegmentInterpolation(srcInverted, dstInverted,
                                                             GetNavigator().ComputeMoveSpeed(srcInverted, dstInverted)));
  }

  void SetDestinationParams(m2::PointD const & dstPos, double dstAngle)
  {
    ASSERT(m_angleAnim != nullptr, ());
    ASSERT(m_posAnim != nullptr, ());

    if (IsVisual() || m_idleFrames > 0)
    {
      //Store new params even if animation is active but don't interrupt the current one.
      //New animation to the pending params will be made after all.
      m_hasPendingAnimation = true;
      m_pendingDstPos = dstPos;
      m_pendingAngle = dstAngle;
    }
    else
      SetParams(dstPos, dstAngle);
  }

  void Update()
  {
    if (!IsVisual() && m_hasPendingAnimation && m_idleFrames == 0)
    {
      m_hasPendingAnimation = false;
      SetParams(m_pendingDstPos, m_pendingAngle);
      m_controller->CallInvalidate();
    }
    else if (m_idleFrames > 0)
    {
      --m_idleFrames;
      m_controller->CallInvalidate();
    }
  }

  m2::PointD const & GetPositionForDraw()
  {
    return m_posAnim->GetCurrentValue();
  }

  virtual void OnStep(double ts)
  {
    if (m_idleFrames > 0)
      return;

    ASSERT(m_angleAnim != nullptr, ());
    ASSERT(m_posAnim != nullptr, ());
    ASSERT(m_pxBindingAnim != nullptr, ());

    bool updateViewPort = false;
    updateViewPort |= OnStep(m_angleAnim.get(), ts);
    updateViewPort |= OnStep(m_posAnim.get(), ts);
    updateViewPort |= OnStep(m_pxBindingAnim.get(), ts);

    if (updateViewPort)
    {
      UpdateViewport();
      if (!IsVisual())
        m_idleFrames = 5;
    }
  }

  virtual bool IsVisual() const
  {
    ASSERT(m_posAnim != nullptr, ());
    ASSERT(m_angleAnim != nullptr, ());
    ASSERT(m_pxBindingAnim != nullptr, ());

    return m_posAnim->IsRunning() ||
           m_angleAnim->IsRunning() ||
           m_pxBindingAnim->IsRunning();
  }

private:
  void UpdateViewport()
  {
    ASSERT(m_posAnim != nullptr, ());
    ASSERT(m_angleAnim != nullptr, ());
    ASSERT(m_pxBindingAnim != nullptr, ());

    m2::PointD const & pxBinding = m_pxBindingAnim->GetCurrentValue();
    m2::PointD const & currentPosition = m_posAnim->GetCurrentValue();
    double currentAngle = m_angleAnim->GetCurrentValue();

    //@{ pixel coord system
    m2::PointD const pxCenter = GetPixelRect().Center();
    m2::PointD vectorToCenter = pxCenter - pxBinding;
    if (!vectorToCenter.IsAlmostZero())
      vectorToCenter = vectorToCenter.Normalize();
    m2::PointD const vectorToTop = m2::PointD(0.0, 1.0);
    double sign = m2::CrossProduct(vectorToTop, vectorToCenter) > 0 ? 1 : -1;
    double angle = sign * acos(m2::DotProduct(vectorToTop, vectorToCenter));
    //@}

    //@{ global coord system
    ScreenBase const & screen = m_animator->GetNavigator().Screen();
    double offset = (screen.PtoG(pxCenter) - screen.PtoG(pxBinding)).Length();
    m2::PointD const viewPoint = currentPosition.Move(1.0, currentAngle + my::DegToRad(90.0));
    m2::PointD const viewVector = viewPoint - currentPosition;
    m2::PointD rotateVector = viewVector;
    rotateVector.Rotate(angle);
    rotateVector.Normalize();
    rotateVector *= offset;
    //@}

    m_animator->SetViewportCenter(currentPosition + rotateVector);
    m_animator->SetAngle(currentAngle);
    m_controller->CallInvalidate();
  }

  void SetParams(m2::PointD const & dstPos, double dstAngle)
  {
    double const angleDist = fabs(ang::GetShortestDistance(m_angleAnim->GetCurrentValue(), dstAngle));
    if (dstPos.EqualDxDy(m_posAnim->GetCurrentValue(), POSITION_TOLERANCE) && angleDist < ANGLE_TOLERANCE)
      return;

    double const posSpeed = 2 * GetNavigator().ComputeMoveSpeed(m_posAnim->GetCurrentValue(), dstPos);
    double const angleSpeed = angleDist < 1.0 ? 1.5 : m_animator->GetRotationSpeed();
    m_angleAnim->ResetDestParams(dstAngle, angleSpeed);
    m_posAnim->ResetDestParams(dstPos, posSpeed);
  }

  bool OnStep(anim::Task * task, double ts)
  {
    if (!task->IsReady() && !task->IsRunning())
      return false;

    if (task->IsReady())
    {
      task->Start();
      task->OnStart(ts);
    }

    if (task->IsRunning())
      task->OnStep(ts);

    if (task->IsEnded())
      task->OnEnd(ts);

    return true;
  }

private:
  m2::PointD InvertPxBinding(m2::PointD const & px) const
  {
    return m2::PointD(px.x, GetPixelRect().maxY() - px.y);
  }

  m2::RectD const & GetPixelRect() const
  {
    return GetNavigator().Screen().PixelRect();
  }

  Navigator const & GetNavigator() const { return m_animator->GetNavigator(); }

private:
  unique_ptr<anim::SafeAngleInterpolation> m_angleAnim;
  unique_ptr<anim::SafeSegmentInterpolation> m_posAnim;
  unique_ptr<anim::SafeSegmentInterpolation> m_pxBindingAnim;

  Animator * m_animator;

  bool m_hasPendingAnimation;
  m2::PointD m_pendingDstPos;
  double m_pendingAngle;
  // When map has active animation, backgroung rendering pausing
  // By this beetwen animations we wait some frames to release background rendering
  int m_idleFrames = 0;
};

string const LocationStateMode = "LastLocationStateMode";

} // namespace

State::Params::Params()
  : m_locationAreaColor(0, 0, 0, 0)
  , m_animator(nullptr)
{}

State::State(Params const & p)
  : TBase(p)
  , m_modeInfo(location::MODE_PENDING_POSITION)
  , m_errorRadius(0)
  , m_position(m2::PointD::Zero())
  , m_drawDirection(0.0)
  , m_lastGPSBearing(false)
  , m_afterPendingMode(location::MODE_FOLLOW)
  , m_routeMatchingInfo()
  , m_currentSlotID(0)
  , m_animator(p.m_animator)
{
  ASSERT(m_animator != nullptr, ());
  m_locationAreaColor = p.m_locationAreaColor;

  int mode = 0;
  if (Settings::Get(LocationStateMode, mode))
    m_modeInfo = mode;

  bool isBench = false;
  if (Settings::Get("IsBenchmarking", isBench) && isBench)
    m_modeInfo = location::MODE_UNKNOWN_POSITION;

  setIsVisible(false);
}

m2::PointD const & State::Position() const
{
  return m_position;
}

double State::GetErrorRadius() const
{
  return m_errorRadius;
}

location::EMyPositionMode State::GetMode() const
{
  return ExcludeAllBits(m_modeInfo);
}

bool State::IsModeChangeViewport() const
{
  return GetMode() >= location::MODE_FOLLOW;
}

bool State::IsModeHasPosition() const
{
  return GetMode() >= location::MODE_NOT_FOLLOW;
}

void State::SwitchToNextMode()
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
      alohalytics::LogEvent(kAlohalyticsClickEvent, "NotFollow");
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
    newMode = IsRotationActive() ? location::MODE_ROTATE_AND_FOLLOW : location::MODE_FOLLOW;

  SetModeInfo(ChangeMode(m_modeInfo, newMode));
}

void State::RouteBuilded()
{
  StopAllAnimations();
  SetModeInfo(IncludeModeBit(m_modeInfo, RoutingSessionBit));

  location::EMyPositionMode const mode = GetMode();
  if (mode > location::MODE_NOT_FOLLOW)
    SetModeInfo(ChangeMode(m_modeInfo, location::MODE_NOT_FOLLOW));
  else if (mode == location::MODE_UNKNOWN_POSITION)
  {
    m_afterPendingMode = location::MODE_NOT_FOLLOW;
    SetModeInfo(ChangeMode(m_modeInfo, location::MODE_PENDING_POSITION));
  }
}

void State::StartRouteFollow()
{
  ASSERT(IsInRouting(), ());
  ASSERT(IsModeHasPosition(), ());

  m2::PointD const size(m_errorRadius, m_errorRadius);
  m_animator->ShowRectVisibleScale(m2::RectD(m_position - size, m_position + size),
                                   scales::GetNavigationScale());

  SetModeInfo(ChangeMode(m_modeInfo, location::MODE_NOT_FOLLOW));
  SetModeInfo(ChangeMode(m_modeInfo, IsRotationActive() ? location::MODE_ROTATE_AND_FOLLOW : location::MODE_FOLLOW));
}

void State::StopRoutingMode()
{
  if (IsInRouting())
  {
    SetModeInfo(ChangeMode(ExcludeModeBit(m_modeInfo, RoutingSessionBit), GetMode() == location::MODE_ROTATE_AND_FOLLOW ? location::MODE_FOLLOW : location::MODE_NOT_FOLLOW));
    RotateOnNorth();
    AnimateFollow();
  }
}

void State::TurnOff()
{
  StopLocationFollow();
  SetModeInfo(location::MODE_UNKNOWN_POSITION);
  setIsVisible(false);
  invalidate();
}

void State::OnLocationUpdate(location::GpsInfo const & info, bool isNavigable)
{
  Assign(info, isNavigable);

  setIsVisible(true);

  if (GetMode() == location::MODE_PENDING_POSITION)
  {
    SetModeInfo(ChangeMode(m_modeInfo, m_afterPendingMode));
    m_afterPendingMode = location::MODE_FOLLOW;
  }
  else
    AnimateFollow();

  CallPositionChangedListeners(m_position);
  invalidate();
}

void State::OnCompassUpdate(location::CompassInfo const & info)
{
  if (Assign(info))
  {
    AnimateFollow();
    invalidate();
  }
}

void State::CallStateModeListeners()
{
  location::EMyPositionMode const currentMode = GetMode();
  for (auto it : m_modeListeners)
    it.second(currentMode);
}

int State::AddStateModeListener(location::TMyPositionModeChanged const & l)
{
  l(GetMode());
  int const slotID = m_currentSlotID++;
  m_modeListeners[slotID] = l;
  return slotID;
}

void State::RemoveStateModeListener(int slotID)
{
  m_modeListeners.erase(slotID);
}

void State::CallPositionChangedListeners(m2::PointD const & pt)
{
  for (auto it : m_positionListeners)
    it.second(pt);
}

int State::AddPositionChangedListener(State::TPositionListener const & func)
{
  int const slotID = m_currentSlotID++;
  m_positionListeners[slotID] = func;
  return slotID;
}

void State::RemovePositionChangedListener(int slotID)
{
  m_positionListeners.erase(slotID);
}

void State::InvalidatePosition()
{
  location::EMyPositionMode currentMode = GetMode();
  if (currentMode > location::MODE_PENDING_POSITION)
  {
    SetModeInfo(ChangeMode(m_modeInfo, location::MODE_UNKNOWN_POSITION));
    SetModeInfo(ChangeMode(m_modeInfo, location::MODE_PENDING_POSITION));
    m_afterPendingMode = currentMode;
    setIsVisible(true);
  }
  else if (currentMode == location::MODE_UNKNOWN_POSITION)
  {
    m_afterPendingMode = location::MODE_FOLLOW;
    setIsVisible(false);
  }

  invalidate();
}

void State::cache()
{
  CachePositionArrow();
  CacheRoutingArrow();
  CacheLocationMark();

  m_controller->GetCacheScreen()->completeCommands();
}

void State::purge()
{
  m_positionArrow.reset();
  m_locationMarkDL.reset();
  m_positionMarkDL.reset();
  m_routingArrow.reset();
}

void State::update()
{
  if (isVisible() && IsModeHasPosition())
  {
    m2::PointD const pxPosition = GetModelView().GtoP(Position());
    setPivot(pxPosition, false);

    if (m_animTask)
      static_cast<RotateAndFollowAnim *>(m_animTask.get())->Update();
  }
}

void State::draw(graphics::OverlayRenderer * r,
                 math::Matrix<double, 3, 3> const & m) const
{
  if (!IsModeHasPosition() || !isVisible())
    return;

  checkDirtyLayout();

  ScreenBase const & modelView = GetModelView();
  m2::PointD const pxPosition = modelView .GtoP(Position());
  double const pxErrorRadius = pxPosition.Length(modelView.GtoP(Position() + m2::PointD(m_errorRadius, 0.0)));

  double const drawScale = pxErrorRadius / s_cacheRadius;
  m2::PointD const & pivotPosition = GetPositionForDraw();

  math::Matrix<double, 3, 3> locationDrawM = math::Shift(
                                               math::Scale(
                                                 math::Identity<double, 3>(),
                                                 drawScale,
                                                 drawScale),
                                               pivotPosition);

  math::Matrix<double, 3, 3> const drawM = locationDrawM * m;
  // draw error sector
  r->drawDisplayList(m_locationMarkDL.get(), drawM);

  // if we know look direction than we draw arrow
  if (IsDirectionKnown())
  {
    double rotateAngle = m_drawDirection + modelView.GetAngle();

    math::Matrix<double, 3, 3> compassDrawM = math::Shift(
                                                math::Rotate(
                                                  math::Identity<double, 3>(),
                                                  rotateAngle),
                                                pivotPosition);

    if (!IsInRouting())
      r->drawDisplayList(m_positionArrow.get(), compassDrawM * m);
    else
      r->drawDisplayList(m_routingArrow.get(), compassDrawM * m);
  }
  else
    r->drawDisplayList(m_positionMarkDL.get(), drawM);
}

void State::CachePositionArrow()
{
  m_positionArrow.reset();
  m_positionArrow.reset(m_controller->GetCacheScreen()->createDisplayList());
  CacheArrow(m_positionArrow.get(), "current-position-compas");
}

void State::CacheRoutingArrow()
{
  m_routingArrow.reset();
  m_routingArrow.reset(m_controller->GetCacheScreen()->createDisplayList());
  CacheArrow(m_routingArrow.get(), "current-routing-compas");
}

void State::CacheLocationMark()
{
  graphics::Screen * cacheScreen = m_controller->GetCacheScreen();

  m_locationMarkDL.reset();
  m_locationMarkDL.reset(cacheScreen->createDisplayList());

  m_positionMarkDL.reset();
  m_positionMarkDL.reset(cacheScreen->createDisplayList());

  cacheScreen->beginFrame();
  cacheScreen->setDisplayList(m_locationMarkDL.get());

  cacheScreen->fillSector(m2::PointD(0, 0),
                          0, 2.0 * math::pi,
                          s_cacheRadius,
                          m_locationAreaColor,
                          graphics::locationFaultDepth);

  cacheScreen->setDisplayList(m_positionMarkDL.get());
  cacheScreen->drawSymbol(m2::PointD(0, 0),
                          "current-position",
                          graphics::EPosCenter,
                          graphics::locationDepth);

  cacheScreen->setDisplayList(0);

  cacheScreen->endFrame();
}

void State::CacheArrow(graphics::DisplayList * dl, const string & iconName)
{
  graphics::Screen * cacheScreen = m_controller->GetCacheScreen();
  graphics::Icon::Info info(iconName);

  graphics::Resource const * res = cacheScreen->fromID(cacheScreen->findInfo(info));
  m2::RectU const rect = res->m_texRect;
  m2::PointD const halfArrowSize(rect.SizeX() / 2.0, rect.SizeY() / 2.0);

  cacheScreen->beginFrame();
  cacheScreen->setDisplayList(dl);

  m2::PointD coords[4] =
  {
    m2::PointD(-halfArrowSize.x, -halfArrowSize.y),
    m2::PointD(-halfArrowSize.x,  halfArrowSize.y),
    m2::PointD( halfArrowSize.x, -halfArrowSize.y),
    m2::PointD( halfArrowSize.x,  halfArrowSize.y)
  };

  m2::PointF const normal(0.0, 0.0);
  shared_ptr<graphics::gl::BaseTexture> texture = cacheScreen->pipeline(res->m_pipelineID).texture();

  m2::PointF texCoords[4] =
  {
    texture->mapPixel(m2::PointF(rect.minX(), rect.minY())),
    texture->mapPixel(m2::PointF(rect.minX(), rect.maxY())),
    texture->mapPixel(m2::PointF(rect.maxX(), rect.minY())),
    texture->mapPixel(m2::PointF(rect.maxX(), rect.maxY()))
  };

  cacheScreen->addTexturedStripStrided(coords, sizeof(m2::PointD),
                                       &normal, 0,
                                       texCoords, sizeof(m2::PointF),
                                       4, graphics::locationDepth, res->m_pipelineID);
  cacheScreen->setDisplayList(0);
  cacheScreen->endFrame();
}

bool State::IsRotationActive() const
{
  return IsDirectionKnown();
}

bool State::IsDirectionKnown() const
{
  return TestModeBit(m_modeInfo, KnownDirectionBit);
}

bool State::IsInRouting() const
{
  return TestModeBit(m_modeInfo, RoutingSessionBit);
}

m2::PointD const State::GetModeDefaultPixelBinding(location::EMyPositionMode mode) const
{
  switch (mode)
  {
  case location::MODE_FOLLOW: return GetModelView().PixelRect().Center();
  case location::MODE_ROTATE_AND_FOLLOW: return GetRaFModeDefaultPxBind();
  default: return m2::PointD(0.0, 0.0);
  }
}

bool State::FollowCompass()
{
  if (!IsRotationActive() || GetMode() != location::MODE_ROTATE_AND_FOLLOW || m_animTask == nullptr)
    return false;

  RotateAndFollowAnim * task = static_cast<RotateAndFollowAnim *>(m_animTask.get());
  task->SetDestinationParams(Position(), -m_drawDirection);
  return true;
}

void State::CreateAnimTask()
{
  CreateAnimTask(GetModelView().GtoP(Position()),
                 GetModeDefaultPixelBinding(GetMode()));
}

void State::CreateAnimTask(const m2::PointD & srcPx, const m2::PointD & dstPx)
{
  EndAnimation();
  m_animTask.reset(new RotateAndFollowAnim(m_animator, Position(),
                                           GetModelView().GetAngle(),
                                           srcPx, dstPx));
  m_controller->PushAnimTask(m_animTask);
}

void State::EndAnimation()
{
  if (m_animTask != nullptr)
  {
    m_animTask->End();
    m_animTask.reset();
  }
}

void State::SetModeInfo(uint16_t modeInfo, bool callListeners)
{
  location::EMyPositionMode const newMode = ExcludeAllBits(modeInfo);
  location::EMyPositionMode const oldMode = GetMode();
  m_modeInfo = modeInfo;
  if (newMode != oldMode)
  {
    Settings::Set(LocationStateMode, static_cast<int>(GetMode()));

    if (callListeners)
      CallStateModeListeners();

    AnimateStateTransition(oldMode, newMode);
    invalidate();
  }
}

void State::StopAllAnimations()
{
  EndAnimation();
  m_animator->StopRotation();
  m_animator->StopMoveScreen();
}

ScreenBase const & State::GetModelView() const
{
  return m_controller->GetModelView();
}

m2::PointD const State::GetRaFModeDefaultPxBind() const
{
  m2::RectD const & pixelRect = GetModelView().PixelRect();
  return m2::PointD(pixelRect.Center().x,
                    pixelRect.maxY() - POSITION_Y_OFFSET * visualScale());
}

void State::StopCompassFollowing()
{
  if (GetMode() != location::MODE_ROTATE_AND_FOLLOW)
    return;

  StopAllAnimations();
  SetModeInfo(ChangeMode(m_modeInfo, location::MODE_FOLLOW));
}

void State::StopLocationFollow(bool callListeners)
{
  location::EMyPositionMode const currentMode = GetMode();
  if (currentMode > location::MODE_NOT_FOLLOW)
  {
    StopAllAnimations();
    SetModeInfo(ChangeMode(m_modeInfo, location::MODE_NOT_FOLLOW), callListeners);
  }
  else if (currentMode == location::MODE_PENDING_POSITION)
  {
    StopAllAnimations();
    m_afterPendingMode = location::MODE_NOT_FOLLOW;
  }
}

void State::SetFixedZoom()
{
  SetModeInfo(IncludeModeBit(m_modeInfo, FixedZoomBit));
}

void State::DragStarted()
{
  m_dragModeInfo = m_modeInfo;
  m_afterPendingMode = location::MODE_FOLLOW;
  StopLocationFollow(false);
}

void State::DragEnded()
{
  location::EMyPositionMode const currentMode = ExcludeAllBits(m_dragModeInfo);
  if (currentMode > location::MODE_NOT_FOLLOW)
  {
    // reset GPS centering mode if we have dragged far from the current location
    ScreenBase const & s = GetModelView();
    m2::PointD const defaultPxBinding = GetModeDefaultPixelBinding(currentMode);
    m2::PointD const pxPosition = s.GtoP(Position());

    if (defaultPxBinding.Length(pxPosition) < s.GetMinPixelRectSize() / 5.0)
      SetModeInfo(m_dragModeInfo, false);
    else
      CallStateModeListeners();
  }

  AnimateFollow();

  m_dragModeInfo = 0;
}

void State::ScaleStarted()
{
  m_scaleModeInfo = m_modeInfo;
}

void State::CorrectScalePoint(m2::PointD & pt) const
{
  if (IsModeChangeViewport() || ExcludeAllBits(m_scaleModeInfo) > location::MODE_NOT_FOLLOW)
    pt = GetModelView().GtoP(Position());
}

void State::CorrectScalePoint(m2::PointD & pt1, m2::PointD & pt2) const
{
  if (IsModeChangeViewport() || ExcludeAllBits(m_scaleModeInfo) > location::MODE_NOT_FOLLOW)
  {
    m2::PointD const ptDiff = GetModelView().GtoP(Position()) - (pt1 + pt2) / 2;
    pt1 += ptDiff;
    pt2 += ptDiff;
  }
}

void State::ScaleEnded()
{
  m_scaleModeInfo = 0;
}

void State::Rotated()
{
  m_afterPendingMode = location::MODE_NOT_FOLLOW;
  EndAnimation();
  if (GetMode() == location::MODE_ROTATE_AND_FOLLOW)
    SetModeInfo(ChangeMode(m_modeInfo, location::MODE_NOT_FOLLOW));
}

void State::OnCompassTaped()
{
  StopCompassFollowing();
  RotateOnNorth();
  AnimateFollow();
}

void State::OnSize(m2::RectD const & /*oldPixelRect*/)
{
  if (GetMode() == location::MODE_ROTATE_AND_FOLLOW)
  {
    EndAnimation();
    CreateAnimTask(GetModelView().GtoP(Position()), GetModeDefaultPixelBinding(GetMode()));
  }
}

void State::AnimateStateTransition(location::EMyPositionMode oldMode, location::EMyPositionMode newMode)
{
  StopAllAnimations();

  if (oldMode == location::MODE_PENDING_POSITION && newMode == location::MODE_FOLLOW)
  {
    if (!TestModeBit(m_modeInfo, FixedZoomBit))
    {
      m2::PointD const size(m_errorRadius, m_errorRadius);
      m_animator->ShowRectVisibleScale(m2::RectD(m_position - size, m_position + size),
                                       scales::GetUpperComfortScale());
    }
  }
  else if (newMode == location::MODE_ROTATE_AND_FOLLOW)
  {
    CreateAnimTask();
  }
  else if (oldMode == location::MODE_ROTATE_AND_FOLLOW && newMode == location::MODE_UNKNOWN_POSITION)
  {
    RotateOnNorth();
  }
  else if (oldMode == location::MODE_NOT_FOLLOW && newMode == location::MODE_FOLLOW)
  {
    m2::AnyRectD screenRect = GetModelView().GlobalRect();
    m2::RectD const & clipRect = GetModelView().ClipRect();
    screenRect.Inflate(clipRect.SizeX() / 2.0, clipRect.SizeY() / 2.0);
    if (!screenRect.IsPointInside(m_position))
      m_animator->SetViewportCenter(m_position);
  }

  AnimateFollow();
}

void State::AnimateFollow()
{
  if (!IsModeChangeViewport())
    return;

  SetModeInfo(ExcludeModeBit(m_modeInfo, FixedZoomBit));

  if (!FollowCompass())
  {
    m2::PointD const & mercatorCenter = GetModelView().GetOrg();
    if (!m_position.EqualDxDy(mercatorCenter, POSITION_TOLERANCE))
      m_animator->MoveScreen(mercatorCenter, m_position);
  }
}

void State::RotateOnNorth()
{
  m_animator->RotateScreen(GetModelView().GetAngle(), 0.0);
}

void State::Assign(location::GpsInfo const & info, bool isNavigable)
{
  m2::RectD rect = MercatorBounds::MetresToXY(info.m_longitude,
                                              info.m_latitude,
                                              info.m_horizontalAccuracy);
  m_position = rect.Center();
  m_errorRadius = rect.SizeX() / 2;

  bool const hasBearing = info.HasBearing();
  if ((isNavigable && hasBearing)
      || (!isNavigable && hasBearing && info.HasSpeed() && info.m_speed > 1.0))
  {
    SetDirection(my::DegToRad(info.m_bearing));
    m_lastGPSBearing.Reset();
  }
}

bool State::Assign(location::CompassInfo const & info)
{
  if ((IsInRouting() && GetMode() >= location::MODE_FOLLOW) ||
      (m_lastGPSBearing.ElapsedSeconds() < GPS_BEARING_LIFETIME_S))
    return false;

  SetDirection(info.m_bearing);
  return true;
}

void State::SetDirection(double bearing)
{
  m_drawDirection = bearing;
  SetModeInfo(IncludeModeBit(m_modeInfo, KnownDirectionBit));
}

void State::ResetDirection()
{
  SetModeInfo(ExcludeModeBit(m_modeInfo, KnownDirectionBit));
}

m2::PointD const State::GetPositionForDraw() const
{
  if (m_animTask != nullptr)
    return GetModelView().GtoP(static_cast<RotateAndFollowAnim *>(m_animTask.get())->GetPositionForDraw());

  return pivot();
}

}
