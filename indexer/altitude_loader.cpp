#include "indexer/altitude_loader.hpp"

#include "coding/reader.hpp"
#include "coding/succinct_mapper.hpp"

#include "base/logging.hpp"
#include "base/stl_helpers.hpp"
#include "base/thread.hpp"

#include "defines.hpp"

#include "std/algorithm.hpp"

#include "3party/succinct/mapper.hpp"

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
AltitudeLoader::AltitudeLoader(MwmValue const & mwmValue)
{
  if (mwmValue.GetHeader().GetFormat() < version::Format::v8)
    return;

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
  }
  catch (Reader::OpenException const & e)
  {
    m_header.Reset();
    LOG(LERROR, ("Error while reading", ALTITUDES_FILE_TAG, "section.", e.Msg()));
  }
}

bool AltitudeLoader::HasAltitudes() const
{
  return m_reader != nullptr && m_header.m_minAltitude != kInvalidAltitude;
}

TAltitudes const & AltitudeLoader::GetAltitudes(uint32_t featureId, vector<double> const & distFromBeginningM)
{
  size_t const pointCount = distFromBeginningM.size();
  if (!HasAltitudes())
  {
    // The version of mwm is less than version::Format::v8 or there's no altitude section in mwm.
    return m_cache.insert(make_pair(featureId, TAltitudes(pointCount, kDefaultAltitudeMeters))).first->second;
  }

  auto const it = m_cache.find(featureId);
  if (it != m_cache.end())
    return it->second;

  if (!m_altitudeAvailability[featureId])
  {
    LOG(LINFO, ("Feature featureId =", featureId, "does not contain any altitude information."));
    return m_cache.insert(make_pair(featureId, TAltitudes(pointCount, m_header.m_minAltitude))).first->second;
  }

  uint64_t const r = m_altitudeAvailability.rank(featureId);
  CHECK_LESS(r, m_altitudeAvailability.size(), (featureId));
  uint64_t const offset = m_featureTable.select(r);
  CHECK_LESS_OR_EQUAL(offset, m_featureTable.size(), (featureId));

  uint64_t const altitudeInfoOffsetInSection = m_header.m_altitudesOffset + offset;
  CHECK_LESS(altitudeInfoOffsetInSection, m_reader->Size(), ());

  try
  {
    Altitudes altitudes;
    ReaderSource<FilesContainerR::TReader> src(*m_reader);
    src.Skip(altitudeInfoOffsetInSection);
    bool const isDeserialized = altitudes.Deserialize(distFromBeginningM, m_header.m_minAltitude, pointCount, src);

    bool const allValid = isDeserialized
        && none_of(altitudes.m_altitudes.begin(), altitudes.m_altitudes.end(),
                   [](TAltitude a) { return a == kInvalidAltitude; });
    if (!allValid)
    {
      ASSERT(false, (altitudes.m_altitudes));
      return m_cache.insert(make_pair(featureId, TAltitudes(pointCount, m_header.m_minAltitude))).first->second;
    }

    return m_cache.insert(make_pair(featureId, move(altitudes.m_altitudes))).first->second;
  }
  catch (Reader::OpenException const & e)
  {
    LOG(LERROR, ("Error while getting altitude data", e.Msg()));
    return m_cache.insert(make_pair(featureId, TAltitudes(pointCount, m_header.m_minAltitude))).first->second;
  }
}

void FillDistFormBeginning(FeatureType const & f, vector<double> & distFromBeginning)
{
  size_t const pointCount = f.GetPointsCount();
  if (pointCount == 0)
  {
    ASSERT(false, ());
    return;
  }

  distFromBeginning.resize(pointCount);
  distFromBeginning[0] = 0.0;  /* Distance from start to start in meters. */
  for (size_t i = 1; i < pointCount; ++i)
  {
    distFromBeginning[i] = distFromBeginning[i - 1] +
        MercatorBounds::DistanceOnEarth(f.GetPoint(i - 1), f.GetPoint(i));
  }
}
}  // namespace feature
