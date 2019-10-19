#include "generator/collector_city_area.hpp"

#include "generator/intermediate_data.hpp"
#include "generator/osm_element.hpp"

#include "indexer/classificator.hpp"
#include "indexer/ftypes_matcher.hpp"

#include "coding/internal/file_data.hpp"

#include "geometry/area_on_earth.hpp"
#include "geometry/mercator.hpp"
#include "geometry/polygon.hpp"

#include "base/assert.hpp"
#include "base/string_utils.hpp"

#include <algorithm>
#include <cctype>
#include <iterator>

using namespace feature;
using namespace feature::serialization_policy;

namespace
{
boost::optional<uint64_t> GetAdminNodeFromMembers(OsmElement const & element)
{
  uint64_t adminCentreRef = 0;
  uint64_t labelRef = 0;
  for (auto const & member : element.m_members)
  {
    if (member.m_type == OsmElement::EntityType::Node)
    {
      if (member.m_role == "admin_centre")
        adminCentreRef = member.m_ref;
      else if (member.m_role == "label")
        labelRef = member.m_ref;
    }
  }

  if (labelRef)
    return {labelRef};

  if (adminCentreRef)
    return {adminCentreRef};

  return {};
}

uint64_t GetPopulation(OsmElement const & element)
{
  std::string populationStr = element.GetTag("population");
  if (populationStr.empty())
    return 0;

  return generator::ParsePopulationSting(populationStr);
}

std::vector<m2::PointD> CreateCircleGeometry(m2::PointD const & center, double radiusMercator,
                                             double angleStepDegree)
{
  std::vector<m2::PointD> result;
  double const radStep = base::DegToRad(angleStepDegree);
  for (double angleRad = 0; angleRad <= 2 * math::pi; angleRad += radStep)
  {
    result.emplace_back(center.x + radiusMercator * cos(angleRad),
                        center.y + radiusMercator * sin(angleRad));
  }
  return result;
}

bool IsSuitablePlaceType(ftypes::LocalityType localityType)
{
  switch (localityType)
  {
  case ftypes::LocalityType::City:
  case ftypes::LocalityType::Town:
  case ftypes::LocalityType::Village: return true;
  default: return false;
  }
}
}  // namespace

