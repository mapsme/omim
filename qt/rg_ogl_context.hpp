#pragma once

#include "graphics/opengl/gl_render_context.hpp"

#include "std/vector.hpp"

#include <QtGui/QOpenGLContext>

namespace rg
{

class OGLContext : public graphics::gl::RenderContext
{
  using TBase = graphics::gl::RenderContext;
public:
  OGLContext(QOpenGLContext * ctx, QSurface * surface);

  void makeCurrent() override;
  graphics::RenderContext * createShared() override;

  void endThreadDrawing(unsigned threadSlot);

private:
  QOpenGLContext * m_ctx;
  QSurface * m_surface;
};

}
