#pragma once

#include "qt/qtoglcontextfactory.hpp"

#include "map/framework.hpp"

#include "drape_frontend/gui/skin.hpp"

#include "drape_frontend/drape_engine.hpp"

#include "std/unique_ptr.hpp"
#include "std/mutex.hpp"
#include "std/condition_variable.hpp"

#include <QtWidgets/QOpenGLWidget>

class QQuickWindow;

namespace qt
{
  class QScaleSlider;

  class DrawWidget : public QOpenGLWidget
  {
    using TBase = QOpenGLWidget;

    drape_ptr<QtOGLContextFactory> m_contextFactory;
    unique_ptr<Framework> m_framework;

    qreal m_ratio;

    Q_OBJECT

  public Q_SLOTS:
    void ScalePlus();
    void ScaleMinus();
    void ScalePlusLight();
    void ScaleMinusLight();

    void ShowAll();
    void ScaleChanged(int action);
    void SliderPressed();
    void SliderReleased();

  public:
    DrawWidget(QWidget * parent);
    ~DrawWidget();

    void SetScaleControl(QScaleSlider * pScale);

    bool Search(search::SearchParams params);
    string GetDistance(search::Result const & res) const;
    void ShowSearchResult(search::Result const & res);

    void OnLocationUpdate(location::GpsInfo const & info);

    void SaveState();
    void LoadState();

    void UpdateAfterSettingsChanged();

    void PrepareShutdown();

    Framework & GetFramework() { return *m_framework.get(); }

    void SetMapStyle(MapStyle mapStyle);

    void SetRouter(routing::RouterType routerType);
    Q_SIGNAL void BeforeEngineCreation();

    void CreateEngine();

  protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int width, int height) override;
    /// @name Overriden from QOpenGLWindow.
    //@{
    void mousePressEvent(QMouseEvent * e) override;
    void mouseDoubleClickEvent(QMouseEvent * e) override;
    void mouseMoveEvent(QMouseEvent * e) override;
    void mouseReleaseEvent(QMouseEvent * e) override;
    void wheelEvent(QWheelEvent * e) override;
    void keyPressEvent(QKeyEvent * e) override;
    void keyReleaseEvent(QKeyEvent * e) override;
    //@}

  private:
    void SubmitFakeLocationPoint(m2::PointD const & pt);
    void SubmitRoutingPoint(m2::PointD const & pt);
    void ShowInfoPopup(QMouseEvent * e, m2::PointD const & pt);
    void ShowPOIEditor(m2::PointD const & pt);

    void OnViewportChanged(ScreenBase const & screen);
    void UpdateScaleControl();
    df::Touch GetTouch(QMouseEvent * e);
    df::Touch GetSymmetrical(const df::Touch & touch);
    df::TouchEvent GetTouchEvent(QMouseEvent * e, df::TouchEvent::ETouchType type);

    inline int L2D(int px) const { return px * m_ratio; }
    m2::PointD GetDevicePoint(QMouseEvent * e) const;

    QScaleSlider * m_pScale;
    bool m_enableScaleUpdate;

    bool m_emulatingLocation;

    void InitRenderPolicy();

    unique_ptr<gui::Skin> m_skin;
  };
}
