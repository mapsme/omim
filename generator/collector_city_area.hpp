#pragma once

#include "generator/collector_interface.hpp"
#include "generator/feature_builder.hpp"

#include "indexer/ftypes_matcher.hpp"

#include <cstdint>
#include <limits>
#include <memory>

namespace generator
{
namespace cache
{
class IntermediateDataReader;
}  // namespace cache

class CityAreaCollector : public CollectorInterface
{
public:
  using AdminLevel = uint8_t;
  static AdminLevel constexpr kNoAdminLevel = std::numeric_limits<AdminLevel>::max();

  struct LocalityData
  {
    LocalityData(uint64_t population, ftypes::LocalityType place, m2::PointD const & position)
      : m_population(population), m_place(place), m_position(position)
    {
    }

    uint64_t m_population;
    ftypes::LocalityType m_place;
    m2::PointD m_position;
  };

  explicit CityAreaCollector(std::string const & filename);

  // CollectorInterface overrides:
  std::shared_ptr<CollectorInterface>
  Clone(std::shared_ptr<cache::IntermediateDataReader> const & = {}) const override;

  void CollectFeature(feature::FeatureBuilder const & feature, OsmElement const & osmElement) override;
  void Finish() override;
  void Save() override;

  void Merge(generator::CollectorInterface const & collector) override;
  void MergeInto(CityAreaCollector & collector) const override;

private:



  std::unordered_map<uint64_t, LocalityData> m_nodeOsmIdToLocalityData;
  std::unordered_map<uint64_t, std::vector<feature::FeatureBuilder>> m_nodeOsmIdToBoundaries;
  std::unique_ptr<feature::FeatureBuilderWriter<feature::serialization_policy::MaxAccuracy>> m_writer;
};

uint64_t ParsePopulationSting(std::string const & populationStr);
}  // namespace generator
