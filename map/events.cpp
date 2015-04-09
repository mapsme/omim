#include "events.hpp"

#ifndef USE_DRAPE
PaintEvent::PaintEvent(Drawer * drawer,
                       core::CommandsQueue::Environment const * env)
    : m_drawer(drawer),
      m_env(env),
      m_isCancelled(false),
      m_isEmptyDrawing(true),
      m_renderOffscreen(false)
{}

Drawer * PaintEvent::drawer() const
{
  return m_drawer;
}

void PaintEvent::cancel()
{
  ASSERT(m_env == 0, ());
  m_isCancelled = true;
}

bool PaintEvent::isCancelled() const
{
  if (m_env)
    return m_env->IsCancelled();
  else
    return m_isCancelled;
}

bool PaintEvent::isEmptyDrawing() const
{
  return m_isEmptyDrawing;
}

void PaintEvent::setIsEmptyDrawing(bool flag)
{
  m_isEmptyDrawing = flag;
}

bool PaintEvent::isOffscreenRendering() const
{
  return m_renderOffscreen;
}

ScreenBase const & PaintEvent::getOffscreenRect() const
{
  return m_offscreenRect;
}

void PaintEvent::setOffscreenRect(ScreenBase const & offscreenRect)
{
  m_offscreenRect = offscreenRect;
  m_renderOffscreen = true;
}

#endif // USE_DRAPE
