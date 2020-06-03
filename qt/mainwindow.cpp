#include "qt/about.hpp"
#include "qt/bookmark_dialog.hpp"
#include "qt/draw_widget.hpp"
#include "qt/mainwindow.hpp"
#include "qt/mwms_borders_selection.hpp"
#include "qt/osm_auth_dialog.hpp"
#include "qt/preferences_dialog.hpp"
#include "qt/qt_common/helpers.hpp"
#include "qt/qt_common/scale_slider.hpp"
#include "qt/routing_settings_dialog.hpp"
#include "qt/screenshoter.hpp"
#include "qt/search_panel.hpp"

#include "platform/settings.hpp"
#include "platform/platform.hpp"

#include "base/assert.hpp"

#include "defines.hpp"

#include <sstream>

#include "std/target_os.hpp"

#ifdef BUILD_DESIGNER
#include "build_style/build_common.h"
#include "build_style/build_phone_pack.h"
#include "build_style/build_style.h"
#include "build_style/build_statistics.h"
#include "build_style/run_tests.h"

#include "drape_frontend/debug_rect_renderer.hpp"
#endif // BUILD_DESIGNER

#include <QtGui/QCloseEvent>
#include <QtWidgets/QAction>
#include <QtWidgets/QDesktopWidget>
#include <QtWidgets/QDockWidget>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QToolButton>

#define IDM_ABOUT_DIALOG        1001
#define IDM_PREFERENCES_DIALOG  1002

#ifndef NO_DOWNLOADER
#include "qt/update_dialog.hpp"
#include "qt/info_dialog.hpp"

#include "indexer/classificator.hpp"

#include <QtCore/QFile>

#endif // NO_DOWNLOADER

namespace
{
using namespace qt;

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

void FormatMapSize(uint64_t sizeInBytes, std::string & units, size_t & sizeToDownload)
{
  int const mbInBytes = 1024 * 1024;
  int const kbInBytes = 1024;
  if (sizeInBytes > mbInBytes)
  {
    sizeToDownload = (sizeInBytes + mbInBytes - 1) / mbInBytes;
    units = "MB";
  }
  else if (sizeInBytes > kbInBytes)
  {
    sizeToDownload = (sizeInBytes + kbInBytes -1) / kbInBytes;
    units = "KB";
  }
  else
  {
    sizeToDownload = sizeInBytes;
    units = "B";
  }
}

DrawWidget::SelectionMode ConvertFromMwmsBordersSelection(qt::MwmsBordersSelection::Response mode)
{
  switch (mode)
  {
  case MwmsBordersSelection::Response::MwmsBordersByPolyFiles:
  {
    return DrawWidget::SelectionMode::MwmsBordersByPolyFiles;
  }
  case MwmsBordersSelection::Response::MwmsBordersWithVerticesByPolyFiles:
  {
    return DrawWidget::SelectionMode::MwmsBordersWithVerticesByPolyFiles;
  }
  case MwmsBordersSelection::Response::MwmsBordersByPackedPolygon:
  {
    return DrawWidget::SelectionMode::MwmsBordersByPackedPolygon;
  }
  case MwmsBordersSelection::Response::MwmsBordersWithVerticesByPackedPolygon:
  {
    return DrawWidget::SelectionMode::MwmsBordersWithVerticesByPackedPolygon;
  }
  case MwmsBordersSelection::Response::BoundingBoxByPolyFiles:
  {
    return DrawWidget::SelectionMode::BoundingBoxByPolyFiles;
  }
  case MwmsBordersSelection::Response::BoundingBoxByPackedPolygon:
  {
    return DrawWidget::SelectionMode::BoundingBoxByPackedPolygon;
  }
  default:
    UNREACHABLE();
  }
}

bool IsMwmsBordersSelectionMode(DrawWidget::SelectionMode mode)
{
  return mode == DrawWidget::SelectionMode::MwmsBordersByPolyFiles ||
         mode == DrawWidget::SelectionMode::MwmsBordersWithVerticesByPolyFiles ||
         mode == DrawWidget::SelectionMode::MwmsBordersByPackedPolygon ||
         mode == DrawWidget::SelectionMode::MwmsBordersWithVerticesByPackedPolygon;
}
}  // namespace

