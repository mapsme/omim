#include "qt/mainwindow.hpp"

#ifndef USE_DRAPE
#include "qt/draw_widget.hpp"
#else
#include "qt/drape_surface.hpp"
#endif

#include "qt/slider_ctrl.hpp"
#include "qt/about.hpp"
#include "qt/preferences_dialog.hpp"
#include "qt/search_panel.hpp"

#include "defines.hpp"

#include "platform/settings.hpp"
#include "platform/platform.hpp"

#include "std/bind.hpp"

#include "build_style/build_style.h"

#include <QtGui/QCloseEvent>

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
  #include <QtGui/QAction>
  #include <QtGui/QDockWidget>
  #include <QtGui/QMenu>
  #include <QtGui/QMenuBar>
  #include <QtGui/QToolBar>
#else
  #include <QtWidgets/QAction>
  #include <QtWidgets/QDockWidget>
  #include <QtWidgets/QMenu>
  #include <QtWidgets/QMenuBar>
  #include <QtWidgets/QToolBar>
#endif

#include <QMessageBox>

#define IDM_ABOUT_DIALOG        1001
#define IDM_PREFERENCES_DIALOG  1002

#ifndef NO_DOWNLOADER
#include "qt/update_dialog.hpp"
#include "qt/info_dialog.hpp"

#include "indexer/classificator.hpp"

#include <QtCore/QFile>

#endif // NO_DOWNLOADER


namespace qt
{

MainWindow::MainWindow(QString const & mapcssFilePath /*= QString()*/)
  : m_pBuildStyleAction(nullptr)
  , m_locationService(CreateDesktopLocationService(*this))
  , m_mapcssFilePath(mapcssFilePath)
{
#ifndef USE_DRAPE
  m_pDrawWidget = new DrawWidget(this);
  setCentralWidget(m_pDrawWidget);
#else
  m_pDrawWidget = new DrapeSurface();
  QSurfaceFormat format = m_pDrawWidget->requestedFormat();
  format.setDepthBufferSize(16);
  m_pDrawWidget->setFormat(format);
  QWidget * w = QWidget::createWindowContainer(m_pDrawWidget, this);
  w->setMouseTracking(true);
  setCentralWidget(w);
#endif // USE_DRAPE

  shared_ptr<location::State> locState = m_pDrawWidget->GetFramework().GetLocationState();
  locState->AddStateModeListener([this] (location::State::Mode mode)
                                 {
                                    LocationStateModeChanged(mode);
                                 });

  CreateNavigationBar();
  CreateSearchBarAndPanel();

  QString caption = qAppName();
  if (!m_mapcssFilePath.isEmpty())
    caption += QString(" - ") + m_mapcssFilePath;

  setWindowTitle(caption);
  setWindowIcon(QIcon(":/ui/logo.png"));

#ifndef OMIM_OS_WINDOWS
  QMenu * helpMenu = new QMenu(tr("Help"), this);
  menuBar()->addMenu(helpMenu);
  helpMenu->addAction(tr("About"), this, SLOT(OnAbout()));
  helpMenu->addAction(tr("Preferences"), this, SLOT(OnPreferences()));
#else
  {
    // create items in the system menu
    HMENU menu = ::GetSystemMenu((HWND)winId(), FALSE);
    MENUITEMINFOA item;
    item.cbSize = sizeof(MENUITEMINFOA);
    item.fMask = MIIM_FTYPE | MIIM_ID | MIIM_STRING;
    item.fType = MFT_STRING;
    item.wID = IDM_PREFERENCES_DIALOG;
    QByteArray const prefsStr = tr("Preferences...").toLocal8Bit();
    item.dwTypeData = const_cast<char *>(prefsStr.data());
    item.cch = prefsStr.size();
    ::InsertMenuItemA(menu, ::GetMenuItemCount(menu) - 1, TRUE, &item);
    item.wID = IDM_ABOUT_DIALOG;
    QByteArray const aboutStr = tr("About...").toLocal8Bit();
    item.dwTypeData = const_cast<char *>(aboutStr.data());
    item.cch = aboutStr.size();
    ::InsertMenuItemA(menu, ::GetMenuItemCount(menu) - 1, TRUE, &item);
    item.fType = MFT_SEPARATOR;
    ::InsertMenuItemA(menu, ::GetMenuItemCount(menu) - 1, TRUE, &item);
  }
#endif

  LoadState();

#ifndef NO_DOWNLOADER
  // Show intro dialog if necessary
  bool bShow = true;
  (void)Settings::Get("ShowWelcome", bShow);

  if (bShow)
  {
    bool bShowUpdateDialog = true;

    string text;
    try
    {
      ReaderPtr<Reader> reader = GetPlatform().GetReader("welcome.html");
      reader.ReadAsString(text);
    }
    catch (...)
    {}

    if (!text.empty())
    {
      InfoDialog welcomeDlg(QString("Welcome to ") + qAppName(), text.c_str(),
                            this, QStringList(tr("Download Maps")));
      if (welcomeDlg.exec() == QDialog::Rejected)
        bShowUpdateDialog = false;
    }
    Settings::Set("ShowWelcome", false);

    if (bShowUpdateDialog)
      ShowUpdateDialog();
  }
#endif // NO_DOWNLOADER

#ifndef USE_DRAPE
  m_pDrawWidget->UpdateAfterSettingsChanged();
#endif // USE_DRAPE
  locState->InvalidatePosition();
}

#if defined(Q_WS_WIN)
bool MainWindow::winEvent(MSG * msg, long * result)
{
  if (msg->message == WM_SYSCOMMAND)
  {
    switch (msg->wParam)
    {
    case IDM_PREFERENCES_DIALOG:
      OnPreferences();
      *result = 0;
      return true;
    case IDM_ABOUT_DIALOG:
      OnAbout();
      *result = 0;
      return true;
    }
  }
  return false;
}
#endif

MainWindow::~MainWindow()
{
  SaveState();
}

void MainWindow::SaveState()
{
  pair<int, int> xAndY(x(), y());
  pair<int, int> widthAndHeight(width(), height());
  Settings::Set("MainWindowXY", xAndY);
  Settings::Set("MainWindowSize", widthAndHeight);

  m_pDrawWidget->SaveState();
}

void MainWindow::LoadState()
{
  // do always show on full screen
  showMaximized();
  m_pDrawWidget->LoadState();
}

void MainWindow::LocationStateModeChanged(location::State::Mode mode)
{
  if (mode == location::State::PendingPosition)
  {
    m_locationService->Start();
    m_pMyPositionAction->setIcon(QIcon(":/navig64/location-search.png"));
    m_pMyPositionAction->setToolTip(tr("Looking for position..."));
    return;
  }

  if (mode == location::State::UnknownPosition)
    m_locationService->Stop();

  m_pMyPositionAction->setIcon(QIcon(":/navig64/location.png"));
  m_pMyPositionAction->setToolTip(tr("My Position"));
}

namespace
{
  struct button_t
  {
    QString name;
    char const * icon;
    char const * slot;
  };

