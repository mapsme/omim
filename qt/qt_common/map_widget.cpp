#include "qt/qt_common/map_widget.hpp"

#include "qt/qt_common/helpers.hpp"
#include "qt/qt_common/scale_slider.hpp"

#include "map/framework.hpp"

#include "geometry/point2d.hpp"

#include "base/assert.hpp"

#include <functional>
#include <string>

#include <QtGui/QMouseEvent>
#include <QtGui/QOpenGLFunctions>
#include <QtGui/QOpenGLShaderProgram>
#include <QtWidgets/QAction>
#include <QtWidgets/QMenu>

#include <QtGui/QOpenGLBuffer>
#include <QtGui/QOpenGLVertexArrayObject>

namespace qt
{
namespace common
{
//#define ENABLE_AA_SWITCH

MapWidget::MapWidget(Framework & framework, bool apiOpenGLES3, bool isScreenshotMode, QWidget * parent)
  : QOpenGLWidget(parent)
  , m_framework(framework)
  , m_apiOpenGLES3(apiOpenGLES3)
  , m_screenshotMode(isScreenshotMode)
  , m_slider(nullptr)
  , m_sliderState(SliderState::Released)
  , m_ratio(1.0)
  , m_contextFactory(nullptr)
{
  setMouseTracking(true);
  // Update widget contents each 30ms.
  m_updateTimer = std::make_unique<QTimer>(this);
  VERIFY(connect(m_updateTimer.get(), SIGNAL(timeout()), this, SLOT(update())), ());
  m_updateTimer->setSingleShot(false);
  m_updateTimer->start(30);
}

MapWidget::~MapWidget()
{
  m_framework.EnterBackground();
  m_framework.SetRenderingDisabled(true);
  m_contextFactory->PrepareToShutdown();
  m_framework.DestroyDrapeEngine();
  m_contextFactory.reset();
}

void MapWidget::BindHotkeys(QWidget & parent)
{
  Hotkey const hotkeys[] = {
      {Qt::Key_Equal, SLOT(ScalePlus())},
      {Qt::Key_Minus, SLOT(ScaleMinus())},
      {Qt::ALT + Qt::Key_Equal, SLOT(ScalePlusLight())},
      {Qt::ALT + Qt::Key_Minus, SLOT(ScaleMinusLight())},
      {Qt::ALT + Qt::Key_Minus, SLOT(ScaleMinusLight())},
#ifdef ENABLE_AA_SWITCH
      {Qt::ALT + Qt::Key_A, SLOT(AntialiasingOn())},
      {Qt::ALT + Qt::Key_S, SLOT(AntialiasingOff())},
#endif
  };

  for (auto const & hotkey : hotkeys)
  {
    auto action = std::make_unique<QAction>(&parent);
    action->setShortcut(QKeySequence(hotkey.m_key));
    connect(action.get(), SIGNAL(triggered()), this, hotkey.m_slot);
    parent.addAction(action.release());
  }
}

void MapWidget::BindSlider(ScaleSlider & slider)
{
  m_slider = &slider;

  connect(m_slider, SIGNAL(actionTriggered(int)), this, SLOT(ScaleChanged(int)));
  connect(m_slider, SIGNAL(sliderPressed()), this, SLOT(SliderPressed()));
  connect(m_slider, SIGNAL(sliderReleased()), this, SLOT(SliderReleased()));
}

void MapWidget::CreateEngine()
{
  Framework::DrapeCreationParams p;

  p.m_apiVersion = m_apiOpenGLES3 ? dp::ApiVersion::OpenGLES3 : dp::ApiVersion::OpenGLES2;

  p.m_surfaceWidth = m_screenshotMode ? width() : static_cast<int>(m_ratio * width());
  p.m_surfaceHeight = m_screenshotMode ? height() : static_cast<int>(m_ratio * height());
  p.m_visualScale = static_cast<float>(m_ratio);
  p.m_hints.m_screenshotMode = m_screenshotMode;

  m_skin.reset(new gui::Skin(gui::ResolveGuiSkinFile("default"), m_ratio));
  m_skin->Resize(p.m_surfaceWidth, p.m_surfaceHeight);
  m_skin->ForEach(
      [&p](gui::EWidget widget, gui::Position const & pos) { p.m_widgetsInitInfo[widget] = pos; });

  p.m_widgetsInitInfo[gui::WIDGET_SCALE_FPS_LABEL] = gui::Position(dp::LeftBottom);

  m_framework.CreateDrapeEngine(make_ref(m_contextFactory), std::move(p));
  m_framework.SetViewportListener(std::bind(&MapWidget::OnViewportChanged, this, std::placeholders::_1));
}

void MapWidget::ScalePlus() { m_framework.Scale(Framework::SCALE_MAG, true); }

void MapWidget::ScaleMinus() { m_framework.Scale(Framework::SCALE_MIN, true); }

void MapWidget::ScalePlusLight() { m_framework.Scale(Framework::SCALE_MAG_LIGHT, true); }

void MapWidget::ScaleMinusLight() { m_framework.Scale(Framework::SCALE_MIN_LIGHT, true); }

void MapWidget::AntialiasingOn()
{
  auto engine = m_framework.GetDrapeEngine();
  if (engine != nullptr)
    engine->SetPosteffectEnabled(df::PostprocessRenderer::Antialiasing, true);
}

void MapWidget::AntialiasingOff()
{
  auto engine = m_framework.GetDrapeEngine();
  if (engine != nullptr)
    engine->SetPosteffectEnabled(df::PostprocessRenderer::Antialiasing, false);
}

void MapWidget::ScaleChanged(int action)
{
  if (!m_slider)
    return;

  if (action == QAbstractSlider::SliderNoAction)
    return;

  double const factor = m_slider->GetScaleFactor();
  if (factor != 1.0)
    m_framework.Scale(factor, false);
}

void MapWidget::SliderPressed() { m_sliderState = SliderState::Pressed; }

void MapWidget::SliderReleased() { m_sliderState = SliderState::Released; }

m2::PointD MapWidget::GetDevicePoint(QMouseEvent * e) const
{
  return m2::PointD(L2D(e->x()), L2D(e->y()));
}

df::Touch MapWidget::GetTouch(QMouseEvent * e) const
{
  df::Touch touch;
  touch.m_id = 0;
  touch.m_location = GetDevicePoint(e);
  return touch;
}

df::TouchEvent MapWidget::GetTouchEvent(QMouseEvent * e, df::TouchEvent::ETouchType type) const
{
  df::TouchEvent event;
  event.SetTouchType(type);
  event.SetFirstTouch(GetTouch(e));
  if (IsCommandModifier(e))
    event.SetSecondTouch(GetSymmetrical(event.GetFirstTouch()));

  return event;
}

df::Touch MapWidget::GetSymmetrical(df::Touch const & touch) const
{
  m2::PointD const pixelCenter = m_framework.GetPixelCenter();
  m2::PointD const symmetricalLocation = pixelCenter + pixelCenter - m2::PointD(touch.m_location);

  df::Touch result;
  result.m_id = touch.m_id + 1;
  result.m_location = symmetricalLocation;

  return result;
}

void MapWidget::OnViewportChanged(ScreenBase const & screen)
{
  UpdateScaleControl();
}

void MapWidget::UpdateScaleControl()
{
  if (!m_slider || m_sliderState == SliderState::Pressed)
    return;
  m_slider->SetPosWithBlockedSignals(m_framework.GetDrawScale());
}

void MapWidget::Build()
{
  std::string vertexSrc;
  std::string fragmentSrc;
  if (m_apiOpenGLES3)
  {
    vertexSrc =
        "\
      #version 150 core\n \
      in vec4 a_position; \
      uniform vec2 u_samplerSize; \
      out vec2 v_texCoord; \
      \
      void main() \
      { \
        v_texCoord = vec2(a_position.z * u_samplerSize.x, a_position.w * u_samplerSize.y); \
        gl_Position = vec4(a_position.x, a_position.y, 0.0, 1.0);\
      }";

    fragmentSrc =
        "\
      #version 150 core\n \
      uniform sampler2D u_sampler; \
      in vec2 v_texCoord; \
      out vec4 v_FragColor; \
      \
      void main() \
      { \
        v_FragColor = vec4(texture(u_sampler, v_texCoord).rgb, 1.0); \
      }";
  }
  else
  {
    vertexSrc =
        "\
      attribute vec4 a_position; \
      uniform vec2 u_samplerSize; \
      varying vec2 v_texCoord; \
      \
      void main() \
      { \
        v_texCoord = vec2(a_position.z * u_samplerSize.x, a_position.w * u_samplerSize.y); \
        gl_Position = vec4(a_position.x, a_position.y, 0.0, 1.0);\
      }";

    fragmentSrc =
        "\
      uniform sampler2D u_sampler; \
      varying vec2 v_texCoord; \
      \
      void main() \
      { \
        gl_FragColor = vec4(texture2D(u_sampler, v_texCoord).rgb, 1.0); \
      }";
  }

  m_program = std::make_unique<QOpenGLShaderProgram>(this);
  m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexSrc.c_str());
  m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentSrc.c_str());
  m_program->link();

