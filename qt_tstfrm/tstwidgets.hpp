#pragma once

#include "qt/rg_ogl_context.hpp"

#include "graphics/opengl/texture.hpp"
#include "graphics/opengl/renderbuffer.hpp"
#include "graphics/opengl/framebuffer.hpp"

#include "graphics/screen.hpp"

#include "graphics/resource_manager.hpp"

#include "std/shared_ptr.hpp"

#include <QtWidgets/QOpenGLWidget>

namespace graphics
{

namespace gl
{

class Screen;

} // namespace gl

} // namespace graphics

namespace tst
{

class GLDrawWidget : public QOpenGLWidget
{
public:

  shared_ptr<graphics::ResourceManager> m_resourceManager;

  shared_ptr<graphics::gl::FrameBuffer> m_primaryFrameBuffer;
  shared_ptr<rg::OGLContext> m_primaryContext;
  shared_ptr<graphics::Screen> m_primaryScreen;

  virtual void initializeGL();
  virtual void resizeGL(int w, int h);
  virtual void paintGL();

public:

  virtual void DoDraw(shared_ptr<graphics::Screen> const & screen) = 0;
};

} // namespace tst