  void add_buttons(QToolBar * pBar, button_t buttons[], size_t count, QObject * pReceiver)
  {
    for (size_t i = 0; i < count; ++i)
    {
      if (buttons[i].icon)
        pBar->addAction(QIcon(buttons[i].icon), buttons[i].name, pReceiver, buttons[i].slot);
      else
        pBar->addSeparator();
    }
  }

  struct hotkey_t
  {
    int key;
    char const * slot;
  };
}

void MainWindow::CreateNavigationBar()
{
  QToolBar * pToolBar = new QToolBar(tr("Navigation Bar"), this);
  pToolBar->setOrientation(Qt::Vertical);
  pToolBar->setIconSize(QSize(32, 32));

#ifndef USE_DRAPE
  {
    // add navigation hot keys
    hotkey_t arr[] = {
      { Qt::Key_Left, SLOT(MoveLeft()) },
      { Qt::Key_Right, SLOT(MoveRight()) },
      { Qt::Key_Up, SLOT(MoveUp()) },
      { Qt::Key_Down, SLOT(MoveDown()) },
      { Qt::Key_Equal, SLOT(ScalePlus()) },
      { Qt::Key_Minus, SLOT(ScaleMinus()) },
      { Qt::ALT + Qt::Key_Equal, SLOT(ScalePlusLight()) },
      { Qt::ALT + Qt::Key_Minus, SLOT(ScaleMinusLight()) },
      { Qt::Key_A, SLOT(ShowAll()) },
      { Qt::Key_S, SLOT(QueryMaxScaleMode()) }
    };

    for (size_t i = 0; i < ARRAY_SIZE(arr); ++i)
    {
      QAction * pAct = new QAction(this);
      pAct->setShortcut(QKeySequence(arr[i].key));
      connect(pAct, SIGNAL(triggered()), m_pDrawWidget, arr[i].slot);
      addAction(pAct);
    }
  }
#endif // USE_DRAPE

  {
    // add search button with "checked" behavior
    m_pSearchAction = pToolBar->addAction(QIcon(":/navig64/search.png"),
                                           tr("Search"),
                                           this,
                                           SLOT(OnSearchButtonClicked()));
    m_pSearchAction->setCheckable(true);
    m_pSearchAction->setToolTip(tr("Offline Search"));
    m_pSearchAction->setShortcut(QKeySequence::Find);

    pToolBar->addSeparator();

// #ifndef OMIM_OS_LINUX
    // add my position button with "checked" behavior

    m_pMyPositionAction = pToolBar->addAction(QIcon(":/navig64/location.png"),
                                           tr("My Position"),
                                           this,
                                           SLOT(OnMyPosition()));
    m_pMyPositionAction->setCheckable(true);
    m_pMyPositionAction->setToolTip(tr("My Position"));
// #endif

#ifdef BUILD_DESIGNER
    if (!m_mapcssFilePath.isEmpty())
    {
      m_pBuildStyleAction = pToolBar->addAction(QIcon(":/navig64/run.png"),
                                                tr("Build style"),
                                                this,
                                                SLOT(OnBuildStyle()));
      m_pBuildStyleAction->setCheckable(false);
      m_pBuildStyleAction->setToolTip(tr("Run script"));
    }
#endif // BUILD_DESIGNER

#ifndef USE_DRAPE
    // add view actions 1
    button_t arr[] = {
      { QString(), 0, 0 },
      //{ tr("Show all"), ":/navig64/world.png", SLOT(ShowAll()) },
      { tr("Scale +"), ":/navig64/plus.png", SLOT(ScalePlus()) }
    };
    add_buttons(pToolBar, arr, ARRAY_SIZE(arr), m_pDrawWidget);
#endif // USE_DRAPE
  }

#ifndef USE_DRAPE
  // add scale slider
  QScaleSlider * pScale = new QScaleSlider(Qt::Vertical, this, 20);
  pScale->SetRange(2, scales::GetUpperScale());
  pScale->setTickPosition(QSlider::TicksRight);

  pToolBar->addWidget(pScale);
  m_pDrawWidget->SetScaleControl(pScale);

  {
    // add view actions 2
    button_t arr[] = {
      { tr("Scale -"), ":/navig64/minus.png", SLOT(ScaleMinus()) }
    };
    add_buttons(pToolBar, arr, ARRAY_SIZE(arr), m_pDrawWidget);
  }
#endif // USE_DRAPE

#ifndef NO_DOWNLOADER
  {
    // add mainframe actions
    button_t arr[] = {
      { QString(), 0, 0 },
      { tr("Download Maps"), ":/navig64/download.png", SLOT(ShowUpdateDialog()) }
    };
    add_buttons(pToolBar, arr, ARRAY_SIZE(arr), this);
  }
#endif // NO_DOWNLOADER

  addToolBar(Qt::RightToolBarArea, pToolBar);
}

void MainWindow::OnAbout()
{
  AboutDialog dlg(this);
  dlg.exec();
}

void MainWindow::OnLocationError(location::TLocationError errorCode)
{
  switch (errorCode)
  {
  case location::EDenied:
    m_pMyPositionAction->setEnabled(false);
    break;

  default:
    ASSERT(false, ("Not handled location notification:", errorCode));
    break;
  }

  m_pDrawWidget->GetFramework().OnLocationError(errorCode);
}

void MainWindow::OnLocationUpdated(location::GpsInfo const & info)
{
  m_pDrawWidget->GetFramework().OnLocationUpdate(info);
}

void MainWindow::OnMyPosition()
{
  if (m_pMyPositionAction->isEnabled())
    m_pDrawWidget->GetFramework().GetLocationState()->SwitchToNextMode();
}

void MainWindow::OnSearchButtonClicked()
{
  if (m_pSearchAction->isChecked())
  {
    m_pDrawWidget->GetFramework().PrepareSearch();

    m_Docks[0]->show();
  }
  else
  {
    m_Docks[0]->hide();
  }
}

void MainWindow::OnPreferences()
{
  PreferencesDialog dlg(this);
  dlg.exec();

  m_pDrawWidget->GetFramework().SetupMeasurementSystem();
}

#ifdef BUILD_DESIGNER
void MainWindow::OnBuildStyle()
{
  try
  {
    build_style::BuildAndApply(m_mapcssFilePath);

    m_pDrawWidget->RefreshDrawingRules();
  }
  catch (exception & e)
  {
    QMessageBox msgBox;
    msgBox.setWindowTitle("Error");
    msgBox.setText(e.what());
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.setDefaultButton(QMessageBox::Ok);
    msgBox.exec();
  }
}
#endif // BUILD_DESIGNER

#ifndef NO_DOWNLOADER
void MainWindow::ShowUpdateDialog()
{
  UpdateDialog dlg(this, m_pDrawWidget->GetFramework());
  dlg.ShowModal();
}

#endif // NO_DOWNLOADER

void MainWindow::CreateSearchBarAndPanel()
{
#ifndef USE_DRAPE
  CreatePanelImpl(0, Qt::RightDockWidgetArea, tr("Search"), QKeySequence(), 0);

  SearchPanel * panel = new SearchPanel(m_pDrawWidget, m_Docks[0]);
  m_Docks[0]->setWidget(panel);
#endif // USE_DRAPE
}

void MainWindow::CreatePanelImpl(size_t i, Qt::DockWidgetArea area, QString const & name,
                                 QKeySequence const & hotkey, char const * slot)
{
  ASSERT_LESS(i, ARRAY_SIZE(m_Docks), ());
  m_Docks[i] = new QDockWidget(name, this);

  addDockWidget(area, m_Docks[i]);

  // hide by default
  m_Docks[i]->hide();

  // register a hotkey to show panel
  if (slot && !hotkey.isEmpty())
  {
    QAction * pAct = new QAction(this);
    pAct->setShortcut(hotkey);
    connect(pAct, SIGNAL(triggered()), this, slot);
    addAction(pAct);
  }
}

void MainWindow::closeEvent(QCloseEvent * e)
{
#ifndef USE_DRAPE
  m_pDrawWidget->PrepareShutdown();
#else
  m_pDrawWidget->GetFramework().PrepareToShutdown();
#endif // USE_DRAPE
  e->accept();
}

}
