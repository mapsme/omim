#pragma once

#include "events.hpp"
#include "drawer.hpp"

#include "platform/video_timer.hpp"

#include "base/logging.hpp"

#include "std/shared_ptr.hpp"
#include "std/atomic.hpp"

namespace graphics
{

class RenderContext;

} // namespace graphics

namespace rg
{

class RenderPolicy;

class WindowHandle
{
  shared_ptr<graphics::RenderContext> m_renderContext;
  RenderPolicy * m_renderPolicy;

  bool m_hasPendingUpdates;
  atomic<bool> m_isUpdatesEnabled;
  atomic<bool> m_needRedraw;

  VideoTimer * m_videoTimer;
  VideoTimer::TFrameFn m_frameFn;
  int m_stallsCount;

public:
  WindowHandle();
  virtual ~WindowHandle();

  void setRenderPolicy(RenderPolicy * renderPolicy);
  void setVideoTimer(VideoTimer * videoTimer);

  void checkedFrameFn();

  bool needRedraw() const;

  void checkTimer();

  void setNeedRedraw(bool flag);

  shared_ptr<graphics::RenderContext> const & renderContext();

  void setRenderContext(shared_ptr<graphics::RenderContext> const & renderContext);

  bool setUpdatesEnabled(bool doEnable);

  void invalidate();
};

} // namespace rg
