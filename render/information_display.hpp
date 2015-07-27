#pragma once

#include "render/active_maps_bridge.hpp"

#include "graphics/font_desc.hpp"

#include "storage/index.hpp"

#include "geometry/point2d.hpp"
#include "geometry/rect2d.hpp"

#include "std/shared_ptr.hpp"

namespace gui
{

class Controller;
class CachedTextView;

} // namesapce gui

namespace rg
{

class State;
class CountryStatusDisplay;
class CompassArrow;
class Ruler;
class Animator;

/// Class, which displays additional information on the primary layer like:
/// rules, coordinates, GPS position and heading, compass, Download button, etc.
class InformationDisplay
{
  graphics::FontDesc m_fontDesc;

  shared_ptr<Ruler> m_ruler;
  gui::Controller * m_controller;

  shared_ptr<CountryStatusDisplay> m_countryStatusDisplay;
  shared_ptr<CompassArrow> m_compassArrow;
  shared_ptr<rg::State> m_locationState;
  shared_ptr<gui::CachedTextView> m_debugLabel;
  shared_ptr<gui::CachedTextView> m_copyrightLabel;

  void InitRuler();
  void InitDebugLabel();
  void InitLocationState(Animator * animator);
  void InitCompassArrow();
  void InitCountryStatusDisplay(ActiveMapsBridge * mapsBridge);

  void InitCopyright();

  void OnCompassTapped();

public:
  enum class WidgetType
  {
    Ruler = 0,
    CopyrightLabel,
    CountryStatusDisplay,
    CompassArrow,
    DebugLabel
  };

  InformationDisplay(ActiveMapsBridge * mapsBridge, Animator * animator);

  void setController(gui::Controller * controller);
  /*!
   * \brief SetWidgetPivotsByDefault sets the default pivot points for all the widgets on the map.
   * The pivot points can be overridden by a call of SetWidgetPivot()
   * after Framework::OnSize() call.
   */
  void SetWidgetPivotsByDefault(int screenWidth, int screenHeight);
  void setVisualScale(double visualScale);

  bool isCopyrightActive() const;
  void enableCopyright(bool doEnable);

  void enableRuler(bool doEnable);
  bool isRulerEnabled() const;

  void enableDebugInfo(bool doEnable);
  void setDebugInfo(double frameDuration, int currentScale);

  void measurementSystemChanged();

  void enableCompassArrow(bool doEnable);
  bool isCompassArrowEnabled() const;
  void setCompassArrowAngle(double angle);

  shared_ptr<rg::State> const & locationState() const;

  void setEmptyCountryIndex(storage::TIndex const & idx);

  shared_ptr<CountryStatusDisplay> const & countryStatusDisplay() const;

  void ResetRouteMatchingInfo();

  void SetWidgetPivot(WidgetType widget, m2::PointD const & pivot);
  m2::PointD GetWidgetSize(WidgetType widget) const;
};

} // namespace rg