  m_vao = std::make_unique<QOpenGLVertexArrayObject>(this);
  m_vao->create();
  m_vao->bind();

  m_vbo = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
  m_vbo->setUsagePattern(QOpenGLBuffer::StaticDraw);
  m_vbo->create();
  m_vbo->bind();

  QVector4D vertices[4] = {QVector4D(-1.0, 1.0, 0.0, 1.0),
                           QVector4D(1.0, 1.0, 1.0, 1.0),
                           QVector4D(-1.0, -1.0, 0.0, 0.0),
                           QVector4D(1.0, -1.0, 1.0, 0.0)};
  m_vbo->allocate(static_cast<void*>(vertices), sizeof(vertices));

  m_program->release();
  m_vao->release();
}

void MapWidget::ShowInfoPopup(QMouseEvent * e, m2::PointD const & pt)
{
  // show feature types
  QMenu menu;
  auto const addStringFn = [&menu](std::string const & s) {
    if (s.empty())
      return;

    menu.addAction(QString::fromUtf8(s.c_str()));
  };

  m_framework.ForEachFeatureAtPoint(
      [&](FeatureType & ft) {
        std::string concat;
        auto types = feature::TypesHolder(ft);
        types.SortBySpec();
        for (auto const & type : types.ToObjectNames())
          concat += type + " ";
        addStringFn(concat);

        std::string name;
        ft.GetReadableName(name);
        addStringFn(name);

        auto const info = GetFeatureAddressInfo(m_framework, ft);
        addStringFn(info.FormatAddress());

        menu.addSeparator();
      },
      m_framework.PtoG(pt));

  menu.exec(e->pos());
}

