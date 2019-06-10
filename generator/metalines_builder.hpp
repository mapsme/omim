#pragma once

#include "generator/collector_interface.hpp"
#include "generator/feature_builder.hpp"
#include "generator/osm_element.hpp"

#include <cstdlib>
#include <memory>
#include <string>
#include <unordered_map>

namespace generator
{
namespace cache
{
  class IntermediateDataReader;
}  // namespace cache
}  // namespace generator

namespace feature
{
/// A string of connected ways.
class LineString
{
public:
  using Ways = std::vector<int32_t>;

  explicit LineString(OsmElement const & way);

  Ways const & GetWays() const { return m_ways; }
  void Reverse();
  bool Add(LineString & line);

private:
  Ways m_ways;
  uint64_t m_start;
  uint64_t m_end;
  bool m_oneway;
};

/// Merges road segments with similar name and ref values into groups called metalines.
class MetalinesBuilder : public generator::CollectorInterface
{
public:
  explicit MetalinesBuilder(std::string const & filename);

  // CollectorInterface overrides:
  std::shared_ptr<CollectorInterface>
  Clone(std::shared_ptr<generator::cache::IntermediateDataReader> const & = {}) const override;

  /// Add a highway segment to the collection of metalines.
  void CollectFeature(FeatureBuilder const & feature, OsmElement const & element) override;
  void Save() override;

  void Merge(generator::CollectorInterface const * collector) override;
  void MergeInto(MetalinesBuilder * collector) const override;

private:
  std::unordered_multimap<size_t, LineString> m_data;
};

/// Read an intermediate file from MetalinesBuilder and convert it to an mwm section.
bool WriteMetalinesSection(std::string const & mwmPath, std::string const & metalinesPath,
                           std::string const & osmIdsToFeatureIdsPath);
}
