#include "openlr/openlr_match_quality/openlr_assessment_tool/mainwindow.hpp"

#include "openlr/openlr_match_quality/openlr_assessment_tool/map_widget.hpp"
#include "openlr/openlr_match_quality/openlr_assessment_tool/points_controller_delegate_base.hpp"
#include "openlr/openlr_match_quality/openlr_assessment_tool/traffic_drawer_delegate_base.hpp"
#include "openlr/openlr_match_quality/openlr_assessment_tool/traffic_panel.hpp"
#include "openlr/openlr_match_quality/openlr_assessment_tool/trafficmodeinitdlg.h"

#include "map/framework.hpp"

#include "drape_frontend/drape_api.hpp"

#include "routing/features_road_graph.hpp"
#include "routing/road_graph.hpp"

#include "routing_common/car_model.hpp"

#include "storage/country_parent_getter.hpp"

#include "geometry/mercator.hpp"
#include "geometry/point2d.hpp"

#include <QApplication>
#include <QClipboard>
#include <QDockWidget>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLayout>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStandardPaths>

#include <cerrno>
#include <cstring>
#include <vector>

using namespace openlr;

namespace
{
class TrafficDrawerDelegate : public TrafficDrawerDelegateBase
{
  static constexpr char const * kEncodedLineId = "encodedPath";
  static constexpr char const * kDecodedLineId = "decodedPath";
  static constexpr char const * kGoldenLineId = "goldenPath";

public:
  TrafficDrawerDelegate(Framework & framework)
    : m_framework(framework)
    , m_drapeApi(m_framework.GetDrapeApi())
    , m_bm(framework.GetBookmarkManager())
  {
  }

  void SetViewportCenter(m2::PointD const & center) override
  {
    m_framework.SetViewportCenter(center);
  }

  void DrawDecodedSegments(std::vector<m2::PointD> const & points) override
  {
    CHECK(!points.empty(), ("Points must not be empty."));

    LOG(LINFO, ("Decoded segment", points));
    m_drapeApi.AddLine(kDecodedLineId,
                       df::DrapeApiLineData(points, dp::Color(0, 0, 255, 255))
                       .Width(3.0f).ShowPoints(true /* markPoints */));
  }

  void DrawEncodedSegment(std::vector<m2::PointD> const & points) override
  {
    LOG(LINFO, ("Encoded segment", points));
    m_drapeApi.AddLine(kEncodedLineId,
                       df::DrapeApiLineData(points, dp::Color(255, 0, 0, 255))
                       .Width(3.0f).ShowPoints(true /* markPoints */));
  }

  void DrawGoldenPath(std::vector<m2::PointD> const & points) override
  {
    m_drapeApi.AddLine(kGoldenLineId,
                       df::DrapeApiLineData(points, dp::Color(255, 127, 36, 255))
                       .Width(4.0f).ShowPoints(true /* markPoints */));
  }

  void ClearGoldenPath() override
  {
    m_drapeApi.RemoveLine(kGoldenLineId);
  }

  void ClearAllPaths() override
  {
    m_drapeApi.Clear();
  }

  void VisualizePoints(std::vector<m2::PointD> const & points) override
  {
    auto editSession = m_bm.GetEditSession();
    editSession.SetIsVisible(UserMark::Type::DEBUG_MARK, true);
    for (auto const & p : points)
      editSession.CreateUserMark<DebugMarkPoint>(p);
  }

  void ClearAllVisualizedPoints() override
  {
    m_bm.GetEditSession().ClearGroup(UserMark::Type::DEBUG_MARK);
  }

private:
  Framework & m_framework;
  df::DrapeApi & m_drapeApi;
  BookmarkManager & m_bm;
};

bool PointsMatch(m2::PointD const & a, m2::PointD const & b)
{
  auto constexpr kToleranceDistanceM = 1.0;
  return MercatorBounds::DistanceOnEarth(a, b) < kToleranceDistanceM;
}

class PointsControllerDelegate : public PointsControllerDelegateBase
{
public:
  PointsControllerDelegate(Framework & framework)
    : m_framework(framework)
    , m_dataSource(framework.GetDataSource())
    , m_roadGraph(m_dataSource, routing::IRoadGraph::Mode::ObeyOnewayTag,
                  make_unique<routing::CarModelFactory>(storage::CountryParentGetter{}))
  {
  }

  std::vector<m2::PointD> GetAllJunctionPointsInViewport() const override
  {
    std::vector<m2::PointD> points;
    auto const & rect = m_framework.GetCurrentViewport();
    auto const pushPoint = [&points, &rect](m2::PointD const & point) {
      if (!rect.IsPointInside(point))
        return;
      for (auto const & p : points)
      {
        if (PointsMatch(point, p))
          return;
      }
      points.push_back(point);
    };

    auto const pushFeaturePoints = [&pushPoint](FeatureType & ft) {
      if (ft.GetFeatureType() != feature::GEOM_LINE)
        return;
      auto const roadClass = ftypes::GetHighwayClass(feature::TypesHolder(ft));
      if (roadClass == ftypes::HighwayClass::Error ||
          roadClass == ftypes::HighwayClass::Pedestrian)
      {
        return;
      }
      ft.ForEachPoint(pushPoint, scales::GetUpperScale());
    };

    m_dataSource.ForEachInRect(pushFeaturePoints, rect, scales::GetUpperScale());
    return points;
  }

