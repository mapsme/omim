#pragma once

#include "render/animator.hpp"

#include "gui/element.hpp"

#include "platform/location.hpp"

#include "geometry/point2d.hpp"

#include "base/timer.hpp"

#include "routing/turns.hpp"

#include "std/function.hpp"
#include "std/shared_ptr.hpp"
#include "std/unique_ptr.hpp"
#include "std/map.hpp"

class ScreenBase;

namespace graphics { class DisplayList; }
namespace anim { class Task;}

namespace rg
{

// Class, that handles position and compass updates,
// centers, scales and rotates map according to this updates
// and draws location and compass marks.
class State : public gui::Element
{
  using TBase = gui::Element;

public:
  struct Params : TBase::Params
  {
    graphics::Color m_locationAreaColor;
    Animator * m_animator;
    Params();
  };

  using TPositionListener = function<void (m2::PointD const &)>;

  State(Params const & p);

  /// @return GPS center point in mercator
  m2::PointD const & Position() const;
  double GetErrorRadius() const;
  double GetDirection() const { return m_drawDirection; }
  bool IsDirectionKnown() const;

  location::EMyPositionMode GetMode() const;
  bool IsModeChangeViewport() const;
  bool IsModeHasPosition() const;
  void SwitchToNextMode();

  void RouteBuilded();
  void StartRouteFollow();
  void StopRoutingMode();

  int  AddStateModeListener(location::TMyPositionModeChanged const & l);
  void RemoveStateModeListener(int slotID);

  int  AddPositionChangedListener(TPositionListener const & func);
  void RemovePositionChangedListener(int slotID);

  void InvalidatePosition();
  void TurnOff();
  void StopCompassFollowing();
  void StopLocationFollow(bool callListeners = true);
  void SetFixedZoom();

  /// @name User input notification block
  //@{
  void DragStarted();
  void DragEnded();

  void ScaleStarted();
  void CorrectScalePoint(m2::PointD & pt) const;
  void CorrectScalePoint(m2::PointD & pt1, m2::PointD & pt2) const;
  void ScaleEnded();

  void Rotated();
  //@}

  void OnCompassTaped();

  void OnSize(m2::RectD const & oldPixelRect);

  /// @name GPS location updates routine.
  //@{
  void OnLocationUpdate(location::GpsInfo const & info, bool isNavigable);
  void OnCompassUpdate(location::CompassInfo const & info);
  //@}

  location::RouteMatchingInfo const & GetRouteMatchingInfo() const { return m_routeMatchingInfo; }
  void ResetRouteMatchingInfo() { m_routeMatchingInfo.Reset(); }
  void ResetDirection();

  /// @name Override from graphics::OverlayElement and gui::Element.
  //@{
  virtual m2::RectD GetBoundRect() const { return m2::RectD(); }

  void draw(graphics::OverlayRenderer * r, math::Matrix<double, 3, 3> const & m) const;
  bool hitTest(m2::PointD const & /*pt*/) const { return false; }

  void cache();
  void purge();
  void update();
  //@}

  void RotateOnNorth();

private:
  void AnimateStateTransition(location::EMyPositionMode oldMode, location::EMyPositionMode newMode);
  void AnimateFollow();


  void CallPositionChangedListeners(m2::PointD const & pt);
  void CallStateModeListeners();

  void CachePositionArrow();
  void CacheRoutingArrow();
  void CacheLocationMark();

  void CacheArrow(graphics::DisplayList * dl, string const & iconName);

  bool IsRotationActive() const;
  bool IsInRouting() const;

  m2::PointD const GetModeDefaultPixelBinding(location::EMyPositionMode mode) const;
  m2::PointD const GetRaFModeDefaultPxBind() const;

  void SetModeInfo(uint16_t modeInfo, bool callListeners = true);

  void StopAllAnimations();

  ScreenBase const & GetModelView() const;

  void Assign(location::GpsInfo const & info, bool isNavigable);
  bool Assign(location::CompassInfo const & info);
  void SetDirection(double bearing);
  const m2::PointD GetPositionForDraw() const;

private:
  // Mode bits
  // {
  static uint16_t const FixedZoomBit = 0x20;
  static uint16_t const RoutingSessionBit = 0x40;
  static uint16_t const KnownDirectionBit = 0x80;
  // }
  static uint16_t const s_cacheRadius = 500.0f;

  uint16_t m_modeInfo; // combination of Mode enum and "Mode bits"
  uint16_t m_dragModeInfo = 0;
  uint16_t m_scaleModeInfo = 0;

  double m_errorRadius;   //< error radius in mercator
  m2::PointD m_position;  //< position in mercator
  double m_drawDirection;
  my::Timer m_lastGPSBearing;
  location::EMyPositionMode m_afterPendingMode;

  location::RouteMatchingInfo m_routeMatchingInfo;

  using TModeListeners = map<int, location::TMyPositionModeChanged>;
  using TPositionListeners = map<int, TPositionListener>;

  TModeListeners m_modeListeners;
  TPositionListeners m_positionListeners;
  int m_currentSlotID;

  /// @name Compass Rendering Parameters
  //@{
  unique_ptr<graphics::DisplayList> m_positionArrow;
  unique_ptr<graphics::DisplayList> m_locationMarkDL;
  unique_ptr<graphics::DisplayList> m_positionMarkDL;
  unique_ptr<graphics::DisplayList> m_routingArrow;
  graphics::Color m_locationAreaColor;
  //@}

  /// @name Rotation mode animation
  //@{
  shared_ptr<anim::Task> m_animTask;
  bool FollowCompass();
  void CreateAnimTask();
  void CreateAnimTask(m2::PointD const & srcPx, m2::PointD const & dstPx);
  void EndAnimation();
  //@}

  Animator * m_animator;
};

} // rg
