#pragma once
#include "indexer/feature_altitude.hpp"
#include "indexer/index.hpp"

#include "coding/memory_region.hpp"

#include <string>
#include <memory>
#include <vector>

#include "3party/succinct/rs_bit_vector.hpp"

namespace feature
{
class AltitudeLoader
{
public:
  explicit AltitudeLoader(MwmValue const & mwmValue);

  /// \returns altitude of feature with |featureId|. All items of the returned vector are valid
  /// or the returned vector is empty.
  TAltitudes const & GetAltitudes(uint32_t featureId, size_t pointCount);

  bool HasAltitudes() const;

private:
  std::unique_ptr<CopiedMemoryRegion> m_altitudeAvailabilityRegion;
  std::unique_ptr<CopiedMemoryRegion> m_featureTableRegion;

  succinct::rs_bit_vector m_altitudeAvailability;
  succinct::elias_fano m_featureTable;

  std::unique_ptr<FilesContainerR::TReader> m_reader;
  map<uint32_t, TAltitudes> m_cache;
  AltitudeHeader m_header;
  std::string m_countryFileName;
};
}  // namespace feature
