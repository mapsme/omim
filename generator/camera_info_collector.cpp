#include "generator/camera_info_collector.hpp"

namespace generator
{
// TODO add "inline static ..." after moving to c++17
uint32_t constexpr CamerasInfoCollector::kLatestVersion;
double constexpr CamerasInfoCollector::Camera::kEqualityEps;
double constexpr CamerasInfoCollector::Camera::kSignificantPartOfSegmentCoef;
double constexpr CamerasInfoCollector::kMaxDistFromCameraToClosestSegmentMeters;
double constexpr CamerasInfoCollector::kSearchCameraRadiusMeters;
uint32_t constexpr CamerasInfoCollector::kMaxCameraSpeedKmpH;

CamerasInfoCollector::CamerasInfoCollector(std::string const & dataFilePath, std::string const & camerasInfoPath,
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

  if (!ParseIntermediateInfo(camerasInfoPath))
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
    camera.TranslateWaysIdFromOsmId(osmIdToFeatureId);
    camera.FindClosestSegment(m_dataSource, registerResult.first);
  }
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
        it->m_segmentId = *id;
        it->k = 0; // Camera starts at the begin of segment.
        ++it;
      }
      else
      {
        m_ways.erase(it);
      }
    }

    return;
  }

  uint32_t bestFeatureId, segmentIdOfBestFeatureId;
  auto minDist = std::numeric_limits<double>::max();
  double coef = 0;

  // Look at each segment of roads and find the closest.
  auto const updClosestFeatureCallback = [&](FeatureType & ft)
  {
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
    m2::RectD rect = MercatorBounds::RectByCenterXYAndSizeInMeters(m_center, kSearchCameraRadiusMeters);
    auto bestSegment = polyline.UpdateProjection(rect);
    double tmpCoef = 0;

    if (bestSegment.IsValid())
    {
      auto p1 = polyline.GetPolyline().GetPoint(bestSegment.m_ind);
      auto p2 = polyline.GetPolyline().GetPoint(bestSegment.m_ind + 1);

      m2::ParametrizedSegment<m2::PointD> st(p1, p2);
      auto cameraProjOnSegment = st.ClosestPointTo(m_center);
      curMinDist = MercatorBounds::DistanceOnEarth(cameraProjOnSegment, m_center);

      tmpCoef = MercatorBounds::DistanceOnEarth(p1, cameraProjOnSegment) / MercatorBounds::DistanceOnEarth(p1, p2);
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
    updClosestFeatureCallback, MercatorBounds::RectByCenterXYAndSizeInMeters(m_center, kSearchCameraRadiusMeters),
    scales::GetUpperScale());

  if (minDist != std::numeric_limits<double>::max())
    m_ways.emplace_back(bestFeatureId, segmentIdOfBestFeatureId, coef);
}

boost::optional<uint32_t> CamerasInfoCollector::Camera::FindMyself(uint32_t wayId,
                                                                   FrozenDataSource const & dataSource,
                                                                   MwmSet::MwmId const & mwmId)
{
  FeatureID ft(mwmId, wayId);
  uint32_t result = 0;
  bool isRoad = true;

  auto const readFeature = [&result, this, &isRoad, wayId](FeatureType & ft)
  {
    bool found = false;
    isRoad = routing::IsRoad(feature::TypesHolder(ft));
    if (!isRoad)
      return;

    auto const forEachPoint = [&result, &found, this](m2::PointD const & pt)
    {
      if (found)
        return;

      if (AlmostEqualAbs(m_center, pt, kEqualityEps))
        found = true;

      result++;
    };

    ft.ForEachPoint(forEachPoint, scales::GetUpperScale());

    CHECK(found, ("Cannot find camera point at the feature:", wayId));
  };

  dataSource.ReadFeature(readFeature, ft);

  return isRoad ? boost::optional<uint32_t>(result) : boost::optional<uint32_t>();
}

void CamerasInfoCollector::Camera::TranslateWaysIdFromOsmId(std::map<osm::Id, uint32_t> const & osmIdToFeatureId)
{
  for (auto it = m_ways.begin(); it != m_ways.end();)
  {
    auto const mapIt = osmIdToFeatureId.find(osm::Id::Way(it->m_featureId));
    if (mapIt == osmIdToFeatureId.cend())
    {
      // It means, that way was not valid, and didn't pass to mwm.
      // So we should also erase it from our set.
      m_ways.erase(it);
    }
    else
    {
      it->m_featureId = mapIt->second; // osmId -> featureId
      ++it;
    }
  }
}

void CamerasInfoCollector::Camera::Serialize(FileWriter & writer, CamerasInfoCollector::Camera const & camera)
{
  auto waysNumber = base::asserted_cast<uint8_t>(camera.m_ways.size());
  WriteToSink(writer, waysNumber);

  for (auto const & way : camera.m_ways)
  {
    auto const featureId = static_cast<uint32_t>(way.m_featureId);
    WriteToSink(writer, featureId);
    WriteToSink(writer, way.m_segmentId);

    auto const coef = static_cast<uint32_t>(way.k * kSignificantPartOfSegmentCoef);
    WriteToSink(writer, coef);
  }

  auto const speed = static_cast<uint8_t>(camera.m_maxSpeed);
  WriteToSink(writer, speed);

  auto const direction = static_cast<uint8_t>(camera.m_direction);
  WriteToSink(writer, direction);

  // TODO add implementation of this feature
  WriteToSink(writer, 0); // Time, from which camera is on
  WriteToSink(writer, 0); // Time, from which camera is off
}

bool CamerasInfoCollector::ParseIntermediateInfo(string const & camerasInfoPath)
{
  FileReader reader(camerasInfoPath);
  ReaderSource<FileReader> src(reader);

  uint32_t maxSpeed, relatedWaysNumber;

  // At first pair of ways osmId and None. Sometimes later become
  // pair of (feature_id, segment_id) - coord of camera, after
  // TranslateWaysIdFromOsmId() and FindClosestSegment().
  std::vector<Camera::SegmentCoord> ways;
  double lat, lon;
  m2::PointD center;

  while (src.Size() > 0)
  {
    src.Read(&lat, sizeof(lat));
    src.Read(&lon, sizeof(lon));
    src.Read(&maxSpeed, sizeof(maxSpeed));
    src.Read(&relatedWaysNumber, sizeof(relatedWaysNumber));

    ways.resize(relatedWaysNumber);
    center = MercatorBounds::FromLatLon(lat, lon);

    CHECK((0 <= maxSpeed && maxSpeed <= kMaxCameraSpeedKmpH), ());
    CHECK((0 <= relatedWaysNumber && relatedWaysNumber <= 255),
          ("Number of related to camera ways should be interval from 0 to 255"));

    for (uint32_t i = 0; i < relatedWaysNumber; ++i)
      src.Read(&ways[i].m_featureId, sizeof(ways[i].m_featureId));

    m_cameras.emplace_back(center, maxSpeed, std::move(ways));
  }

  return true;
}

void CamerasInfoCollector::Serialize(FileWriter & writer, std::vector<CamerasInfoCollector::Camera> const & cameras)
{
  WriteToSink(writer, CamerasInfoCollector::kLatestVersion);

  auto amount = static_cast<uint32_t>(cameras.size());
  WriteToSink(writer, amount);

  for (auto const & camera : cameras)
    Camera::Serialize(writer, camera);
}
} // namespace generator

void routing::BuildCamerasInfo(std::string const & dataFilePath, std::string const & camerasInfoPath,
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

