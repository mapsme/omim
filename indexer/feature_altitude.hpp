#pragma once

#include "coding/bit_streams.hpp"
#include "coding/elias_coder.hpp"
#include "coding/reader.hpp"
#include "coding/varint.hpp"
#include "coding/write_to_sink.hpp"

#include "base/assert.hpp"
#include "base/bits.hpp"

#include <cstdint>
#include <limits>
#include <vector>

namespace feature
{
using TAltitude = int16_t;
using TAltitudes = std::vector<feature::TAltitude>;

TAltitude constexpr kInvalidAltitude = std::numeric_limits<TAltitude>::min();
feature::TAltitude constexpr kDefaultAltitudeMeters = 0;

struct AltitudeHeader
{
  using TAltitudeSectionVersion = uint16_t;

  AltitudeHeader() { Reset(); }

  template <class TSink>
  void Serialize(TSink & sink) const
  {
    WriteToSink(sink, m_version);
    WriteToSink(sink, m_minAltitude);
    WriteToSink(sink, m_featureTableOffset);
    WriteToSink(sink, m_altitudesOffset);
    WriteToSink(sink, m_endOffset);
  }

  template <class TSource>
  void Deserialize(TSource & src)
  {
    m_version = ReadPrimitiveFromSource<TAltitudeSectionVersion>(src);
    m_minAltitude = ReadPrimitiveFromSource<TAltitude>(src);
    m_featureTableOffset = ReadPrimitiveFromSource<uint32_t>(src);
    m_altitudesOffset = ReadPrimitiveFromSource<uint32_t>(src);
    m_endOffset = ReadPrimitiveFromSource<uint32_t>(src);
  }

  // Methods below return sizes of parts of altitude section in bytes.
  size_t GetAltitudeAvailabilitySize() const
  {
    return m_featureTableOffset - sizeof(AltitudeHeader);
  }

  size_t GetFeatureTableSize() const { return m_altitudesOffset - m_featureTableOffset; }

  size_t GetAltitudeInfoSize() const { return m_endOffset - m_altitudesOffset; }

  void Reset()
  {
    m_version = 0;
    m_minAltitude = kDefaultAltitudeMeters;
    m_featureTableOffset = 0;
    m_altitudesOffset = 0;
    m_endOffset = 0;
  }

  TAltitudeSectionVersion m_version;
  TAltitude m_minAltitude;
  uint32_t m_featureTableOffset;
  uint32_t m_altitudesOffset;
  uint32_t m_endOffset;
};

static_assert(sizeof(AltitudeHeader) == 16, "Wrong header size of altitude section.");

class Altitudes
{
public:
  Altitudes() = default;

  explicit Altitudes(TAltitudes const & altitudes) : m_altitudes(altitudes) {}

  template <class TSink>
  void Serialize(TAltitude minAltitude, TSink & sink) const
  {
    CHECK(!m_altitudes.empty(), ());

    BitWriter<TSink> bits(sink);
    TAltitude prevAltitude = minAltitude;
    for (auto const altitude : m_altitudes)
    {
      CHECK_LESS_OR_EQUAL(minAltitude, altitude, ("A point altitude is less than min mwm altitude"));
      uint32_t const delta = bits::ZigZagEncode(static_cast<int32_t>(altitude) -
                                                static_cast<int32_t>(prevAltitude));
      coding::DeltaCoder::Encode(bits, delta + 1 /* making it greater than zero */);
      prevAltitude = altitude;
    }
  }

  template <class TSource>
  bool Deserialize(TAltitude minAltitude, size_t pointCount, string const & countryFileName,
                   uint32_t featureId, TSource & src)
  {
    ASSERT_NOT_EQUAL(pointCount, 0, ());

    BitReader<TSource> bits(src);
    TAltitude prevAltitude = minAltitude;
    m_altitudes.resize(pointCount);

    for (size_t i = 0; i < pointCount; ++i)
    {
      uint64_t const biasedDelta = coding::DeltaCoder::Decode(bits);
      if (biasedDelta == 0)
      {
        LOG(LERROR, ("Decoded altitude delta is zero. File", countryFileName,
                     ". Feature Id", featureId, ". Point number in the feature", i, "."));
        m_altitudes.clear();
        return false;
      }
      uint64_t const delta = biasedDelta - 1;

      m_altitudes[i] = static_cast<TAltitude>(bits::ZigZagDecode(delta) + prevAltitude);
      if (m_altitudes[i] < minAltitude)
      {
        LOG(LERROR, ("A point altitude read from file(", m_altitudes[i],
                     ") is less than min mwm altitude(", minAltitude, "). File ",
                     countryFileName, ". Feature Id", featureId, ". Point number in the feature", i, "."));
        m_altitudes.clear();
        return false;
      }
      prevAltitude = m_altitudes[i];
    }
    return true;
  }

  /// \note |m_altitudes| is a vector of feature point altitudes. There's two possibilities:
  /// * |m_altitudes| is empty. It means there is no altitude information for this feature.
  /// * size of |m_pointAlt| is equal to the number of this feature's points. If so
  ///   all items of |m_altitudes| have valid value.
  TAltitudes m_altitudes;
};
}  // namespace feature
