#pragma once
#include "indexer/feature_altitude.hpp"
#include "indexer/mwm_set.hpp"

#include "coding/memory_region.hpp"

#include "geometry/point_with_altitude.hpp"

#include <memory>
#include <string>
#include <vector>

#include "3party/succinct/rs_bit_vector.hpp"

class DataSource;

namespace feature
{
// @TODO(bykoianko) |m_altitudeAvailability| and |m_featureTable| are saved without
// taking into account endianness. It should be fixed. The plan is
// * to use one bit form AltitudeHeader::m_version for keeping information about endianness. (Zero
//   should be used for big endian.)
// * to check the endianness of the reader and the bit while reading and to use an appropriate
//   methods for reading.
class AltitudeLoader
{
public:
  AltitudeLoader(DataSource const & dataSource, MwmSet::MwmId const & mwmId,
                 bool twoThreadsReady);

  bool HasAltitudes() const;

  /// \returns altitude of feature with |featureId|. All items of the returned vector are valid
  /// or the returned vector is empty.
  geometry::Altitudes const & GetAltitudes(uint32_t featureId, size_t pointCount, bool isOutgoing);

  bool IsTwoThreadsReady() const { return m_handleBwd != nullptr; }

  void ClearCache(bool isOutgoing) { isOutgoing ? m_cache.clear() : m_cacheBwd.clear(); }

private:
  geometry::Altitudes const & GetAltitudes(
      uint32_t featureId, size_t pointCount, std::unique_ptr<FilesContainerR::TReader> & reader,
      std::map<uint32_t, geometry::Altitudes> & cache) const;

  std::unique_ptr<CopiedMemoryRegion> m_altitudeAvailabilityRegion;
  std::unique_ptr<CopiedMemoryRegion> m_featureTableRegion;

  succinct::rs_bit_vector m_altitudeAvailability;
  succinct::elias_fano m_featureTable;

  // Note. If |twoThreadsReady| parameter of constructor is true method GetAltitudes() may be called
  // from two different threads. In that case all calls of GetAltitudes() with |isOutgoing| == true
  // should be done from one thread and from another one of |isOutgoing| == true.
  // To call GetAltitudes() from two threads without locks it's necessary to have
  // two caches for reading from disk (in m_handle/m_reader and m_handleBwd/m_readerBwd)
  // and two caches for keeping read altitudes (m_cache and m_cacheBwd).
  std::unique_ptr<FilesContainerR::TReader> m_reader;
  std::unique_ptr<FilesContainerR::TReader> m_readerBwd;
  std::map<uint32_t, geometry::Altitudes> m_cache;
  std::map<uint32_t, geometry::Altitudes> m_cacheBwd;
  AltitudeHeader m_header;
  std::string m_countryFileName;
  MwmSet::MwmHandle m_handle;
  std::unique_ptr<MwmSet::MwmHandle> m_handleBwd;
};
}  // namespace feature
