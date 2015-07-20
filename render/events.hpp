#pragma once

#include "geometry/screenbase.hpp"
#include "geometry/point2d.hpp"

#include "base/commands_queue.hpp"

namespace rg
{

class DragEvent
{
  m2::PointD m_pt;
public:
  DragEvent(double x, double y) : m_pt(x, y) {}
  DragEvent(m2::PointD const & pt) : m_pt(pt) {}
  inline m2::PointD const & Pos() const { return m_pt; }
};

class ScaleEvent
{
  m2::PointD m_Pt1, m_Pt2;
public:
  ScaleEvent(double x1, double y1, double x2, double y2) : m_Pt1(x1, y1), m_Pt2(x2, y2) {}
  ScaleEvent(m2::PointD const & pt1, m2::PointD const & pt2) : m_Pt1(pt1), m_Pt2(pt2) {}
  inline m2::PointD const & Pt1() const { return m_Pt1; }
  inline m2::PointD const & Pt2() const { return m_Pt2; }
};

class Drawer;

class PaintEvent
{
  Drawer * m_drawer;
  core::CommandsQueue::Environment const * m_env;
  bool m_isCancelled;
  bool m_isEmptyDrawing;

public:
  PaintEvent(Drawer * drawer, core::CommandsQueue::Environment const * env = 0);

  Drawer * drawer() const;
  void cancel();
  bool isCancelled() const;
  bool isEmptyDrawing() const;
  void setIsEmptyDrawing(bool flag);
};

class PaintOverlayEvent
{
public:
  PaintOverlayEvent(Drawer * drawer, ScreenBase const & modelView)
    : m_drawer(drawer), m_modelView(modelView) {}

  ScreenBase const & GetModelView() const { return m_modelView; }
  Drawer * GetDrawer() const { return m_drawer; }
  m2::RectD const & GetClipRect() const { return m_modelView.ClipRect(); }

private:
  Drawer * m_drawer;
  ScreenBase m_modelView;
};

} // namespace rg