  std::pair<std::vector<FeaturePoint>, m2::PointD> GetCandidatePoints(
      m2::PointD const & p) const override
  {
    auto constexpr kInvalidIndex = std::numeric_limits<size_t>::max();

    std::vector<FeaturePoint> points;
    m2::PointD pointOnFt;
    indexer::ForEachFeatureAtPoint(m_dataSource, [&points, &p, &pointOnFt](FeatureType & ft) {
        if (ft.GetFeatureType() != feature::GEOM_LINE)
          return;

        ft.ParseGeometry(FeatureType::BEST_GEOMETRY);

        auto minDistance = std::numeric_limits<double>::max();
        auto bestPointIndex = kInvalidIndex;
        for (size_t i = 0; i < ft.GetPointsCount(); ++i)
        {
          auto const & fp = ft.GetPoint(i);
          auto const distance = MercatorBounds::DistanceOnEarth(fp, p);
          if (PointsMatch(fp, p) && distance < minDistance)
          {
            bestPointIndex = i;
            minDistance = distance;
          }
        }

        if (bestPointIndex != kInvalidIndex)
        {
          points.emplace_back(ft.GetID(), bestPointIndex);
          pointOnFt = ft.GetPoint(bestPointIndex);
        }
      },
      p);
    return std::make_pair(points, pointOnFt);

  }

  std::vector<m2::PointD> GetReachablePoints(m2::PointD const & p) const override
  {
    routing::FeaturesRoadGraph::TEdgeVector edges;
    m_roadGraph.GetOutgoingEdges(
        routing::Junction(p, feature::kDefaultAltitudeMeters),
        edges);

    std::vector<m2::PointD> points;
    for (auto const & e : edges)
      points.push_back(e.GetEndJunction().GetPoint());
    return points;
  }

  ClickType CheckClick(m2::PointD const & clickPoint,
                       m2::PointD const & lastClickedPoint,
                       std::vector<m2::PointD> const & reachablePoints) const override
  {
    // == Comparison is safe here since |clickPoint| is adjusted by GetFeaturesPointsByPoint
    // so to be equal the closest feature's one.
    if (clickPoint == lastClickedPoint)
      return ClickType::Remove;
    for (auto const & p : reachablePoints)
    {
      if (PointsMatch(clickPoint, p))
        return ClickType::Add;
    }
    return ClickType::Miss;
  }

private:
  Framework & m_framework;
  DataSource const & m_dataSource;
  routing::FeaturesRoadGraph m_roadGraph;
};
}  // namespace

