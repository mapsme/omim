#include "drape_head/mainwindow.hpp"

#include "drape_head/drape_surface.hpp"

#include <QtWidgets/QWidget>

MainWindow::MainWindow(QWidget *parent)
  : QMainWindow(parent)
  , m_surface(nullptr)
{
  resize(1200, 800);

  DrapeSurface * surface = new DrapeSurface();
  QSurfaceFormat format = surface->requestedFormat();

  format.setMajorVersion(3);
  format.setMinorVersion(2);

  format.setAlphaBufferSize(0);
  format.setBlueBufferSize(8);
  format.setGreenBufferSize(8);
  format.setRedBufferSize(8);
  format.setStencilBufferSize(0);
  format.setSamples(0);
  format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
  format.setSwapInterval(1);
  format.setDepthBufferSize(16);

  format.setProfile(QSurfaceFormat::CoreProfile);

  surface->setFormat(format);
  m_surface = QWidget::createWindowContainer(surface, this);
  m_surface->setMouseTracking(true);
  setCentralWidget(m_surface);
}

MainWindow::~MainWindow()
{
  ASSERT(m_surface == NULL, ());
}

void MainWindow::closeEvent(QCloseEvent * closeEvent)
{
  delete m_surface;
  m_surface = NULL;
}
