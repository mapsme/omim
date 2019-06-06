#include "generator/collector_city_boundary.hpp"

#include "generator/feature_generator.hpp"

#include "indexer/ftypes_matcher.hpp"

#include <algorithm>
#include <iterator>

namespace generator
{
CityBoundaryCollector::CityBoundaryCollector(std::string const & filename)
  : CollectorInterface(filename) {}

std::shared_ptr<CollectorInterface> CityBoundaryCollector::Clone() const
{
  return std::make_shared<CityBoundaryCollector>(GetFilename());
}

void CityBoundaryCollector::CollectFeature(FeatureBuilder1 const & feature, OsmElement const &)
{
  if (feature.IsArea() && ftypes::IsCityTownOrVillage(feature.GetTypes()))
    m_boundaries.emplace_back(feature);
}

void CityBoundaryCollector::Save()
{
  feature::FeaturesCollector collector(GetFilename());
  for (auto & boundary : m_boundaries)
  {
    if (boundary.PreSerialize())
      collector.Collect(boundary);
  }
}

void CityBoundaryCollector::Merge(generator::CollectorInterface const * collector)
{
  CHECK(collector, ());

  collector->MergeInto(const_cast<CityBoundaryCollector *>(this));
}

void CityBoundaryCollector::MergeInto(CityBoundaryCollector * collector) const
{
  CHECK(collector, ());

  std::copy(std::begin(m_boundaries), std::end(m_boundaries),
            std::back_inserter(collector->m_boundaries));
}
}  // namespace generator
