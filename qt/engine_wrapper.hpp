#pragma once

#include "qt/qtoglcontextfactory.hpp"

#include "map/framework.hpp"

#include "platform/video_timer.hpp"

#include "std/mutex.hpp"
#include "std/condition_variable.hpp"

#include <QtGui/QOpenGLWindow>

#include <QtCore/QObject>
#include <QtCore/QTimer>

namespace qt
{

class EngineWrapper
{
public:
  EngineWrapper(Framework & framework) : m_framework(framework) {}
  virtual ~EngineWrapper() {}

  virtual void Create(QOpenGLWindow * window) = 0;
  virtual void Paint() = 0;
  virtual void Destroy() = 0;
  virtual void Expose() {}

protected:
  Framework & m_framework;
};

class DrapeEngineWrapper : public QObject, public EngineWrapper
{
  Q_OBJECT
public:
  DrapeEngineWrapper(Framework & framework);

  void Create(QOpenGLWindow * window) override;
  void Paint() override;
  void Destroy() override;
  void Expose() override;

private:
  enum RenderingState
  {
    NotInitialized,
    WaitContext,
    WaitSwap,
    Render,
  };

  void CallSwap();
  void CallRegisterThread(QThread * thread);
  Q_SIGNAL void Swap();
  Q_SIGNAL void RegRenderingThread(QThread * thread);
  Q_SLOT void OnSwap();
  Q_SLOT void OnRegRenderingThread(QThread * thread);
  Q_SLOT void FrameSwappedSlot(RenderingState state = Render);

  void MoveContextToRenderThread();
  QThread * m_rendererThread;

  mutex m_swapMutex;
  condition_variable m_swapCond;

  mutex m_waitContextMutex;
  condition_variable m_waitContextCond;

  RenderingState m_state;

private:
  QOpenGLWindow * m_window;
  QOpenGLContext * m_ctx;
  drape_ptr<QtOGLContextFactory> m_contextFactory;
};

class QVideoTimer : public QObject,
                    public VideoTimer
{
  Q_OBJECT
public:
  QVideoTimer(QOpenGLWindow * window);
  ~QVideoTimer();

  void start();
  void pause();
  void resume();
  void stop();
  void paint();

private:
  QTimer m_timer;

  Q_SLOT void timerShoot();
  Q_SIGNAL void update();
};

class RGEngineWrapper : public EngineWrapper
{
  using TBase = EngineWrapper;
public:
  RGEngineWrapper(Framework & framework);

  void Create(QOpenGLWindow * window) override;
  void Paint() override;
  void Destroy() override;

private:
  unique_ptr<QVideoTimer> m_videoTimer;
};

}