void MapWidget::initializeGL()
{
  ASSERT(m_contextFactory == nullptr, ());
  if (!m_screenshotMode)
    m_ratio = devicePixelRatio();
  m_contextFactory.reset(new QtOGLContextFactory(context()));

  emit BeforeEngineCreation();
  CreateEngine();
  m_framework.EnterForeground();
}

void MapWidget::paintGL()
{
  if (m_program == nullptr)
    Build();

  if (m_contextFactory->AcquireFrame())
  {
    m_vao->bind();
    m_program->bind();

    QOpenGLFunctions * funcs = context()->functions();
    funcs->glActiveTexture(GL_TEXTURE0);
    GLuint const image = m_contextFactory->GetTextureHandle();
    funcs->glBindTexture(GL_TEXTURE_2D, image);

    int const samplerLocation = m_program->uniformLocation("u_sampler");
    m_program->setUniformValue(samplerLocation, 0);

    QRectF const & texRect = m_contextFactory->GetTexRect();
    QVector2D const samplerSize(texRect.width(), texRect.height());

    int const samplerSizeLocation = m_program->uniformLocation("u_samplerSize");
    m_program->setUniformValue(samplerSizeLocation, samplerSize);

    m_program->enableAttributeArray("a_position");
    m_program->setAttributeBuffer("a_position", GL_FLOAT, 0, 4, 0);

    funcs->glClearColor(0.0, 0.0, 0.0, 1.0);
    funcs->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    funcs->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    m_program->release();
    m_vao->release();

  }
}

void MapWidget::resizeGL(int width, int height)
{
  float w = m_screenshotMode ? width : static_cast<float>(m_ratio * width);
  float h = m_screenshotMode ? height : static_cast<float>(m_ratio * height);
  m_framework.OnSize(w, h);
  m_framework.SetVisibleViewport(m2::RectD(0, 0, w, h));
  if (m_skin)
  {
    m_skin->Resize(w, h);

    gui::TWidgetsLayoutInfo layout;
    m_skin->ForEach(
        [&layout](gui::EWidget w, gui::Position const & pos) { layout[w] = pos.m_pixelPivot; });

    m_framework.SetWidgetLayout(std::move(layout));
  }
}

void MapWidget::mouseDoubleClickEvent(QMouseEvent * e)
{
  if (m_screenshotMode)
    return;

  QOpenGLWidget::mouseDoubleClickEvent(e);
  if (IsLeftButton(e))
    m_framework.Scale(Framework::SCALE_MAG_LIGHT, GetDevicePoint(e), true);
}

void MapWidget::mousePressEvent(QMouseEvent * e)
{
  if (m_screenshotMode)
    return;

  QOpenGLWidget::mousePressEvent(e);
  if (IsLeftButton(e))
    m_framework.TouchEvent(GetTouchEvent(e, df::TouchEvent::TOUCH_DOWN));
}

void MapWidget::mouseMoveEvent(QMouseEvent * e)
{
  if (m_screenshotMode)
    return;

  QOpenGLWidget::mouseMoveEvent(e);
  if (IsLeftButton(e))
    m_framework.TouchEvent(GetTouchEvent(e, df::TouchEvent::TOUCH_MOVE));
}

void MapWidget::mouseReleaseEvent(QMouseEvent * e)
{
  if (m_screenshotMode)
    return;

  if (e->button() == Qt::RightButton)
    emit OnContextMenuRequested(e->globalPos());

  QOpenGLWidget::mouseReleaseEvent(e);
  if (IsLeftButton(e))
    m_framework.TouchEvent(GetTouchEvent(e, df::TouchEvent::TOUCH_UP));
}

void MapWidget::wheelEvent(QWheelEvent * e)
{
  if (m_screenshotMode)
    return;

  QOpenGLWidget::wheelEvent(e);
  m_framework.Scale(exp(e->delta() / 360.0), m2::PointD(L2D(e->x()), L2D(e->y())), false);
}

search::ReverseGeocoder::Address GetFeatureAddressInfo(Framework const & framework,
                                                       FeatureType & ft)
{
  search::ReverseGeocoder const coder(framework.GetDataSource());
  search::ReverseGeocoder::Address address;
  coder.GetExactAddress(ft, address);

  return address;
}
}  // namespace common
}  // namespace qt
