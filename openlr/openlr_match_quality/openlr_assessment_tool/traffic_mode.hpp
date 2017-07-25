#pragma once

#include "indexer/feature.hpp"

#include "openlr/decoded_path.hpp"
#include "openlr/openlr_model.hpp"

#include "base/exception.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

#include <boost/optional.hpp>

#include <QAbstractTableModel>

class Index;
class QItemSelection;
class Selection;

DECLARE_EXCEPTION(TrafficModeError, RootException);

using FeaturePoint = std::pair<FeatureID, size_t>;

namespace impl
{
/// This class denotes a "non-deterministic" feature point.
/// I.e. it is a set of all pairs <FeatureID, point index>
/// located at a specified coordinate.
/// Only one point at a time is considered active.
class RoadPointCandidate
{
  static size_t const kInvalidId;

public:
  RoadPointCandidate(std::vector<FeaturePoint> const & points,
                     m2::PointD const & coord);

  void ActivatePoint(RoadPointCandidate const & rpc);
  FeaturePoint const & GetPoint() const;
  m2::PointD const & GetCoordinate() const;

private:
  void SetActivePoint(FeatureID const & fid);

  m2::PointD m_coord = m2::PointD::Zero();
  std::vector<FeaturePoint> m_candidates;
  size_t m_activePointIndex = kInvalidId;
};
}  // namespace impl

class SegmentCorrespondence
{
public:
  boost::optional<openlr::Path> const & GetMatchedPath() const { return m_matchedPath; }
  boost::optional<openlr::Path> & GetMatchedPath() { return m_matchedPath; }

  boost::optional<openlr::Path> const & GetFakePath() const { return m_fakePath; }
  boost::optional<openlr::Path> & GetFakePath() { return m_fakePath; }

  openlr::LinearSegment const & GetPartnerSegment() const { return m_partnerSegment; }
  openlr::LinearSegment & GetPartnerSegment() { return m_partnerSegment; }

  uint32_t GetPartnerSegmentId() const { return m_partnerSegment.m_segmentId; }

private:
  openlr::LinearSegment m_partnerSegment;
  boost::optional<openlr::Path> m_matchedPath;
  boost::optional<openlr::Path> m_fakePath;
};

/// This class is used to delegate segments drawing to the DrapeEngine.
class TrafficDrawerDelegateBase
{
public:
  virtual ~TrafficDrawerDelegateBase() = default;

  virtual void SetViewportCenter(m2::PointD const & center) = 0;

  virtual void DrawDecodedSegments(std::vector<m2::PointD> const & points) = 0;
  virtual void DrawEncodedSegment(std::vector<m2::PointD> const & points) = 0;
  virtual void ClearAllPaths() = 0;

  virtual void VisualizeGoldenPath(std::vector<m2::PointD> const & points) = 0;

  virtual void VisualizePoints(std::vector<m2::PointD> const & points) = 0;
  virtual void CleanAllVisualizedPoints() = 0;
};

class BookmarkManager;

/// This class is responsible for collecting junction points and
/// checking user's clicks.
class PointsControllerDelegateBase
{
public:
  enum class ClickType
  {
    Miss,
    Add,
    Remove
  };

  virtual std::vector<m2::PointD> GetAllJunctionPointsInViewPort() const = 0;
  /// Returns all junction points at a given location in the form of feature id and
  /// point index in the feature. And meke p equal to the neares junction.
  virtual std::vector<FeaturePoint> GetFeaturesPointsByPoint(m2::PointD & p) const = 0;
  virtual std::vector<m2::PointD> GetReachablePoints(m2::PointD const & p) const = 0;

  virtual ClickType CheckClick(m2::PointD const & clickPoint,
                       m2::PointD const & lastClickedPoint,
                       std::vector<m2::PointD> const & reachablePoints) const = 0;
};

/// This class is used to map sample ids to real data
/// and change sample evaluations.
class TrafficMode : public QAbstractTableModel
{
  Q_OBJECT

public:
  // TODO(mgsergio): Check we are on the right mwm. I.E. right mwm version an everything.
  TrafficMode(std::string const & dataFileName,
              Index const & index,
              std::unique_ptr<TrafficDrawerDelegateBase> drawerDelegate,
              std::unique_ptr<PointsControllerDelegateBase> pointsDelegate,
              QObject * parent = Q_NULLPTR);

  bool SaveSampleAs(std::string const & fileName) const;

  int rowCount(const QModelIndex & parent = QModelIndex()) const Q_DECL_OVERRIDE;
  int columnCount(const QModelIndex & parent = QModelIndex()) const Q_DECL_OVERRIDE;

  QVariant data(const QModelIndex & index, int role) const Q_DECL_OVERRIDE;
  // QVariant headerData(int section, Qt::Orientation orientation,
  //                     int role = Qt::DisplayRole) const Q_DECL_OVERRIDE;

  Qt::ItemFlags flags(QModelIndex const & index) const Q_DECL_OVERRIDE;

  bool IsBuildingPath() const { return m_buildingPath; }
  void StartBuildingPath();
  void PushPoint(m2::PointD const & coord,
                 std::vector<FeaturePoint> const & points);
  void PopPoint();
  void CommitPath();
  void RollBackPath();

  size_t GetPointsCount() const;
  m2::PointD const & GetPoint(size_t const index) const;
  m2::PointD const & GetLastPoint() const;
  std::vector<m2::PointD> GetCoordinates() const;

public slots:
  void OnItemSelected(QItemSelection const & selected, QItemSelection const &);
  void OnClick(m2::PointD const & clickPoint) { HandlePoint(clickPoint); }

private:
  void HandlePoint(m2::PointD clickPoint);

  Index const & m_index;
  std::vector<SegmentCorrespondence> m_segments;

  std::unique_ptr<TrafficDrawerDelegateBase> m_drawerDelegate;
  std::unique_ptr<PointsControllerDelegateBase> m_pointsDelegate;

  bool m_buildingPath = false;
  std::vector<impl::RoadPointCandidate> m_path;
};
