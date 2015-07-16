#include "engine_wrapper.hpp"

#include "rg_ogl_context.hpp"

#include <QtWidgets/QApplication>
#include <QtWidgets/QDesktopWidget>

namespace qt
{

DrapeEngineWrapper::DrapeEngineWrapper(Framework & framework)
  : EngineWrapper(framework)
  , m_rendererThread(nullptr)
  , m_state(NotInitialized)
  , m_window(nullptr)
  , m_ctx(nullptr)
{
}

void DrapeEngineWrapper::Create(QOpenGLWindow * window)
{
  m_window = window;
  m_ctx = window->context();

  Qt::ConnectionType swapType = Qt::QueuedConnection;
  Qt::ConnectionType regType = Qt::BlockingQueuedConnection;
  VERIFY(connect(this, SIGNAL(Swap()), SLOT(OnSwap()), swapType), ());
  VERIFY(connect(this, SIGNAL(RegRenderingThread(QThread *)), SLOT(OnRegRenderingThread(QThread *)), regType), ());
  VERIFY(connect(window, SIGNAL(frameSwapped()), SLOT(FrameSwappedSlot())), ());

  ASSERT(m_contextFactory == nullptr, ());
  QtOGLContextFactory::TRegisterThreadFn regFn = bind(&DrapeEngineWrapper::CallRegisterThread, this, _1);
  QtOGLContextFactory::TSwapFn swapFn = bind(&DrapeEngineWrapper::CallSwap, this);
  m_contextFactory = make_unique_dp<QtOGLContextFactory>(m_ctx, m_window, QThread::currentThread(), regFn, swapFn);

  float ratio = m_window->devicePixelRatio();
  Framework::DrapeCreationParams p;
  p.m_surfaceWidth = ratio * m_window->width();
  p.m_surfaceHeight = ratio * m_window->height();
  p.m_visualScale = ratio;

  gui::Skin skin(gui::ResolveGuiSkinFile("default"), ratio);
  skin.Resize(p.m_surfaceWidth, p.m_surfaceHeight);
  skin.ForEach([&p](gui::EWidget widget, gui::Position const & pos)
  {
    p.m_widgetsInitInfo[widget] = pos;
  });

  p.m_widgetsInitInfo[gui::WIDGET_SCALE_LABLE] = gui::Position(dp::LeftBottom);

  m_framework.CreateDrapeEngine(make_ref(m_contextFactory), move(p));
}

void DrapeEngineWrapper::Paint() {}

void DrapeEngineWrapper::Destroy()
{
  // Discard current and all future Swap requests
  m_contextFactory->shutDown();
  FrameSwappedSlot(NotInitialized);

  // Shutdown engine. FR have ogl context in this moment and can delete OGL resources
  // PrepareToShutdown make FR::join and after this call we can bind OGL context to gui thread

  m_framework.PrepareToShutdown();
  m_contextFactory.reset();
}

void DrapeEngineWrapper::Expose()
{
  m_swapMutex.lock();
  if (m_state == Render)
  {
    unique_lock<mutex> waitContextLock(m_waitContextMutex);
    m_state = WaitContext;
    m_swapMutex.unlock();

    m_waitContextCond.wait(waitContextLock, [this](){ return m_state != WaitContext; });
  }
  else
    m_swapMutex.unlock();
}

void DrapeEngineWrapper::CallSwap()
{
  // Called on FR thread. In this point OGL context have already moved into GUI thread.
  unique_lock<mutex> lock(m_swapMutex);
  if (m_state == NotInitialized)
  {
    // This can be in two cases if GUI thread in PrepareToShutDown
    return;
  }

  ASSERT(m_state != WaitSwap, ());
  if (m_state == WaitContext)
  {
    lock_guard<mutex> waitContextLock(m_waitContextMutex);
    m_state = Render;
    m_waitContextCond.notify_one();
  }

  if (m_state == Render)
  {
    // We have to wait, while Qt on GUI thread finish composing widgets and make SwapBuffers
    // After SwapBuffers Qt will call our SLOT(frameSwappedSlot)
    m_state = WaitSwap;
    emit Swap();
    m_swapCond.wait(lock, [this]() { return m_state != WaitSwap; });
  }
}

void DrapeEngineWrapper::CallRegisterThread(QThread * thread)
{
  // Called on FR thread. SIGNAL(RegRenderingThread) and SLOT(OnRegRenderingThread)
  // connected by through Qt::BlockingQueuedConnection and we don't need any synchronization
  ASSERT(m_state == NotInitialized, ());
  emit RegRenderingThread(thread);
}

void DrapeEngineWrapper::OnSwap()
{
  // Called on GUI thread. In this point FR thread must wait SwapBuffers signal
  lock_guard<mutex> lock(m_swapMutex);
  if (m_state == WaitSwap)
  {
    m_ctx->makeCurrent(m_window);
    m_window->update();
  }
}

void DrapeEngineWrapper::OnRegRenderingThread(QThread * thread)
{
  // Called on GUI thread.
  // Here we register thread of FR, to return OGL context into it after SwapBuffers
  // After this operation we can start rendering into back buffer on FR thread
  lock_guard<mutex> lock(m_swapMutex);

  ASSERT(m_state == NotInitialized, ());
  m_state = Render;
  m_rendererThread = thread;
  MoveContextToRenderThread();
}

void DrapeEngineWrapper::FrameSwappedSlot(DrapeEngineWrapper::RenderingState state)
{
  // Qt call this slot on GUI thread after glSwapBuffers perfomed
  // Here we move OGL context into FR thread and wake up FR
  lock_guard<mutex> lock(m_swapMutex);

  if (m_state == WaitSwap)
  {
    MoveContextToRenderThread();
    m_state = state;
    m_swapCond.notify_all();
  }
}

void DrapeEngineWrapper::MoveContextToRenderThread()
{
  m_ctx->doneCurrent();
  m_ctx->moveToThread(m_rendererThread);
}

QVideoTimer::QVideoTimer(QOpenGLWindow * window)
  : VideoTimer(TFrameFn())
{
  VERIFY(window->connect(this, SIGNAL(update()), SLOT(update())), ());
  VERIFY(connect(&m_timer, SIGNAL(timeout()), SLOT(timerShoot())), ());

  m_timer.setInterval(1000 / 60);
  m_timer.setSingleShot(false);
  m_timer.start();
}

QVideoTimer::~QVideoTimer()
{
  m_timer.stop();
}

void QVideoTimer::start()
{
  resume();
}

void QVideoTimer::pause()
{
  m_state = EPaused;
}

void QVideoTimer::resume()
{
  m_state = ERunning;
}

void QVideoTimer::stop()
{
  m_state = EStopped;
}

void QVideoTimer::paint()
{
  m_frameFn();
}

void QVideoTimer::timerShoot()
{
  if (m_state == ERunning)
    emit update();
}

RGEngineWrapper::RGEngineWrapper(Framework & framework)
  : TBase(framework)
{
}

void RGEngineWrapper::Create(QOpenGLWindow * window)
{
  rg::Engine::Params params;
  params.m_initGui = true;
  params.m_rpParams.m_rmParams.m_texFormat = graphics::Data8Bpp;
  params.m_rpParams.m_rmParams.m_texRtFormat = graphics::Data4Bpp;

  float ratio = window->devicePixelRatio();

  QRect const & geometry = QApplication::desktop()->geometry();
  params.m_rpParams.m_screenWidth = ratio * geometry.width();
  params.m_rpParams.m_screenHeight = ratio * geometry.height();

  params.m_rpParams.m_density = graphics::EDensityXHDPI;
  if (ratio < 2.0 && QApplication::desktop()->physicalDpiX() < 180)
    params.m_rpParams.m_density = graphics::EDensityMDPI;

  m_videoTimer.reset(new QVideoTimer(window));

  params.m_rpParams.m_useDefaultFB = true;
  params.m_rpParams.m_primaryRC = shared_ptr<graphics::RenderContext>(new rg::OGLContext(window->context(), window));
  params.m_rpParams.m_videoTimer = m_videoTimer.get();
  params.m_rpParams.m_skinName = "symbols.sdf";

  m_framework.CreateRGEngine(move(params));
}

void RGEngineWrapper::Paint()
{
  ASSERT(m_videoTimer, ());
  m_videoTimer->paint();
}

void RGEngineWrapper::Destroy()
{
  m_framework.PrepareToShutdown();
  m_videoTimer.reset();
}

}
