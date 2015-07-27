#pragma once

#include "gui/element.hpp"

#include "std/shared_ptr.hpp"
#include "std/unique_ptr.hpp"
#include "std/function.hpp"

namespace anim
{

class Task;

} // namespace anim

namespace graphics
{

class DisplayList;

namespace gl
{

class OverlayRenderer;

} // namespace gl

} //namespace graphics

namespace rg
{

class Ruler : public gui::Element
{
  typedef gui::Element BaseT;

  class RulerFrame
  {
  public:
    typedef function<void (bool isVisible, RulerFrame *)> frame_end_fn;

    RulerFrame(gui::Controller * controller,
               frame_end_fn const & fn,
               double depth);

    RulerFrame(RulerFrame const & other, const frame_end_fn & fn);
    ~RulerFrame();

    bool IsValid() const;

    void Cache(const string & text, const graphics::FontDesc & f);
    void Purge();
    bool IsHidingAnim() const;
    bool IsAnimActive() const;
    void SetScale(double scale);
    double GetScale() const;
    void SetOrgPoint(m2::PointD const & org);
    m2::PointD const & GetOrgPoint() const;

    void ShowAnimate(bool needPause);
    void HideAnimate(bool needPause);
    void Draw(graphics::OverlayRenderer * r, math::Matrix<double, 3, 3> const & m);

  private:
    void CreateAnim(double startAlfa, double endAlfa,
                    double timeInterval, double timeOffset,
                    bool isVisibleAtEnd);
    float GetCurrentAlfa();
    void AnimEnded(bool isVisible);

  private:
    shared_ptr<graphics::DisplayList> m_dl;
    shared_ptr<graphics::DisplayList> m_textDL;

    int m_textLengthInPx;
    double m_scale;
    double m_depth;
    m2::PointD m_orgPt;
    frame_end_fn m_callback;
    gui::Controller * m_controller;

    shared_ptr<anim::Task> m_frameAnim;
  };

private:
  int m_currentRangeIndex;
  int m_currSystem;
  double CalcMetresDiff(double value);
  void UpdateText(const string & text);

  void MainFrameAnimEnded(bool isVisible, RulerFrame * frame);
  void AnimFrameAnimEnded(bool isVisible, RulerFrame * frame);

  RulerFrame * GetMainFrame();
  RulerFrame * GetMainFrame() const;

  unique_ptr<RulerFrame> m_mainFrame;
  unique_ptr<RulerFrame> m_animFrame;

public:
  Ruler(Params const & p);

  void AnimateShow();
  void AnimateHide();

  /// @name Override from graphics::OverlayElement and gui::Element.
  //@{
  virtual m2::RectD GetBoundRect() const;

  void draw(graphics::OverlayRenderer * r, math::Matrix<double, 3, 3> const & m) const;

  void update();
  void layout();
  void cache();
  void purge();
  //@}

  int GetTextOffsetFromLine() const;
};

} // namespace rg
