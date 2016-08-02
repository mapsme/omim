#include "indexer/feature_altitude.hpp"

#include "base/bits.hpp"

namespace
{
/// \returns y = k * x + b. It's the expected altitude in meters.
double GetY(double k, double b, double x) { return k * x + b; }

/// \returns k factor for a line which goes through two points.
double GetK(double y1, double y2, double deltaX) { return (y2 - y1) / deltaX; }
}  // namespace

namespace feature
{
void Altitudes::PrepareSerializationData(vector<double> const & distFromBeginningM,
                                         TAltitude minAltitude, vector<uint32_t> & deviations) const
{
  CHECK_LESS(1, m_altitudes.size(), ());
  CHECK_EQUAL(m_altitudes.size(), distFromBeginningM.size(), ());

  deviations.clear();

  size_t const pointCount = m_altitudes.size();
  TAltitude const startPntAltitude = m_altitudes[0];
  TAltitude const endPntAltitude = m_altitudes[pointCount - 1];

  uint32_t const startPntDeviation =
      bits::ZigZagEncode(static_cast<int32_t>(startPntAltitude) -
      static_cast<int32_t>(minAltitude)) + 1 /* making it more than zero */;

  CHECK_LESS(0, startPntDeviation, ());
  deviations.push_back(startPntDeviation);

  double const k = GetK(startPntAltitude, endPntAltitude, distFromBeginningM.back());
  for (size_t i = 1; i + 1 < pointCount; ++i)
  {
    uint32_t const nextPntDeviation = bits::ZigZagEncode(static_cast<int32_t>(m_altitudes[i]) -
        static_cast<int32_t>(GetY(k, startPntAltitude, distFromBeginningM[i]))) + 1 /* making it more than zero */;
    CHECK_LESS(0, nextPntDeviation, (i));
    deviations.push_back(nextPntDeviation);
  }

  uint32_t const endPntDeviation =
      bits::ZigZagEncode(static_cast<int32_t>(endPntAltitude) -
      static_cast<int32_t>(startPntAltitude)) + 1 /* making it more than zero */;

  CHECK_LESS(0, endPntDeviation, ());
  deviations.push_back(endPntDeviation);
}

bool Altitudes::FillAltitudesByDeserializedDate(vector<double> const & distFromBeginningM,
                                                TAltitude minAltitude, vector<uint32_t> const & deviations)
{
  m_altitudes.clear();
  if (deviations.size() < 2 || distFromBeginningM.size() != deviations.size())
  {
    ASSERT(false, ());
    return false;
  }

  size_t const pointCount = deviations.size();
  if (deviations[0] == 0 || deviations[pointCount - 1] == 0)
  {
    ASSERT(false, (deviations[0], deviations[pointCount - 1]));
    return false;
  }

  TAltitude const startPntAltitude = static_cast<TAltitude>(
        minAltitude + bits::ZigZagDecode(deviations[0] - 1 /* Recovering value */));
  TAltitude const endPntAltitude = static_cast<TAltitude>(
        startPntAltitude + bits::ZigZagDecode(deviations[pointCount - 1] - 1 /* Recovering value */));
  if (startPntAltitude < minAltitude || endPntAltitude < minAltitude)
  {
    ASSERT(false, (startPntAltitude, endPntAltitude));
    return false;
  }

  m_altitudes.resize(pointCount);
  m_altitudes[0] = startPntAltitude;
  m_altitudes[pointCount - 1] = endPntAltitude;

  double const k = GetK(startPntAltitude, endPntAltitude, distFromBeginningM.back());
  for (size_t i = 1; i + 1 < deviations.size(); ++i)
  {
    if (deviations[i] == 0)
    {
      ASSERT(false, (i));
      m_altitudes.clear();
      return false;
    }

    TAltitude const aproxAltitude = static_cast<TAltitude>(GetY(k, startPntAltitude, distFromBeginningM[i]));
    m_altitudes[i] =
          static_cast<TAltitude>(aproxAltitude + bits::ZigZagDecode(deviations[i] - 1 /* Recovering value */));

    if (m_altitudes[i] < minAltitude)
    {
      ASSERT(false, (i));
      m_altitudes.clear();
      return false;
    }
  }
  return true;
}
}  // namespace feature
