#include "generator/collector_city_area.hpp"

#include "generator/intermediate_data.hpp"
#include "generator/osm_element.hpp"

#include "indexer/classificator.hpp"
#include "indexer/ftypes_matcher.hpp"

#include "platform/platform.hpp"

#include "coding/internal/file_data.hpp"

#include "geometry/mercator.hpp"

#include "base/assert.hpp"
#include "base/string_utils.hpp"

#include <algorithm>
#include <cctype>
#include <iterator>

using namespace feature;
using namespace feature::serialization_policy;

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
  if (!ftypes::IsCityTownOrVillage(feature.GetTypes()))
    return;

  auto copy = feature;
  LOG(LINFO, ("here"));
  if (feature.IsPoint())
  {
    LOG(LINFO, ("is point:", osmElement.m_id, osmElement.Tags()));
    std::string populationStr = osmElement.GetTag("population");
    LOG(LINFO, ("populationStr =", populationStr));
    if (populationStr.empty())
      return;

    LOG(LINFO, ("start parse:", populationStr));
    uint32_t const population = ParsePopulationSting(populationStr);
    if (!population)
      return;

    double radius = ftypes::GetRadiusByPopulation(population);
    ftypes::LocalityType placeType = ftypes::GetPlaceType(feature.GetTypes());
    switch (placeType)
    {
    case ftypes::LocalityType::Country:
    case ftypes::LocalityType::State: return;
    case ftypes::LocalityType::City: break;
    case ftypes::LocalityType::Town: radius /= 2.0; break;
    case ftypes::LocalityType::Village: radius /= 3.0; break;
    default: return;
    }

    m2::RectD const rect =
        MercatorBounds::RectByCenterLatLonAndSizeInMeters(osmElement.m_lat, osmElement.m_lon, radius);

    copy.SetArea();
    copy.ResetGeometry();
    copy.AddPoint(rect.LeftTop());
    copy.AddPoint(rect.RightTop());
    copy.AddPoint(rect.RightBottom());
    copy.AddPoint(rect.LeftBottom());
    LOG(LINFO, ("create area:", osmElement.m_lat, osmElement.m_lon));
  }

  if (!copy.IsArea())
    return;

  if (copy.PreSerialize())
    m_writer->Write(copy);
}

void CityAreaCollector::Finish()
{
  m_writer.reset({});
}

void CityAreaCollector::Save()
{
  CHECK(!m_writer, ("Finish() has not been called."));
  if (Platform::IsFileExistsByFullPath(GetTmpFilename()))
    CHECK(base::CopyFileX(GetTmpFilename(), GetFilename()), ());
}

void CityAreaCollector::Merge(generator::CollectorInterface const & collector)
{
  collector.MergeInto(*this);
}

void CityAreaCollector::MergeInto(CityAreaCollector & collector) const
{
  CHECK(!m_writer || !collector.m_writer, ("Finish() has not been called."));
  base::AppendFileToFile(GetTmpFilename(), collector.GetTmpFilename());
}

uint32_t ParsePopulationSting(std::string const & populationStr)
{
  LOG(LINFO, ("populationStr =", populationStr));
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

  LOG(LINFO, ("number =", number));
  uint32_t result = 0;
  CHECK(strings::to_uint(number, result), (number));
  LOG(LINFO, ("result =", result));
  return result;
}
}  // namespace generator
