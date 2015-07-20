#pragma once

#include "render/scales_processor.hpp"
#include "render/navigator.hpp"
#include "render/render_policy.hpp"

#include "std/unique_ptr.hpp"
#include "std/function.hpp"

class StringsBundle;
namespace gui { class Controller; }
namespace anim { class Controller; }

class FeatureType;

namespace rg
{

class RenderPolicy;

class Engine
{
public:
  using TParseFn = function<bool (FeatureType const &)>;
  using TDrawFn = function<void (TParseFn const &, m2::RectD const &, int, bool)>;
  using TScreenChangedFn = function<void (ScreenBase const &)>;

  struct Params
  {
    RenderPolicy::Params m_rpParams;
    TDrawFn m_drawFn;
    bool m_initGui = true;
  };

  Engine(Params && params);
  ~Engine();

  void Scale(double factor, m2::PointD const & pxPoint, bool isAnim);

  void InitGui(StringsBundle const & bundle);
  void Resize(int w, int h);
  void DrawFrame();

  void ShowRect(m2::AnyRectD const & rect);
  int AddModelViewListener(TScreenChangedFn const & listener);
  void RemoveModelViewListener(int slotID);

  int GetDrawScale();

private:
  void BeginPaint(shared_ptr<PaintEvent> const & e);
  void DoPaint(shared_ptr<PaintEvent> const & e);
  void EndPaint(shared_ptr<PaintEvent> const & e);

  void DrawModel(shared_ptr<PaintEvent> const & e, ScreenBase const & screen,
                 m2::RectD const & renderRect, int baseScale);

  void Invalidate(bool forceUpdate = true);
  void Invalidate(m2::RectD const & rect, bool forceUpdate = true);

  int GetWidth() const;
  int GetHeight() const;

private:
  unique_ptr<gui::Controller> m_guiController; // can be nullptr if gui subsystem not initialized
  unique_ptr<RenderPolicy> m_renderPolicy;

  ScalesProcessor m_scales;
  Navigator m_navigator;
  TDrawFn m_drawFn;
};

}
