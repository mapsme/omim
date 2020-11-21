#include "indexer/altitude_loader.hpp"

#include "indexer/data_source.hpp"

#include "coding/reader.hpp"
#include "coding/succinct_mapper.hpp"

#include "base/logging.hpp"
#include "base/stl_helpers.hpp"
#include "base/thread.hpp"

#include "defines.hpp"

#include <algorithm>

#include "3party/succinct/mapper.hpp"

using namespace std;

namespace
{
template <class TCont>
void LoadAndMap(size_t dataSize, ReaderSource<FilesContainerR::TReader> & src, TCont & cont,
                unique_ptr<CopiedMemoryRegion> & region)
{
  vector<uint8_t> data(dataSize);
  src.Read(data.data(), data.size());
  region = make_unique<CopiedMemoryRegion>(move(data));
  coding::MapVisitor visitor(region->ImmutableData());
  cont.map(visitor);
}
}  // namespace

namespace feature
{
AltitudeLoader::AltitudeLoader(DataSource const & dataSource, MwmSet::MwmId const & mwmId,
                               bool twoThreadsReady)
  : m_handle(dataSource.GetMwmHandleById(mwmId))
  , m_handleBwd(twoThreadsReady ? make_unique<MwmSet::MwmHandle>(dataSource.GetMwmHandleById(mwmId))
                                : nullptr)
{
  if (!m_handle.IsAlive() || (IsTwoThreadsReady() && !m_handleBwd->IsAlive()))
    return;

  auto const & mwmValue = *m_handle.GetValue();

  m_countryFileName = mwmValue.GetCountryFileName();

  CHECK_GREATER_OR_EQUAL(mwmValue.GetHeader().GetFormat(), version::Format::v8,
                         ("Unsupported mwm format"));

  if (!mwmValue.m_cont.IsExist(ALTITUDES_FILE_TAG))
    return;

  try
  {
    m_reader = make_unique<FilesContainerR::TReader>(mwmValue.m_cont.GetReader(ALTITUDES_FILE_TAG));
    ReaderSource<FilesContainerR::TReader> src(*m_reader);
    m_header.Deserialize(src);

    LoadAndMap(m_header.GetAltitudeAvailabilitySize(), src, m_altitudeAvailability,
               m_altitudeAvailabilityRegion);
    LoadAndMap(m_header.GetFeatureTableSize(), src, m_featureTable, m_featureTableRegion);

    if (IsTwoThreadsReady())
    {
      m_readerBwd = make_unique<FilesContainerR::TReader>(
          m_handleBwd->GetValue()->m_cont.GetReader(ALTITUDES_FILE_TAG));
    }
  }
  catch (Reader::OpenException const & e)
  {
    m_header.Reset();
    LOG(LERROR, ("File", m_countryFileName, "Error while reading", ALTITUDES_FILE_TAG, "section.", e.Msg()));
  }
}

bool AltitudeLoader::HasAltitudes() const
{
  return m_reader != nullptr && m_header.m_minAltitude != geometry::kInvalidAltitude;
}

geometry::Altitudes const & AltitudeLoader::GetAltitudes(uint32_t featureId, size_t pointCount,
                                                         bool isOutgoing)
{
  // Note. The backward reader and cache should not be used in case of calling GetAltitudes()
  // method from one thread.
  auto isForward = IsTwoThreadsReady() ? isOutgoing : true;
  return GetAltitudes(featureId, pointCount, isForward ? m_reader : m_readerBwd,
                      isForward ? m_cache : m_cacheBwd);
}

geometry::Altitudes const & AltitudeLoader::AltitudeLoader::GetAltitudes(
    uint32_t featureId, size_t pointCount, unique_ptr<FilesContainerR::TReader> & reader,
    map<uint32_t, geometry::Altitudes> & cache) const
{
  if (!HasAltitudes())
  {
    // There's no altitude section in mwm.
    return cache
        .insert(
            make_pair(featureId, geometry::Altitudes(pointCount, geometry::kDefaultAltitudeMeters)))
        .first->second;
  }

  auto const it = cache.find(featureId);
  if (it != cache.end())
    return it->second;

  if (!m_altitudeAvailability[featureId])
  {
    return cache
        .insert(make_pair(featureId, geometry::Altitudes(pointCount, m_header.m_minAltitude)))
        .first->second;
  }

  uint64_t const r = m_altitudeAvailability.rank(featureId);
  CHECK_LESS(r, m_altitudeAvailability.size(), ("Feature Id", featureId, "of", m_countryFileName));
  uint64_t const offset = m_featureTable.select(r);
  CHECK_LESS_OR_EQUAL(offset, m_featureTable.size(), ("Feature Id", featureId, "of", m_countryFileName));

  uint64_t const altitudeInfoOffsetInSection = m_header.m_altitudesOffset + offset;
  CHECK_LESS(altitudeInfoOffsetInSection, reader->Size(), ("Feature Id", featureId, "of", m_countryFileName));

  try
  {
    Altitudes altitudes;
    ReaderSource<FilesContainerR::TReader> src(*reader);
    src.Skip(altitudeInfoOffsetInSection);
    bool const isDeserialized = altitudes.Deserialize(m_header.m_minAltitude, pointCount,
                                                      m_countryFileName, featureId,  src);

    bool const allValid =
        isDeserialized &&
            none_of(altitudes.m_altitudes.begin(), altitudes.m_altitudes.end(),
                    [](geometry::Altitude a) { return a == geometry::kInvalidAltitude; });
    if (!allValid)
    {
      LOG(LERROR, ("Only a part point of a feature has a valid altitude. Altitudes: ", altitudes.m_altitudes,
          ". Feature Id", featureId, "of", m_countryFileName));
      return cache
          .insert(make_pair(featureId, geometry::Altitudes(pointCount, m_header.m_minAltitude)))
          .first->second;
    }

    return cache.insert(make_pair(featureId, move(altitudes.m_altitudes))).first->second;
  }
  catch (Reader::OpenException const & e)
  {
    LOG(LERROR, ("Feature Id", featureId, "of", m_countryFileName, ". Error while getting altitude data:", e.Msg()));
    return cache
        .insert(make_pair(featureId, geometry::Altitudes(pointCount, m_header.m_minAltitude)))
        .first->second;
  }
}
}  // namespace feature