namespace openlr
{
MainWindow::MainWindow(Framework & framework)
  : m_framework(framework)
{
  m_mapWidget = new MapWidget(
      m_framework, false /* apiOpenGLES3 */, this /* parent */
  );

  m_layout = new QHBoxLayout();
  m_layout->addWidget(m_mapWidget);

  auto * window = new QWidget();
  window->setLayout(m_layout);
  window->setGraphicsEffect(nullptr);

  setCentralWidget(window);

  // setWindowTitle(tr("MAPS.ME"));
  // setWindowIcon(QIcon(":/ui/logo.png"));

  QMenu * fileMenu = new QMenu("File", this);
  menuBar()->addMenu(fileMenu);

  fileMenu->addAction("Open sample", this, &MainWindow::OnOpenTrafficSample,
                      QKeySequence("Ctrl+O"));

  m_closeTrafficSampleAction = fileMenu->addAction(
      "Close sample", this, &MainWindow::OnCloseTrafficSample, QKeySequence("Ctrl+W"));
  m_saveTrafficSampleAction = fileMenu->addAction(
      "Save sample", this, &MainWindow::OnSaveTrafficSample, QKeySequence("Ctrl+S"));

  fileMenu->addSeparator();

  m_goldifyMatchedPathAction = fileMenu->addAction(
      "Goldify", [this] { m_trafficMode->GoldifyMatchedPath(); }, QKeySequence("Ctrl+G"));
  m_startEditingAction = fileMenu->addAction("Edit",
                                             [this] {
                                               m_trafficMode->StartBuildingPath();
                                               m_mapWidget->SetMode(MapWidget::Mode::TrafficMarkup);
                                               m_commitPathAction->setEnabled(true /* enabled */);
                                               m_cancelPathAction->setEnabled(true /* enabled */);
                                             },
                                             QKeySequence("Ctrl+E"));
  m_commitPathAction = fileMenu->addAction("Accept path",
                                           [this] {
                                             m_trafficMode->CommitPath();
                                             m_mapWidget->SetMode(MapWidget::Mode::Normal);
                                           },
                                           QKeySequence("Ctrl+A"));
  m_cancelPathAction = fileMenu->addAction("Revert path",
                                           [this] {
                                             m_trafficMode->RollBackPath();
                                             m_mapWidget->SetMode(MapWidget::Mode::Normal);
                                           },
                                           QKeySequence("Ctrl+R"));
  m_ignorePathAction = fileMenu->addAction("Ignore path",
                                           [this] {
                                             m_trafficMode->IgnorePath();
                                             m_mapWidget->SetMode(MapWidget::Mode::Normal);
                                           },
                                           QKeySequence("Ctrl+I"));

  m_goldifyMatchedPathAction->setEnabled(false /* enabled */);
  m_closeTrafficSampleAction->setEnabled(false /* enabled */);
  m_saveTrafficSampleAction->setEnabled(false /* enabled */);
  m_startEditingAction->setEnabled(false /* enabled */);
  m_commitPathAction->setEnabled(false /* enabled */);
  m_cancelPathAction->setEnabled(false /* enabled */);
  m_ignorePathAction->setEnabled(false /* enabled */);
}

void MainWindow::CreateTrafficPanel(string const & dataFilePath)
{
  m_trafficMode = new TrafficMode(dataFilePath,
                                  m_framework.GetDataSource(),
                                  make_unique<TrafficDrawerDelegate>(m_framework),
                                  make_unique<PointsControllerDelegate>(m_framework));

  connect(m_mapWidget, &MapWidget::TrafficMarkupClick,
          m_trafficMode, &TrafficMode::OnClick);
  connect(m_trafficMode, &TrafficMode::EditingStopped,
          this, &MainWindow::OnPathEditingStop);
  connect(m_trafficMode, &TrafficMode::SegmentSelected,
          [](int segmentId) { QApplication::clipboard()->setText(QString::number(segmentId)); });

  m_docWidget = new QDockWidget(tr("Routes"), this);
  addDockWidget(Qt::DockWidgetArea::RightDockWidgetArea, m_docWidget);

  m_docWidget->setWidget(new TrafficPanel(m_trafficMode, m_docWidget));

  m_docWidget->adjustSize();
  m_docWidget->show();
}

void MainWindow::DestroyTrafficPanel()
{
  removeDockWidget(m_docWidget);
  delete m_docWidget;
  m_docWidget = nullptr;

  delete m_trafficMode;
  m_trafficMode = nullptr;

  m_mapWidget->SetMode(MapWidget::Mode::Normal);
}

void MainWindow::OnOpenTrafficSample()
{
  TrafficModeInitDlg dlg;
  dlg.exec();
  if (dlg.result() != QDialog::DialogCode::Accepted)
    return;

  try
  {
    CreateTrafficPanel(dlg.GetDataFilePath());
  }
  catch (TrafficModeError const & e)
  {
    QMessageBox::critical(this, "Data loading error", QString("Can't load data file."));
    LOG(LERROR, (e.Msg()));
    return;
  }

  m_goldifyMatchedPathAction->setEnabled(true /* enabled */);
  m_closeTrafficSampleAction->setEnabled(true /* enabled */);
  m_saveTrafficSampleAction->setEnabled(true /* enabled */);
  m_startEditingAction->setEnabled(true /* enabled */);
  m_ignorePathAction->setEnabled(true /* enabled */);
}

void MainWindow::OnCloseTrafficSample()
{
  // TODO(mgsergio):
  // If not saved, ask a user if he/she wants to save.
  // OnSaveTrafficSample()

  m_goldifyMatchedPathAction->setEnabled(false /* enabled */);
  m_saveTrafficSampleAction->setEnabled(false /* enabled */);
  m_closeTrafficSampleAction->setEnabled(false /* enabled */);
  m_startEditingAction->setEnabled(false /* enabled */);
  m_commitPathAction->setEnabled(false /* enabled */);
  m_cancelPathAction->setEnabled(false /* enabled */);
  m_ignorePathAction->setEnabled(false /* enabled */);

  DestroyTrafficPanel();
}

void MainWindow::OnSaveTrafficSample()
{
  // TODO(mgsergio): Add default filename.
  auto const & fileName = QFileDialog::getSaveFileName(this, "Save sample");
  if (fileName.isEmpty())
    return;

  if (!m_trafficMode->SaveSampleAs(fileName.toStdString()))
  {
    QMessageBox::critical(
        this, "Saving error",
        QString("Can't save file: ") + strerror(errno));
  }
}

void MainWindow::OnPathEditingStop()
{
  m_commitPathAction->setEnabled(false /* enabled */);
  m_cancelPathAction->setEnabled(false /* enabled */);
  m_cancelPathAction->setEnabled(false /* enabled */);
}
}  // namespace openlr
