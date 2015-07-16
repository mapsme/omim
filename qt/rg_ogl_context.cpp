#include "rg_ogl_context.hpp"

#include <QtCore/QThread>
#include <QtGui/QSurface>
#include <QtGui/QOffscreenSurface>

namespace rg
{

namespace
{

class OGLSharedContext : public graphics::gl::RenderContext
{
  using TBase = graphics::gl::RenderContext;

public:
  OGLSharedContext(QOpenGLContext * sharedCtx)
    : m_sharedCtx(sharedCtx)
  {
  }

  ~OGLSharedContext()
  {
    QThread * currentThread = QThread::currentThread();
    ASSERT(m_ctx->thread() == currentThread, ());
    delete m_ctx;

    ASSERT(m_surface->thread() == currentThread, ());
    m_surface->destroy();
    delete m_surface;
  }

  void makeCurrent() override
  {
    if (m_ctx == nullptr)
    {
      m_ctx = new QOpenGLContext();
      m_ctx->setFormat(m_sharedCtx->format());
      m_ctx->setShareContext(m_sharedCtx);

      VERIFY(m_ctx->create(), ());

      m_surface = new QOffscreenSurface(m_sharedCtx->screen());
      m_surface->setFormat(m_sharedCtx->format());
      m_surface->create();
    }

    m_ctx->makeCurrent(m_surface);
  }

  void endThreadDrawing(unsigned threadSlot)
  {
    m_ctx->doneCurrent();
    TBase::endThreadDrawing(threadSlot);
  }

  graphics::RenderContext * createShared()
  {
    return new OGLSharedContext(m_sharedCtx);
  }

private:
  QOpenGLContext * m_sharedCtx;
  QOpenGLContext * m_ctx;
  QOffscreenSurface * m_surface;
};

} // namespace

OGLContext::OGLContext(QOpenGLContext * ctx, QSurface * surface)
  : m_ctx(ctx)
  , m_surface(surface)
{
}

void OGLContext::makeCurrent()
{
  m_ctx->makeCurrent(m_surface);
}

graphics::RenderContext * OGLContext::createShared()
{
  return new OGLSharedContext(m_ctx);
}

void OGLContext::endThreadDrawing(unsigned threadSlot)
{
  m_ctx->doneCurrent();
  TBase::endThreadDrawing(threadSlot);
}

} // namespace rg
