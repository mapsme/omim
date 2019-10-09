#include "generator/collector_city_area.hpp"

#include "generator/intermediate_data.hpp"
#include "generator/osm_element.hpp"

#include "indexer/classificator.hpp"
#include "indexer/ftypes_matcher.hpp"

#include "platform/platform.hpp"

#include "coding/internal/file_data.hpp"

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
boost::optional<uint64_t> GetAdminCentreFromMembers(OsmElement const & element)
{
  for (auto const & member : element.m_members)
  {
    if (member.m_type == OsmElement::EntityType::Node && member.m_role == "admin_centre")
      return {member.m_ref};
  }

  return {};
}

uint64_t GetPopulation(OsmElement const & element)
{
  std::string populationStr = element.GetTag("population");
  if (populationStr.empty())
    return 0;

  return generator::ParsePopulationSting(populationStr);
}

std::vector<m2::PointD> CreateCircleGeometry(m2::PointD const & center, double r, double angleStepDegree)
{
  std::vector<m2::PointD> result;
  double const radStep = base::DegToRad(angleStepDegree);
  for (double angleRad = 0; angleRad <= 2 * M_PI; angleRad += radStep)
    result.emplace_back(center.x + r * cos(angleRad), center.y + r * sin(angleRad));
  return result;
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
  if (feature.IsArea() && ftypes::GetPlaceType(feature.GetTypes()) == ftypes::LocalityType::City)
  {
    auto copy = feature;
    if (copy.PreSerialize())
      m_writer->Write(copy);
    return;
  }

  if (feature.IsArea())
  {
    auto const op = GetAdminCentreFromMembers(osmElement);
    if (!op)
      return;

    auto const adminCentreOsmId = *op;
    m_nodeOsmIdToBoundaries[adminCentreOsmId].emplace_back(feature);
    return;
  }
  else if (feature.IsPoint())
  {
    ftypes::LocalityType placeType = ftypes::GetPlaceType(feature.GetTypes());
    if (placeType != ftypes::LocalityType::City)
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
  m_writer = std::make_unique<FeatureBuilderWriter<MaxAccuracy>>(GetTmpFilename(),
                                                                 FileWriter::Op::OP_APPEND);
  for (auto const & item : m_nodeOsmIdToBoundaries)
  {
    uint64_t const nodeOsmId = item.first;
    auto const & featureBuilders = item.second;
    LOG(LINFO, (nodeOsmId));

    auto const it = m_nodeOsmIdToLocalityData.find(nodeOsmId);
    if (it == m_nodeOsmIdToLocalityData.cend())
      continue;

    auto const & localityData = it->second;

    size_t bestIndex = 0;
    double minArea = std::numeric_limits<double>::max();
    for (size_t i = 0; i < featureBuilders.size(); ++i)
    {
      auto const & geomentry = featureBuilders[i].GetOuterGeometry();
      double const area = MercatorBounds::MercatorSqrToMetersSqr(
          GetPolygonArea(geomentry.cbegin(), geomentry.cend()));

      if (area > 0 && minArea > area)
      {
        minArea = area;
        bestIndex = i;
      }
    }

    auto bestFeatureBuilder = featureBuilders[bestIndex];

    if (localityData.m_population == 0)
      continue;

    double const r = ftypes::GetRadiusByPopulationForRouting(localityData.m_population, localityData.m_place);
    double const areaUpperBound = M_PI * r * r;

    if (minArea > areaUpperBound)
    {
      auto const circleGeometry =
          CreateCircleGeometry(localityData.m_position, r, 1.0 /* angleStepDegree */);

      bestFeatureBuilder.SetArea();
      bestFeatureBuilder.ResetGeometry();
      for (auto const & point : circleGeometry)
        bestFeatureBuilder.AddPoint(point);
    }

    static uint32_t const kCityType = classif().GetTypeByPath({"place", "city"});
    bestFeatureBuilder.AddType(kCityType);

    if (bestFeatureBuilder.PreSerialize())
      m_writer->Write(bestFeatureBuilder);
  }

  m_writer.reset({});
  CHECK(base::CopyFileX(GetTmpFilename(), GetFilename()), ());
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