namespace generator
{
CityAreaCollector::CityAreaCollector(std::string const & filename)
  : CollectorInterface(filename),
    m_writer(std::make_unique<FeatureBuilderWriter<MaxAccuracy>>(GetTmpFilename())) {}

std::shared_ptr<CollectorInterface>
CityAreaCollector::Clone(std::shared_ptr<cache::IntermediateDataReader> const &) const
{
  return std::make_shared<CityAreaCollector>(GetFilename());
}

void CityAreaCollector::CollectFeature(FeatureBuilder const & feature, OsmElement const & osmElement)
{
  if (osmElement.m_id == 162337)
  {
    int asd = 123;
    (void)asd;
  }
  if (feature.IsArea() && IsSuitablePlaceType(ftypes::GetPlaceType(feature.GetTypes())))
  {
    auto copy = feature;
    if (copy.PreSerialize())
      m_writer->Write(copy);
    return;
  }

  if (feature.IsArea())
  {
    auto const op = GetAdminNodeFromMembers(osmElement);
    if (!op)
      return;

    auto const nodeOsmId = *op;
    m_nodeOsmIdToBoundaries[nodeOsmId].emplace_back(feature);
    return;
  }
  else if (feature.IsPoint())
  {
    ftypes::LocalityType placeType = ftypes::GetPlaceType(feature.GetTypes());
    if (!IsSuitablePlaceType(placeType))
      return;

    uint64_t const population = GetPopulation(osmElement);
    uint64_t nodeOsmId = osmElement.m_id;
    m2::PointD const center = MercatorBounds::FromLatLon(osmElement.m_lat, osmElement.m_lon);
    m_nodeOsmIdToLocalityData.emplace(nodeOsmId, LocalityData(population, placeType, center));
  }
}

void CityAreaCollector::Finish()
{
  m_writer.reset({});
}

void CityAreaCollector::Save()
{
  uint32_t pointToCircle = 0;
  uint32_t matchBoundary = 0;
  m_writer = std::make_unique<FeatureBuilderWriter<MaxAccuracy>>(GetTmpFilename(),
                                                                 FileWriter::Op::OP_APPEND);
  for (auto const & item : m_nodeOsmIdToBoundaries)
  {
    uint64_t const nodeOsmId = item.first;
    auto const & featureBuilders = item.second;

    auto const it = m_nodeOsmIdToLocalityData.find(nodeOsmId);
    if (it == m_nodeOsmIdToLocalityData.cend())
      continue;

    auto const & localityData = it->second;

    size_t bestIndex = 0;
    double minArea = std::numeric_limits<double>::max();
    for (size_t i = 0; i < featureBuilders.size(); ++i)
    {
      auto const & geometry = featureBuilders[i].GetOuterGeometry();
      double const area = ms::AreaOnEarth(geometry);
      if (area < 0)
      {
        LOG(LINFO, ("area =", area, "id =", featureBuilders[i]));
        static int cnt = 0;
        std::ofstream output("/home/m.gorbushin/tmp/" + std::to_string(cnt) + ".points");
        output << std::setprecision(20);
        cnt++;
        for (auto const & point : geometry)
        {
          auto const latlon = MercatorBounds::ToLatLon(point);
          output << latlon.m_lat << " " << latlon.m_lon << std::endl;
        }
        continue;
      }

      if (minArea > area)
      {
        minArea = area;
        bestIndex = i;
      }
    }

    auto bestFeatureBuilder = featureBuilders[bestIndex];

    if (localityData.m_population == 0)
      continue;

    double const radiusMeters =
        ftypes::GetRadiusByPopulationForRouting(localityData.m_population, localityData.m_place);
    double const areaUpperBound = ms::CircleAreaOnEarth(radiusMeters);

    if (minArea > areaUpperBound)
    {
      ++pointToCircle;
      auto const circleGeometry = CreateCircleGeometry(localityData.m_position,
                                                       MercatorBounds::MetersToMercator(radiusMeters),
                                                       1.0 /* angleStepDegree */);

      bestFeatureBuilder.SetArea();
      bestFeatureBuilder.ResetGeometry();
      for (auto const & point : circleGeometry)
        bestFeatureBuilder.AddPoint(point);
    }
    else
    {
      ++matchBoundary;
    }

    static std::unordered_map<ftypes::LocalityType, uint32_t> const kLocalityTypeToClassifType = {
        {ftypes::LocalityType::City, classif().GetTypeByPath({"place", "city"})},
        {ftypes::LocalityType::Town, classif().GetTypeByPath({"place", "town"})},
        {ftypes::LocalityType::Village, classif().GetTypeByPath({"place", "village"})},
    };

    CHECK_NOT_EQUAL(kLocalityTypeToClassifType.count(localityData.m_place), 0, ());
    bestFeatureBuilder.AddType(kLocalityTypeToClassifType.at(localityData.m_place));

    if (bestFeatureBuilder.PreSerialize())
      m_writer->Write(bestFeatureBuilder);
  }

  m_writer.reset({});
  CHECK(base::CopyFileX(GetTmpFilename(), GetFilename()), ());
  LOG(LINFO, ("pointToCircle =", pointToCircle, "matchBoundary =", matchBoundary));
}

void CityAreaCollector::Merge(generator::CollectorInterface const & collector)
{
  collector.MergeInto(*this);
}

void CityAreaCollector::MergeInto(CityAreaCollector & collector) const
{
  for (auto const & item : m_nodeOsmIdToBoundaries)
  {
    uint64_t const osmId = item.first;
    auto const & featureBuilders = item.second;

    collector.m_nodeOsmIdToBoundaries.emplace(osmId, featureBuilders);
  }

  for (auto const & item : m_nodeOsmIdToLocalityData)
  {
    uint64_t const osmId = item.first;
    auto const & localityData = item.second;

    collector.m_nodeOsmIdToLocalityData.emplace(osmId, localityData);
  }

  CHECK(!m_writer || !collector.m_writer, ("Finish() has not been called."));
  base::AppendFileToFile(GetTmpFilename(), collector.GetTmpFilename());
}

uint64_t ParsePopulationSting(std::string const & populationStr)
{
  std::string number;
  for (auto const c : populationStr)
  {
    if (isdigit(c))
      number += c;
    else if (c == '.' || c == ',' || c == ' ')
      continue;
    else
      break;
  }

  if (number.empty())
    return 0;

  uint64_t result = 0;
  CHECK(strings::to_uint64(number, result), (number));
  return result;
}
}  // namespace generator