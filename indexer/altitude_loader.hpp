#pragma once
#include "indexer/feature_altitude.hpp"
#include "indexer/index.hpp"
#include "indexer/mwm_set.hpp"

#include "coding/memory_region.hpp"

#include "std/string.hpp"
#include "std/unique_ptr.hpp"
#include "std/vector.hpp"

#include "3party/succinct/rs_bit_vector.hpp"

namespace feature
{
class AltitudeLoader
{
public:
  AltitudeLoader(Index const & index, MwmSet::MwmId const & mwmId);
  ~AltitudeLoader()
  {
    LOG(LINFO, ("AltitudeLoader size:", GetSizeMB(GetSize()), "MB."));
  }

  /// \returns altitude of feature with |featureId|. All items of the returned vector are valid
  /// or the returned vector is empty.
  TAltitudes const & GetAltitudes(uint32_t featureId, size_t pointCount);

  bool HasAltitudes() const;

  void ClearCache() { m_cache.clear(); }

  size_t GetSize() const
  {
    size_t cacheSz = 0;
    for (auto const & kv : m_cache)
      cacheSz += (sizeof(uint32_t) + sizeof(feature::TAltitude) * kv.second.size());

    return m_altitudeAvailabilityRegion->Size() + m_featureTableRegion->Size() + cacheSz +
           sizeof(AltitudeHeader) + m_countryFileName.size();
  }

private:
  unique_ptr<CopiedMemoryRegion> m_altitudeAvailabilityRegion;
  unique_ptr<CopiedMemoryRegion> m_featureTableRegion;

  succinct::rs_bit_vector m_altitudeAvailability;
  succinct::elias_fano m_featureTable;

  unique_ptr<FilesContainerR::TReader> m_reader;
  map<uint32_t, TAltitudes> m_cache;
  AltitudeHeader m_header;
  string m_countryFileName;
  MwmSet::MwmHandle m_handle;
};
}  // namespace feature
