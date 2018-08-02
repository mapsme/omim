#include "camera_processor.hpp"

MwmSet::MwmId CamerasInfoCollector::m_mwmId{};
uint32_t const CamerasInfoCollector::kLatestVersion = 1;

CamerasInfoCollector::CamerasInfoCollector(std::string const &dataFilePath, std::string const &camerasInfoPath,
                                           std::string const &osmIdsToFeatureIdsPath) : m_valid(true)
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

  m_mwmId = registerResult.first;

  for (auto &camera : m_cameras)
  {
    camera.parseDirection();
    camera.translateWaysIdFromOsmId(osmIdToFeatureId);
    camera.findClosestSegment(m_dataSource);
  }
}

// Modify m_ways vector. Set for each way - id of the closest segment.
void CamerasInfoCollector::Camera::findClosestSegment(FrozenDataSource const & dataSource)
{
  if (!m_ways.empty())
  {
    bool shouldErase = false;
    // If m_ways is not empty. It means, that one of point it is our camera.
    // So we should find ourselve in feature points.
    for (auto it = m_ways.begin(); it != m_ways.end();)
    {
      std::tie(it->second, shouldErase) = findMySelf(static_cast<uint32_t>(it->first), dataSource);
      if (shouldErase)
        m_ways.erase(it);
      else
        ++it;
    }

    return;
  }

  uint32_t bestFeatureId, segmentIdOfBestFeatureId;
  double minDist = std::numeric_limits<double>::max();

  // Look at each segment of roads and find the closest.
  auto const updClosestFeatureCallback = [&](FeatureType & ft)
  {
    if (ft.GetFeatureType() != feature::GEOM_LINE)
      return;

    if (!routing::IsCarRoad(feature::TypesHolder(ft)))
      return;

    double curMinDist = std::numeric_limits<double>::max();

    ft.ParseGeometry(scales::GetUpperScale());
    std::vector<m2::PointD> points(ft.GetPointsCount());
    for (size_t i = 0; i < points.size(); i++)
      points[i] = ft.GetPoint(i);

    routing::FollowedPolyline polyline(points.begin(), points.end());
    m2::RectD rect = MercatorBounds::RectByCenterXYAndSizeInMeters(m_center, kSearchCameraRadius);
    auto bestSegment = polyline.UpdateProjection(rect);

    if (bestSegment.IsValid())
    {
      auto p1 = polyline.GetPolyline().GetPoint(bestSegment.m_ind);
      auto p2 = polyline.GetPolyline().GetPoint(bestSegment.m_ind + 1);

      m2::ParametrizedSegment<m2::PointD> st(p1, p2);
      auto cameraProjOnSegment = st.ClosestPointTo(m_center);
      curMinDist = MercatorBounds::DistanceOnEarth(cameraProjOnSegment, m_center);
    }

    if (curMinDist < minDist && curMinDist < kMaxDistFromCameraToClosestSegment)
    {
      minDist = curMinDist;
      bestFeatureId = ft.GetID().m_index;
      segmentIdOfBestFeatureId = static_cast<uint32_t>(bestSegment.m_ind);
    }
  };

  dataSource.ForEachInRect(
    updClosestFeatureCallback, MercatorBounds::RectByCenterXYAndSizeInMeters(m_center, kSearchCameraRadius),
    scales::GetUpperScale());

  if (minDist != std::numeric_limits<double>::max())
    m_ways.emplace_back(bestFeatureId, segmentIdOfBestFeatureId);
}

std::pair<uint32_t, bool> CamerasInfoCollector::Camera::findMySelf(uint32_t wayId, FrozenDataSource const &dataSource)
{
  FeatureID ft(m_mwmId, wayId);
  uint32_t result = 0;
  bool isRoad = true;

  auto readFeature = [&](FeatureType &ft)
  {
    bool found = false;
    isRoad = routing::IsRoad(feature::TypesHolder(ft));
    if (!isRoad)
      return;

    auto forEachPoint = [&result, &found, this](m2::PointD const &pt)
    {
      if (found)
        return;

      if (AlmostEqualAbs(m_center, pt, kEqualityEps))
        found = true;

      result++;
    };

    ft.ForEachPoint(forEachPoint, scales::GetUpperScale());

    CHECK(found, ("Can not find camera point at the feature:", wayId));
  };

  dataSource.ReadFeature(readFeature, ft);

  return {result, !isRoad};
}


void CamerasInfoCollector::Camera::translateWaysIdFromOsmId(std::map<osm::Id, uint32_t> const &osmIdToFeatureId)
{
  for (auto it = m_ways.begin(); it != m_ways.end();)
  {
    auto const map_it = osmIdToFeatureId.find(osm::Id::Way(it->first));
    if (map_it == osmIdToFeatureId.cend())
    {
      // It means, that way was not valid, and didn't pass to mwm.
      // So we should also erase it from our set.
      m_ways.erase(it);
    }
    else
    {
      it->first = map_it->second; // osmId -> featureId
      ++it;
    }
  }
}

bool CamerasInfoCollector::ParseIntermediateInfo(string const &camerasInfoPath)
{
  FileReader fileReader(camerasInfoPath);
  uint64_t fileSize = fileReader.Size();

  uint32_t maxSpeed, relatedWaysNumber;

  // At first pair of ways osmId and None. Sometimes later became
  // pair of (feature_id, segment_id) - coord of camera, after
  // translateWaysIdFromOsmId() and findClosestSegment().
  std::vector<std::pair<uint64_t, uint32_t>> ways;

  double lat, lon;
  m2::PointD center;
  uint64_t pos = 0;

  while (pos < fileSize)
  {
    Read(fileReader, pos, &lat);
    Read(fileReader, pos, &lon);
    Read(fileReader, pos, &maxSpeed);
    Read(fileReader, pos, &relatedWaysNumber);

    ways.resize(relatedWaysNumber);
    center = MercatorBounds::FromLatLon(lat, lon);

    CHECK((0 <= maxSpeed && maxSpeed <= 255), ("Speed of camera should be in interval from 0 to 255"));
    CHECK((0 <= relatedWaysNumber && relatedWaysNumber <= 255),
          ("Number of related to camera ways should be interval from 0 to 255"));

    for (uint8_t i = 0; i < relatedWaysNumber; i++)
      Read(fileReader, pos, &ways[i].first);

    m_cameras.emplace_back(center, maxSpeed, ways);
  }

  return true;
}

void routing::BuildCamerasInfo(std::string const & dataFilePath, std::string const & camerasInfoPath,
                               std::string const & osmIdsToFeatureIdsPath)
{
  LOG(LINFO, ("Generating cameras info for", dataFilePath));

  CamerasInfoCollector collector(dataFilePath, camerasInfoPath, osmIdsToFeatureIdsPath);

  if (!collector.IsValid())
    return;

  FilesContainerW cont(dataFilePath, FileWriter::OP_WRITE_EXISTING);
  FileWriter writer = cont.GetWriter(CAMERAS_INFO_FILE_TAG);

  CamerasInfoCollector::Serialize(writer, collector.Data());
}
