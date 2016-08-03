#pragma once
#include "indexer/feature_altitude.hpp"
#include "indexer/index.hpp"

#include "coding/memory_region.hpp"

#include "std/unique_ptr.hpp"
#include "std/vector.hpp"

#include "3party/succinct/rs_bit_vector.hpp"

namespace feature
{
class AltitudeLoader
{
public:
  explicit AltitudeLoader(MwmValue const & mwmValue);

  /// \returns altitude of feature with |featureId|. All items of the returned vector are valid
  /// or the returned vector is empty.
  TAltitudes const & GetAltitudes(uint32_t featureId, vector<double> const & distFromBeginningM);

  bool HasAltitudes() const;

private:
  unique_ptr<CopiedMemoryRegion> m_altitudeAvailabilityRegion;
  unique_ptr<CopiedMemoryRegion> m_featureTableRegion;

  succinct::rs_bit_vector m_altitudeAvailability;
  succinct::elias_fano m_featureTable;

  unique_ptr<FilesContainerR::TReader> m_reader;
  map<uint32_t, TAltitudes> m_cache;
  AltitudeHeader m_header;
};

void FillDistFormBeginning(FeatureType const & f, vector<double> & distFromBeginning);
}  // namespace feature