namespace qt
{
// Defined in osm_auth_dialog.cpp.
extern char const * kTokenKeySetting;
extern char const * kTokenSecretSetting;

MainWindow::MainWindow(Framework & framework, bool apiOpenGLES3,
                       std::unique_ptr<ScreenshotParams> && screenshotParams,
                       QString const & mapcssFilePath /*= QString()*/)
  : m_Docks{}
  , m_locationService(CreateDesktopLocationService(*this))
  , m_screenshotMode(screenshotParams != nullptr)
#ifdef BUILD_DESIGNER
  , m_mapcssFilePath(mapcssFilePath)
#endif
{
  // Always runs on the first desktop
  QDesktopWidget const * desktop(QApplication::desktop());
  setGeometry(desktop->screenGeometry(desktop->primaryScreen()));

  if (m_screenshotMode)
  {
    screenshotParams->m_statusChangedFn = [this](std::string const & state, bool finished)
    {
      statusBar()->showMessage(QString::fromStdString(state));
      if (finished)
        QCoreApplication::quit();
    };
  }

  int const width = m_screenshotMode ? static_cast<int>(screenshotParams->m_width) : 0;
  int const height = m_screenshotMode ? static_cast<int>(screenshotParams->m_height) : 0;
  m_pDrawWidget = new DrawWidget(framework, apiOpenGLES3, std::move(screenshotParams), this);

  setCentralWidget(m_pDrawWidget);

  if (m_screenshotMode)
  {
    m_pDrawWidget->setFixedSize(width, height);
    setFixedSize(width, height + statusBar()->height());
  }

  QObject::connect(m_pDrawWidget, SIGNAL(BeforeEngineCreation()), this, SLOT(OnBeforeEngineCreation()));

  CreateCountryStatusControls();
  CreateNavigationBar();
  CreateSearchBarAndPanel();

  QString caption = qAppName();
#ifdef BUILD_DESIGNER
  if (!m_mapcssFilePath.isEmpty())
    caption += QString(" - ") + m_mapcssFilePath;
#endif

  setWindowTitle(caption);
  setWindowIcon(QIcon(":/ui/logo.png"));

#ifndef OMIM_OS_WINDOWS
  QMenu * helpMenu = new QMenu(tr("Help"), this);
  menuBar()->addMenu(helpMenu);
  helpMenu->addAction(tr("About"), this, SLOT(OnAbout()));
  helpMenu->addAction(tr("Preferences"), this, SLOT(OnPreferences()));
  helpMenu->addAction(tr("OpenStreetMap Login"), this, SLOT(OnLoginMenuItem()));
  helpMenu->addAction(tr("Upload Edits"), this, SLOT(OnUploadEditsMenuItem()));
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

  // Always show on full screen.
  showMaximized();

#ifndef NO_DOWNLOADER
  // Show intro dialog if necessary
  bool bShow = true;
  std::string const showWelcome = "ShowWelcome";
  settings::TryGet(showWelcome, bShow);

  if (bShow)
  {
    bool bShowUpdateDialog = true;

    std::string text;
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
    settings::Set("ShowWelcome", false);

    if (bShowUpdateDialog)
      ShowUpdateDialog();
  }
#endif // NO_DOWNLOADER

  m_pDrawWidget->UpdateAfterSettingsChanged();

  m_pDrawWidget->GetFramework().UploadUGC(nullptr /* onCompleteUploading */);
  
  if (RoutingSettings::IsCacheEnabled())
    RoutingSettings::LoadSettings(m_pDrawWidget->GetFramework());
  else
    RoutingSettings::ResetSettings();
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

void MainWindow::LocationStateModeChanged(location::EMyPositionMode mode)
{
  if (mode == location::PendingPosition)
  {
    m_locationService->Start();
    m_pMyPositionAction->setIcon(QIcon(":/navig64/location-search.png"));
    m_pMyPositionAction->setToolTip(tr("Looking for position..."));
    return;
  }

  m_pMyPositionAction->setIcon(QIcon(":/navig64/location.png"));
  m_pMyPositionAction->setToolTip(tr("My Position"));
}

void MainWindow::CreateNavigationBar()
{
  QToolBar * pToolBar = new QToolBar(tr("Navigation Bar"), this);
  pToolBar->setOrientation(Qt::Vertical);
  pToolBar->setIconSize(QSize(32, 32));
  {
    m_pDrawWidget->BindHotkeys(*this);

    // Add navigation hot keys.
    qt::common::Hotkey const hotkeys[] = {
      { Qt::Key_A, SLOT(ShowAll()) },
      // Use CMD+n (New Item hotkey) to activate Create Feature mode.
      { Qt::Key_Escape, SLOT(ChoosePositionModeDisable()) }
    };

    for (auto const & hotkey : hotkeys)
    {
      QAction * pAct = new QAction(this);
      pAct->setShortcut(QKeySequence(hotkey.m_key));
      connect(pAct, SIGNAL(triggered()), m_pDrawWidget, hotkey.m_slot);
      addAction(pAct);
    }
  }

  {
    m_selectLayerTrafficAction = new QAction(QIcon(":/navig64/traffic.png"), tr("Traffic"), this);
    m_selectLayerTrafficAction->setCheckable(true);
    m_selectLayerTrafficAction->setChecked(m_pDrawWidget->GetFramework().LoadTrafficEnabled());
    connect(m_selectLayerTrafficAction, SIGNAL(triggered()), this, SLOT(OnTrafficEnabled()));

    m_selectLayerTransitAction =
        new QAction(QIcon(":/navig64/subway.png"), tr("Public transport"), this);
    m_selectLayerTransitAction->setCheckable(true);
    m_selectLayerTransitAction->setChecked(
        m_pDrawWidget->GetFramework().LoadTransitSchemeEnabled());
    connect(m_selectLayerTransitAction, SIGNAL(triggered()), this, SLOT(OnTransitEnabled()));

    m_selectLayerIsolinesAction =
        new QAction(QIcon(":/navig64/isolines.png"), tr("Isolines"), this);
    m_selectLayerIsolinesAction->setCheckable(true);
    m_selectLayerIsolinesAction->setChecked(m_pDrawWidget->GetFramework().LoadIsolinesEnabled());
    connect(m_selectLayerIsolinesAction, SIGNAL(triggered()), this, SLOT(OnIsolinesEnabled()));

    m_selectLayerGuidesAction =
        new QAction(QIcon(":/navig64/guides.png"), tr("Guides"), this);
    m_selectLayerGuidesAction->setCheckable(true);
    m_selectLayerGuidesAction->setChecked(m_pDrawWidget->GetFramework().LoadGuidesEnabled());
    connect(m_selectLayerGuidesAction, SIGNAL(triggered()), this, SLOT(OnGuidesEnabled()));

    auto layersMenu = new QMenu();
    layersMenu->addAction(m_selectLayerTrafficAction);
    layersMenu->addAction(m_selectLayerTransitAction);
    layersMenu->addAction(m_selectLayerIsolinesAction);
    layersMenu->addAction(m_selectLayerGuidesAction);

    m_selectLayerButton = new QToolButton();
    m_selectLayerButton->setPopupMode(QToolButton::MenuButtonPopup);
    m_selectLayerButton->setMenu(layersMenu);
    m_selectLayerButton->setIcon(QIcon(":/navig64/layers.png"));
    pToolBar->addWidget(m_selectLayerButton);
    pToolBar->addSeparator();

    m_bookmarksAction = pToolBar->addAction(QIcon(":/navig64/bookmark.png"), tr("Show bookmarks and tracks"),
                                            this, SLOT(OnBookmarksAction()));
    pToolBar->addSeparator();

#ifndef BUILD_DESIGNER
    m_selectStartRoutePoint = new QAction(QIcon(":/navig64/point-start.png"),
                                          tr("Start point"), this);
    connect(m_selectStartRoutePoint, SIGNAL(triggered()), this, SLOT(OnStartPointSelected()));

    m_selectFinishRoutePoint = new QAction(QIcon(":/navig64/point-finish.png"),
                                           tr("Finish point"), this);
    connect(m_selectFinishRoutePoint, SIGNAL(triggered()), this, SLOT(OnFinishPointSelected()));

    m_selectIntermediateRoutePoint = new QAction(QIcon(":/navig64/point-intermediate.png"),
                                                 tr("Intermediate point"), this);
    connect(m_selectIntermediateRoutePoint, SIGNAL(triggered()), this, SLOT(OnIntermediatePointSelected()));

    auto routePointsMenu = new QMenu();
    routePointsMenu->addAction(m_selectStartRoutePoint);
    routePointsMenu->addAction(m_selectFinishRoutePoint);
    routePointsMenu->addAction(m_selectIntermediateRoutePoint);
    m_routePointsToolButton = new QToolButton();
    m_routePointsToolButton->setPopupMode(QToolButton::MenuButtonPopup);
    m_routePointsToolButton->setMenu(routePointsMenu);
    switch (m_pDrawWidget->GetRoutePointAddMode())
    {
    case RouteMarkType::Start:
      m_routePointsToolButton->setIcon(m_selectStartRoutePoint->icon());
      break;
    case RouteMarkType::Finish:
      m_routePointsToolButton->setIcon(m_selectFinishRoutePoint->icon());
      break;
    case RouteMarkType::Intermediate:
      m_routePointsToolButton->setIcon(m_selectIntermediateRoutePoint->icon());
      break;
    }
    pToolBar->addWidget(m_routePointsToolButton);
    auto routingAction = pToolBar->addAction(QIcon(":/navig64/routing.png"), tr("Follow route"),
                                             this, SLOT(OnFollowRoute()));
    routingAction->setToolTip(tr("Follow route"));
    auto clearAction = pToolBar->addAction(QIcon(":/navig64/clear-route.png"), tr("Clear route"),
                                           this, SLOT(OnClearRoute()));
    clearAction->setToolTip(tr("Clear route"));

    auto settingsRoutingAction = pToolBar->addAction(QIcon(":/navig64/settings-routing.png"),
                                                     tr("Routing settings"), this,
                                                     SLOT(OnRoutingSettings()));
    settingsRoutingAction->setToolTip(tr("Routing settings"));

    pToolBar->addSeparator();

    m_pCreateFeatureAction = pToolBar->addAction(QIcon(":/navig64/select.png"), tr("Create Feature"),
                                                 this, SLOT(OnCreateFeatureClicked()));
    m_pCreateFeatureAction->setCheckable(true);
    m_pCreateFeatureAction->setToolTip(tr("Please select position on a map."));
    m_pCreateFeatureAction->setShortcut(QKeySequence::New);

    pToolBar->addSeparator();

    m_selectionMode = pToolBar->addAction(QIcon(":/navig64/selectmode.png"), tr("Selection mode"),
                                          this, SLOT(OnSwitchSelectionMode()));
    m_selectionMode->setCheckable(true);
    m_selectionMode->setToolTip(tr("Turn on/off selection mode"));

    m_selectionCityBoundariesMode =
        pToolBar->addAction(QIcon(":/navig64/city_boundaries.png"), tr("City boundaries selection mode"),
                            this, SLOT(OnSwitchCityBoundariesSelectionMode()));
    m_selectionCityBoundariesMode->setCheckable(true);

    m_selectionCityRoadsMode =
        pToolBar->addAction(QIcon(":/navig64/city_roads.png"), tr("City roads selection mode"),
                            this, SLOT(OnSwitchCityRoadsSelectionMode()));
    m_selectionCityRoadsMode->setCheckable(true);

    m_selectionMwmsBordersMode =
      pToolBar->addAction(QIcon(":/navig64/borders_selection.png"), tr("MWMs borders selection mode"),
                          this, SLOT(OnSwitchMwmsBordersSelectionMode()));
    m_selectionMwmsBordersMode->setCheckable(true);

    pToolBar->addSeparator();

#endif // NOT BUILD_DESIGNER

    // Add search button with "checked" behavior.
    m_pSearchAction = pToolBar->addAction(QIcon(":/navig64/search.png"), tr("Search"),
                                          this, SLOT(OnSearchButtonClicked()));
    m_pSearchAction->setCheckable(true);
    m_pSearchAction->setToolTip(tr("Offline Search"));
    m_pSearchAction->setShortcut(QKeySequence::Find);

    m_rulerAction = pToolBar->addAction(QIcon(":/navig64/ruler.png"), tr("Ruler"),
                                        this, SLOT(OnRulerEnabled()));
    m_rulerAction->setCheckable(true);
    m_rulerAction->setChecked(false);

    pToolBar->addSeparator();

    m_clearSelection = pToolBar->addAction(QIcon(":/navig64/clear.png"), tr("Clear selection"),
                                           this, SLOT(OnClearSelection()));
    m_clearSelection->setToolTip(tr("Clear selection"));

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
    // Add "Build style" button
    if (!m_mapcssFilePath.isEmpty())
    {
      m_pBuildStyleAction = pToolBar->addAction(QIcon(":/navig64/run.png"),
                                                tr("Build style"),
                                                this,
                                                SLOT(OnBuildStyle()));
      m_pBuildStyleAction->setCheckable(false);
      m_pBuildStyleAction->setToolTip(tr("Build style"));

      m_pRecalculateGeomIndex = pToolBar->addAction(QIcon(":/navig64/geom.png"),
                                                    tr("Recalculate geometry index"),
                                                    this,
                                                    SLOT(OnRecalculateGeomIndex()));
      m_pRecalculateGeomIndex->setCheckable(false);
      m_pRecalculateGeomIndex->setToolTip(tr("Recalculate geometry index"));
    }

    // Add "Debug style" button
    m_pDrawDebugRectAction = pToolBar->addAction(QIcon(":/navig64/bug.png"),
                                              tr("Debug style"),
                                              this,
                                              SLOT(OnDebugStyle()));
    m_pDrawDebugRectAction->setCheckable(true);
    m_pDrawDebugRectAction->setChecked(false);
    m_pDrawDebugRectAction->setToolTip(tr("Debug style"));
    m_pDrawWidget->GetFramework().EnableDebugRectRendering(false);

    // Add "Get statistics" button
    m_pGetStatisticsAction = pToolBar->addAction(QIcon(":/navig64/chart.png"),
                                                 tr("Get statistics"),
                                                 this,
                                                 SLOT(OnGetStatistics()));
    m_pGetStatisticsAction->setCheckable(false);
    m_pGetStatisticsAction->setToolTip(tr("Get statistics"));

    // Add "Run tests" button
    m_pRunTestsAction = pToolBar->addAction(QIcon(":/navig64/test.png"),
                                            tr("Run tests"),
                                            this,
                                            SLOT(OnRunTests()));
    m_pRunTestsAction->setCheckable(false);
    m_pRunTestsAction->setToolTip(tr("Run tests"));

    // Add "Build phone package" button
    m_pBuildPhonePackAction = pToolBar->addAction(QIcon(":/navig64/phonepack.png"),
                                                  tr("Build phone package"),
                                                  this,
                                                  SLOT(OnBuildPhonePackage()));
    m_pBuildPhonePackAction->setCheckable(false);
    m_pBuildPhonePackAction->setToolTip(tr("Build phone package"));
#endif // BUILD_DESIGNER
  }

  qt::common::ScaleSlider::Embed(Qt::Vertical, *pToolBar, *m_pDrawWidget);
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

  if (m_screenshotMode)
    pToolBar->setVisible(false);

  addToolBar(Qt::RightToolBarArea, pToolBar);
}

void MainWindow::CreateCountryStatusControls()
{
  QHBoxLayout * mainLayout = new QHBoxLayout();
  m_downloadButton = new QPushButton("Download");
  mainLayout->addWidget(m_downloadButton, 0, Qt::AlignHCenter);
  m_downloadButton->setVisible(false);
  connect(m_downloadButton, SIGNAL(released()), this, SLOT(OnDownloadClicked()));

  m_retryButton = new QPushButton("Retry downloading");
  mainLayout->addWidget(m_retryButton, 0, Qt::AlignHCenter);
  m_retryButton->setVisible(false);
  connect(m_retryButton, SIGNAL(released()), this, SLOT(OnRetryDownloadClicked()));

  m_downloadingStatusLabel = new QLabel("Downloading");
  mainLayout->addWidget(m_downloadingStatusLabel, 0, Qt::AlignHCenter);
  m_downloadingStatusLabel->setVisible(false);

  m_pDrawWidget->setLayout(mainLayout);

  m_pDrawWidget->SetCurrentCountryChangedListener([this](storage::CountryId const & countryId,
                                                         std::string const & countryName,
                                                         storage::Status status,
                                                         uint64_t sizeInBytes, uint8_t progress) {
    m_lastCountry = countryId;
    if (m_lastCountry.empty() || status == storage::Status::EOnDisk || status == storage::Status::EOnDiskOutOfDate)
    {
      m_downloadButton->setVisible(false);
      m_retryButton->setVisible(false);
      m_downloadingStatusLabel->setVisible(false);
    }
    else
    {
      if (status == storage::Status::ENotDownloaded)
      {
        m_downloadButton->setVisible(true);
        m_retryButton->setVisible(false);
        m_downloadingStatusLabel->setVisible(false);

        std::string units;
        size_t sizeToDownload = 0;
        FormatMapSize(sizeInBytes, units, sizeToDownload);
        std::stringstream str;
        str << "Download (" << countryName << ") " << sizeToDownload << units;
        m_downloadButton->setText(str.str().c_str());
      }
      else if (status == storage::Status::EDownloading)
      {
        m_downloadButton->setVisible(false);
        m_retryButton->setVisible(false);
        m_downloadingStatusLabel->setVisible(true);

        std::stringstream str;
        str << "Downloading (" << countryName << ") " << (int)progress << "%";
        m_downloadingStatusLabel->setText(str.str().c_str());
      }
      else if (status == storage::Status::EInQueue)
      {
        m_downloadButton->setVisible(false);
        m_retryButton->setVisible(false);
        m_downloadingStatusLabel->setVisible(true);

        std::stringstream str;
        str << countryName << " is waiting for downloading";
        m_downloadingStatusLabel->setText(str.str().c_str());
      }
      else
      {
        m_downloadButton->setVisible(false);
        m_retryButton->setVisible(true);
        m_downloadingStatusLabel->setVisible(false);

        std::stringstream str;
        str << "Retry to download " << countryName;
        m_retryButton->setText(str.str().c_str());
      }
    }
  });
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
    m_pDrawWidget->GetFramework().SwitchMyPositionNextMode();
}

void MainWindow::OnCreateFeatureClicked()
{
  if (m_pCreateFeatureAction->isChecked())
  {
    m_pDrawWidget->ChoosePositionModeEnable();
  }
  else
  {
    m_pDrawWidget->ChoosePositionModeDisable();
    m_pDrawWidget->CreateFeature();
  }
}

void MainWindow::OnSwitchSelectionMode()
{
  m_selectionCityBoundariesMode->setChecked(false);
  m_selectionCityRoadsMode->setChecked(false);
  m_selectionMwmsBordersMode->setChecked(false);

  m_pDrawWidget->SetSelectionMode(DrawWidget::SelectionMode::Features);
}

void MainWindow::OnSwitchCityBoundariesSelectionMode()
{
  m_selectionMode->setChecked(false);
  m_selectionCityRoadsMode->setChecked(false);
  m_selectionMwmsBordersMode->setChecked(false);

  m_pDrawWidget->SetSelectionMode(DrawWidget::SelectionMode::CityBoundaries);
}

void MainWindow::OnSwitchCityRoadsSelectionMode()
{
  m_selectionMode->setChecked(false);
  m_selectionCityBoundariesMode->setChecked(false);
  m_selectionMwmsBordersMode->setChecked(false);

  m_pDrawWidget->SetSelectionMode(DrawWidget::SelectionMode::CityRoads);
}

void MainWindow::OnSwitchMwmsBordersSelectionMode()
{
  MwmsBordersSelection dlg(this);
  auto const response = dlg.ShowModal();
  if (response == MwmsBordersSelection::Response::Cancelled)
  {
    if (m_pDrawWidget->SelectionModeIsSet() &&
        IsMwmsBordersSelectionMode(m_pDrawWidget->GetSelectionMode()))
    {
      m_pDrawWidget->DropSelectionMode();
    }

    m_selectionMwmsBordersMode->setChecked(false);
    return;
  }

  m_selectionMode->setChecked(false);
  m_selectionCityBoundariesMode->setChecked(false);
  m_selectionCityRoadsMode->setChecked(false);

  m_selectionMwmsBordersMode->setChecked(true);

  m_pDrawWidget->SetSelectionMode(ConvertFromMwmsBordersSelection(response));
}

void MainWindow::OnClearSelection()
{
  m_pDrawWidget->GetFramework().GetDrapeApi().Clear();
}

void MainWindow::OnSearchButtonClicked()
{
  if (m_pSearchAction->isChecked())
    m_Docks[0]->show();
  else
    m_Docks[0]->hide();
}

void MainWindow::OnLoginMenuItem()
{
  OsmAuthDialog dlg(this);
  dlg.exec();
}

void MainWindow::OnUploadEditsMenuItem()
{
  std::string key, secret;
  if (!settings::Get(kTokenKeySetting, key) || key.empty() ||
      !settings::Get(kTokenSecretSetting, secret) || secret.empty())
  {
    OnLoginMenuItem();
  }
  else
  {
    auto & editor = osm::Editor::Instance();
    if (editor.HaveMapEditsOrNotesToUpload())
      editor.UploadChanges(key, secret, {{"created_by", "MAPS.ME " OMIM_OS_NAME}});
  }
}

void MainWindow::OnBeforeEngineCreation()
{
  m_pDrawWidget->GetFramework().SetMyPositionModeListener([this](location::EMyPositionMode mode, bool routingActive)
  {
    LocationStateModeChanged(mode);
  });
}

void MainWindow::OnPreferences()
{
  PreferencesDialog dlg(this);
  dlg.exec();

  m_pDrawWidget->GetFramework().SetupMeasurementSystem();
  m_pDrawWidget->GetFramework().EnterForeground();
}

#ifdef BUILD_DESIGNER
void MainWindow::OnBuildStyle()
{
  try
  {
    build_style::BuildAndApply(m_mapcssFilePath);
    m_pDrawWidget->RefreshDrawingRules();

    bool enabled;
    if (!settings::Get(kEnabledAutoRegenGeomIndex, enabled))
      enabled = false;

    if (enabled)
    {
      build_style::NeedRecalculate = true;
      QMainWindow::close();
    }
  }
  catch (std::exception & e)
  {
    QMessageBox msgBox;
    msgBox.setWindowTitle("Error");
    msgBox.setText(e.what());
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.setDefaultButton(QMessageBox::Ok);
    msgBox.exec();
  }
}

void MainWindow::OnRecalculateGeomIndex()
{
  try
  {
    QMessageBox msgBox;
    msgBox.setWindowTitle("Warning");
    msgBox.setText("Geometry index will be regenerated. It can take a while.\nApplication may be closed and reopened!");
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::Yes);
    if (msgBox.exec() == QMessageBox::Yes)
    {
      build_style::NeedRecalculate = true;
      QMainWindow::close();
    }
  }
  catch (std::exception & e)
  {
    QMessageBox msgBox;
    msgBox.setWindowTitle("Error");
    msgBox.setText(e.what());
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.setDefaultButton(QMessageBox::Ok);
    msgBox.exec();
  }
}

void MainWindow::OnDebugStyle()
{
  bool const checked = m_pDrawDebugRectAction->isChecked();
  m_pDrawWidget->GetFramework().EnableDebugRectRendering(checked);
  m_pDrawWidget->RefreshDrawingRules();
}

void MainWindow::OnGetStatistics()
{
  try
  {
    QString text = build_style::GetCurrentStyleStatistics();
    InfoDialog dlg(QString("Style statistics"), text, NULL);
    dlg.exec();
  }
  catch (std::exception & e)
  {
    QMessageBox msgBox;
    msgBox.setWindowTitle("Error");
    msgBox.setText(e.what());
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.setDefaultButton(QMessageBox::Ok);
    msgBox.exec();
  }
}

void MainWindow::OnRunTests()
{
  try
  {
    pair<bool, QString> res = build_style::RunCurrentStyleTests();
    InfoDialog dlg(QString("Style tests: ") + (res.first ? "OK" : "FAILED"), res.second, NULL);
    dlg.exec();
  }
  catch (std::exception & e)
  {
    QMessageBox msgBox;
    msgBox.setWindowTitle("Error");
    msgBox.setText(e.what());
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.setDefaultButton(QMessageBox::Ok);
    msgBox.exec();
  }
}

void MainWindow::OnBuildPhonePackage()
{
  try
  {
    char const * const kStylesFolder = "styles";
    char const * const kClearStyleFolder = "clear";

    QString const targetDir = QFileDialog::getExistingDirectory(nullptr, "Choose output directory");
    if (targetDir.isEmpty())
      return;
    auto outDir = QDir(JoinPathQt({targetDir, kStylesFolder}));
    if (outDir.exists())
    {
      QMessageBox msgBox;
      msgBox.setWindowTitle("Warning");
      msgBox.setText(QString("Folder ") + outDir.absolutePath() + " will be deleted?");
      msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
      msgBox.setDefaultButton(QMessageBox::No);
      auto result = msgBox.exec();
      if (result == QMessageBox::No)
        throw std::runtime_error(std::string("Target directory exists: ") + outDir.absolutePath().toStdString());
    }

    QString const stylesDir = JoinPathQt({m_mapcssFilePath, "..", "..", ".."});
    if (!QDir(JoinPathQt({stylesDir, kClearStyleFolder})).exists())
      throw std::runtime_error(std::string("Styles folder is not found in ") + stylesDir.toStdString());

    QString text = build_style::RunBuildingPhonePack(stylesDir, targetDir);
    text.append("\nMobile device style package is in the directory: ");
    text.append(JoinPathQt({targetDir, kStylesFolder}));
    text.append(". Copy it to your mobile device.\n");
    InfoDialog dlg(QString("Building phone pack"), text, nullptr);
    dlg.exec();
  }
  catch (std::exception & e)
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
  m_pDrawWidget->update();
}

#endif // NO_DOWNLOADER

void MainWindow::CreateSearchBarAndPanel()
{
  CreatePanelImpl(0, Qt::RightDockWidgetArea, tr("Search"), QKeySequence(), 0);

  SearchPanel * panel = new SearchPanel(m_pDrawWidget, m_Docks[0]);
  m_Docks[0]->setWidget(panel);
}

void MainWindow::CreatePanelImpl(size_t i, Qt::DockWidgetArea area, QString const & name,
                                 QKeySequence const & hotkey, char const * slot)
{
  ASSERT_LESS(i, m_Docks.size(), ());
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
  m_pDrawWidget->PrepareShutdown();
  e->accept();
}

void MainWindow::OnDownloadClicked()
{
  m_pDrawWidget->DownloadCountry(m_lastCountry);
}

void MainWindow::OnRetryDownloadClicked()
{
  m_pDrawWidget->RetryToDownloadCountry(m_lastCountry);
}

void MainWindow::SetEnabledTraffic(bool enable)
{
  m_selectLayerTrafficAction->setChecked(enable);
  m_pDrawWidget->GetFramework().GetTrafficManager().SetEnabled(enable);
  m_pDrawWidget->GetFramework().SaveTrafficEnabled(enable);
}

void MainWindow::SetEnabledTransit(bool enable)
{
  m_selectLayerTransitAction->setChecked(enable);
  m_pDrawWidget->GetFramework().GetTransitManager().EnableTransitSchemeMode(enable);
  m_pDrawWidget->GetFramework().SaveTransitSchemeEnabled(enable);
}

void MainWindow::SetEnabledIsolines(bool enable)
{
  m_selectLayerIsolinesAction->setChecked(enable);
  m_pDrawWidget->GetFramework().GetIsolinesManager().SetEnabled(enable);
  m_pDrawWidget->GetFramework().SaveIsolinesEnabled(enable);
}

void MainWindow::SetEnabledGuides(bool enable)
{
  m_selectLayerGuidesAction->setChecked(enable);
  m_pDrawWidget->GetFramework().GetGuidesManager().SetEnabled(enable);
  m_pDrawWidget->GetFramework().SaveGuidesEnabled(enable);
}

void MainWindow::OnTrafficEnabled()
{
  bool const enabled = m_selectLayerTrafficAction->isChecked();
  SetEnabledTraffic(enabled);

  if (enabled)
  {
    SetEnabledTransit(false);
    SetEnabledIsolines(false);
    SetEnabledGuides(false);
  }
}

void MainWindow::OnTransitEnabled()
{
  bool const enabled = m_selectLayerTransitAction->isChecked();
  SetEnabledTransit(enabled);

  if (enabled)
  {
    SetEnabledIsolines(false);
    SetEnabledTraffic(false);
    SetEnabledGuides(false);
  }
}

void MainWindow::OnIsolinesEnabled()
{
  bool const enabled = m_selectLayerIsolinesAction->isChecked();
  SetEnabledIsolines(enabled);

  if (enabled)
  {
    SetEnabledTraffic(false);
    SetEnabledTransit(false);
    SetEnabledGuides(false);
  }
}

void MainWindow::OnGuidesEnabled()
{
  bool const enabled = m_selectLayerGuidesAction->isChecked();
  SetEnabledGuides(enabled);

  if (enabled)
  {
    SetEnabledTraffic(false);
    SetEnabledTransit(false);
    SetEnabledIsolines(false);
  }
}

void MainWindow::OnRulerEnabled()
{
  m_pDrawWidget->SetRuler(m_rulerAction->isChecked());
}

void MainWindow::OnStartPointSelected()
{
  m_routePointsToolButton->setIcon(m_selectStartRoutePoint->icon());
  m_pDrawWidget->SetRoutePointAddMode(RouteMarkType::Start);
}

void MainWindow::OnFinishPointSelected()
{
  m_routePointsToolButton->setIcon(m_selectFinishRoutePoint->icon());
  m_pDrawWidget->SetRoutePointAddMode(RouteMarkType::Finish);
}

void MainWindow::OnIntermediatePointSelected()
{
  m_routePointsToolButton->setIcon(m_selectIntermediateRoutePoint->icon());
  m_pDrawWidget->SetRoutePointAddMode(RouteMarkType::Intermediate);
}

void MainWindow::OnFollowRoute()
{
  m_pDrawWidget->FollowRoute();
}

void MainWindow::OnClearRoute()
{
  m_pDrawWidget->ClearRoute();
}

void MainWindow::OnRoutingSettings()
{
  RoutingSettings dlg(this, m_pDrawWidget->GetFramework());
  dlg.ShowModal();
}

void MainWindow::OnBookmarksAction()
{
  BookmarkDialog dlg(this, m_pDrawWidget->GetFramework());
  dlg.ShowModal();
  m_pDrawWidget->update();
}

// static
void MainWindow::SetDefaultSurfaceFormat(bool apiOpenGLES3)
{
  DrawWidget::SetDefaultSurfaceFormat(apiOpenGLES3);
}
}  // namespace qt
