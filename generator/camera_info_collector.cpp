#include "generator/camera_info_collector.hpp"

#include "coding/pointd_to_pointu.hpp"
#include "coding/reader.hpp"
#include "coding/varint.hpp"

#include "base/checked_cast.hpp"

#include <algorithm>

namespace generator
{
// TODO (@gmoryes) add "inline static ..." after moving to c++17
uint32_t constexpr CamerasInfoCollector::kLatestVersion;
double constexpr CamerasInfoCollector::Camera::kEqualityEps;
double constexpr CamerasInfoCollector::kMaxDistFromCameraToClosestSegmentMeters;
double constexpr CamerasInfoCollector::kSearchCameraRadiusMeters;
uint32_t constexpr CamerasInfoCollector::kMaxCameraSpeedKmpH;

CamerasInfoCollector::CamerasInfoCollector(std::string const & dataFilePath,
                                           std::string const & camerasInfoPath,
                                           std::string const & osmIdsToFeatureIdsPath)
{
  std::map<osm::Id, uint32_t> osmIdToFeatureId;
  if (!routing::ParseOsmIdToFeatureIdMapping(osmIdsToFeatureIdsPath, osmIdToFeatureId))
  {
    LOG(LWARNING, ("An error happened while parsing feature id to osm ids mapping from file:",
                   osmIdsToFeatureIdsPath));

    m_valid = false;
    return;
  }

  if (!ParseIntermediateInfo(camerasInfoPath, osmIdToFeatureId))
  {
    LOG(LWARNING, ("Unable to parse intermediate file(", camerasInfoPath, ") about cameras info"));
    m_valid = false;
    return;
  }

  platform::LocalCountryFile file = platform::LocalCountryFile::MakeTemporary(dataFilePath);
  auto registerResult = m_dataSource.RegisterMap(file);
  if (registerResult.second != MwmSet::RegResult::Success)
  {
    LOG(LWARNING, ("Unable to RegisterMap:", dataFilePath));
    m_valid = false;
    return;
  }

  for (auto & camera : m_cameras)
  {
    camera.ParseDirection();
    camera.FindClosestSegment(m_dataSource, registerResult.first);
  }
}

void CamerasInfoCollector::Serialize(FileWriter & writer,
                                     std::vector<CamerasInfoCollector::Camera> const & cameras)
{
  // Some cameras has several ways related to them, each way has featureId and segmentId.
  // We should from each camera such type, with for example 3 ways, do 3 cameras with 1 way.
  std::vector<CamerasInfoCollector::Camera> camerasDevide;
  for (auto const & camera : cameras)
  {
    for (auto const & way : camera.m_ways)
    {
      camerasDevide.emplace_back(camera.m_center, camera.m_maxSpeedKmPH,
                                 std::vector<Camera::SegmentCoord>{way});
    }
  }

  Header header;
  header.SetVersion(CamerasInfoCollector::kLatestVersion);
  header.SetAmount(static_cast<uint32_t>(camerasDevide.size()));
  CHECK(sizeof(header) == 8, ("Strange size of speed camera section header:", sizeof(header)));
  header.Serialize(writer);

  std::sort(camerasDevide.begin(), camerasDevide.end(), [](auto const & a, auto const & b) {
    CHECK_EQUAL(a.m_ways.size(), 1, ());
    CHECK_EQUAL(b.m_ways.size(), 1, ());

    if (a.m_ways[0].m_featureId != b.m_ways[0].m_featureId)
      return a.m_ways[0].m_featureId < b.m_ways[0].m_featureId;

    if (a.m_ways[0].m_segmentId != b.m_ways[0].m_segmentId)
      return a.m_ways[0].m_segmentId < b.m_ways[0].m_segmentId;

    return a.m_ways[0].m_coef < b.m_ways[0].m_coef;
  });

  // Now each camera has only 1 way
  uint32_t prevFeatureId = 0;
  for (auto const & camera : camerasDevide)
  {
    Camera::Serialize(writer, camera, prevFeatureId);
    prevFeatureId = static_cast<uint32_t>(camera.m_ways[0].m_featureId);
  }
}

bool CamerasInfoCollector::ParseIntermediateInfo(std::string const & camerasInfoPath,
                                                 std::map<osm::Id, uint32_t> const & osmIdToFeatureId)
{
  FileReader reader(camerasInfoPath);
  ReaderSource<FileReader> src(reader);

  uint32_t maxSpeedKmPH = 0;
  uint32_t relatedWaysNumber = 0;

  // pair of (feature_id, segment_id) - coord of camera, after FindClosestSegment().
  std::vector<Camera::SegmentCoord> ways;
  double lat = 0;
  double lon = 0;
  m2::PointD center;

  while (src.Size() > 0)
  {
    ReadPrimitiveFromSource(src, lat);
    ReadPrimitiveFromSource(src, lon);
    ReadPrimitiveFromSource(src, maxSpeedKmPH);
    ReadPrimitiveFromSource(src, relatedWaysNumber);

    ways.resize(relatedWaysNumber);
    center = MercatorBounds::FromLatLon(lat, lon);

    if (maxSpeedKmPH >= kMaxCameraSpeedKmpH)
    {
      LOG(LINFO, ("Bad SpeedCamera max speed:", maxSpeedKmPH));
      ASSERT(false, ("maxSpeedKmPH:", maxSpeedKmPH, "kMax:", kMaxCameraSpeedKmpH));
      maxSpeedKmPH = 0;
    }

    CHECK((0 <= relatedWaysNumber && relatedWaysNumber <= 255),
          ("Number of related to camera ways should be interval from 0 to 255"));

    uint64_t wayOsmId;
    for (uint32_t i = 0; i < relatedWaysNumber; ++i)
    {
      ReadPrimitiveFromSource(src, wayOsmId);

      auto const it = osmIdToFeatureId.find(osm::Id::Way(wayOsmId));
      if (it == osmIdToFeatureId.cend())
        continue;
      else
        ways[i].m_featureId = it->second;
    }

    auto speed = base::asserted_cast<uint8_t>(maxSpeedKmPH);
    m_cameras.emplace_back(center, speed, std::move(ways));
  }

  return true;
}

void CamerasInfoCollector::Camera::FindClosestSegment(FrozenDataSource const & dataSource,
                                                      MwmSet::MwmId const & mwmId)
{
  if (!m_ways.empty())
  {
    // If m_ways is not empty. It means, that one of point it is our camera.
    // So we should find it in feature's points.
    for (auto it = m_ways.begin(); it != m_ways.end();)
    {
      if (auto id = FindMyself(static_cast<uint32_t>(it->m_featureId), dataSource, mwmId))
      {
        it->m_segmentId = (*id).second;
        it->m_coef = (*id).first;  // Camera starts at the begin or the end of segment.
        CHECK(it->m_coef == 0.0 || it->m_coef == 1.0, ("Coefficient must be 0.0 or 1.0 here"));
        ++it;
      }
      else
      {
        m_ways.erase(it);
      }
    }

    if (!m_ways.empty())
      return;
  }

  uint32_t bestFeatureId;
  uint32_t segmentIdOfBestFeatureId;
  auto minDist = std::numeric_limits<double>::max();
  double coef = 0;

  // Look at each segment of roads and find the closest.
  auto const updClosestFeatureCallback = [&](FeatureType & ft) {
    if (ft.GetFeatureType() != feature::GEOM_LINE)
      return;

    if (!routing::IsCarRoad(feature::TypesHolder(ft)))
      return;

    auto curMinDist = std::numeric_limits<double>::max();

    ft.ParseGeometry(scales::GetUpperScale());
    std::vector<m2::PointD> points(ft.GetPointsCount());
    for (size_t i = 0; i < points.size(); ++i)
      points[i] = ft.GetPoint(i);

    routing::FollowedPolyline polyline(points.begin(), points.end());
    m2::RectD const rect =
        MercatorBounds::RectByCenterXYAndSizeInMeters(m_center, kSearchCameraRadiusMeters);
    auto bestSegment = polyline.UpdateProjection(rect);
    double tmpCoef = 0;

    if (bestSegment.IsValid())
    {
      auto const p1 = polyline.GetPolyline().GetPoint(bestSegment.m_ind);
      auto const p2 = polyline.GetPolyline().GetPoint(bestSegment.m_ind + 1);

      m2::ParametrizedSegment<m2::PointD> st(p1, p2);
      auto const cameraProjOnSegment = st.ClosestPointTo(m_center);
      curMinDist = MercatorBounds::DistanceOnEarth(cameraProjOnSegment, m_center);

      tmpCoef = MercatorBounds::DistanceOnEarth(p1, cameraProjOnSegment) /
                MercatorBounds::DistanceOnEarth(p1, p2);
    }

    if (curMinDist < minDist && curMinDist < kMaxDistFromCameraToClosestSegmentMeters)
    {
      minDist = curMinDist;
      bestFeatureId = ft.GetID().m_index;
      segmentIdOfBestFeatureId = static_cast<uint32_t>(bestSegment.m_ind);
      coef = tmpCoef;
    }
  };

  dataSource.ForEachInRect(
      updClosestFeatureCallback,
      MercatorBounds::RectByCenterXYAndSizeInMeters(m_center, kSearchCameraRadiusMeters),
      scales::GetUpperScale());

  if (minDist != std::numeric_limits<double>::max())
    m_ways.emplace_back(bestFeatureId, segmentIdOfBestFeatureId, coef);
}

boost::optional<std::pair<double, uint32_t>> CamerasInfoCollector::Camera::FindMyself(
    uint32_t wayId, FrozenDataSource const & dataSource, MwmSet::MwmId const & mwmId) const
{
  double coef = 0;
  bool isRoad = true;
  uint32_t result = 0;

  auto const readFeature = [&coef, &result, this, &isRoad, wayId](FeatureType & ft) {
    bool found = false;
    isRoad = routing::IsRoad(feature::TypesHolder(ft));
    if (!isRoad)
      return;

    auto const forEachPoint = [&result, &found, this](m2::PointD const & pt) {
      if (found)
        return;

      if (AlmostEqualAbs(m_center, pt, kEqualityEps))
        found = true;
      else
        ++result;
    };

    ft.ForEachPoint(forEachPoint, scales::GetUpperScale());

    CHECK(found, ("Cannot find camera point at the feature:", wayId));

    // If point with number - N, is end of feature, we cannot say: segmentId = N,
    // because number of segments is N - 1, so we say segmentId = N - 1, and coef = 1
    // which means, that camera placed at the end of (N - 1)'th segment.
    if (result == ft.GetPointsCount() - 1)
    {
      CHECK_NOT_EQUAL(result, 0, ("Feature consists of one point!"));
      result--;
      coef = 1;
    }
  };

  FeatureID featureID(mwmId, wayId);
  dataSource.ReadFeature(readFeature, featureID);

  if (isRoad)
    return boost::optional<std::pair<double, uint32_t>>({coef, result});
  else
    return {};
}

void CamerasInfoCollector::Camera::Serialize(FileWriter & writer,
                                             CamerasInfoCollector::Camera const & camera,
                                             uint32_t prevFeatureId)
{
  CHECK(camera.m_ways.size() == 1, ());

  auto const & way = camera.m_ways.back();

  auto featureId = static_cast<uint32_t>(way.m_featureId);
  featureId -= prevFeatureId;  // delta coding
  WriteVarUint(writer, featureId);
  WriteVarUint(writer, way.m_segmentId);

  uint32_t coef = DoubleToUint32(way.m_coef, 0, 1, 32);
  WriteToSink(writer, coef);

  WriteVarUint(writer, camera.m_maxSpeedKmPH);

  auto const direction = static_cast<uint8_t>(camera.m_direction);
  WriteVarUint(writer, direction);

  // TODO (@gmoryes) add implementation of this feature
  // Here will be a list of time conditions, for example:
  //    "maxspeed:conditional": "60 @ (23:00-05:00)"
  //                       or
  //    "60 @ (Su 00:00-24:00; Mo-Sa 00:00-06:00,22:30-24:00)"
  // We will store each condition in 3 bytes:
  //    1. Type of condition (day, hour, month) + start value of range.
  //    2. End value of range.
  //    3. Maxspeed in this time range.
  WriteVarInt(writer, 0); // Number of time conditions.
}
}  // namespace generator

void routing::BuildCamerasInfo(std::string const & dataFilePath,
                               std::string const & camerasInfoPath,
                               std::string const & osmIdsToFeatureIdsPath)
{
  LOG(LINFO, ("Generating cameras info for", dataFilePath));

  generator::CamerasInfoCollector collector(dataFilePath, camerasInfoPath, osmIdsToFeatureIdsPath);

  if (!collector.IsValid())
  {
    LOG(LCRITICAL, ("Cannot get info about cameras"));
    return;
  }

  FilesContainerW cont(dataFilePath, FileWriter::OP_WRITE_EXISTING);
  FileWriter writer = cont.GetWriter(CAMERAS_INFO_FILE_TAG);

  generator::CamerasInfoCollector::Serialize(writer, collector.GetData());
}
