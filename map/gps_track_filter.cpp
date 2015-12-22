#include "map/gps_track_filter.hpp"

#include "geometry/distance_on_sphere.hpp"

#include "platform/settings.hpp"

namespace
{

char const kMinHorizontalAccuracyKey[] = "GpsTrackingMinAccuracy";

// Minimal horizontal accuracy is required to skip 'bad' points.
// Use 250 meters to allow points from a pure GPS + GPS through wifi.
double constexpr kMinHorizontalAccuracyMeters = 250;

// Required for points decimation to reduce number of close points.
double constexpr kClosePointDistanceMeters = 15;

// Max acceptable acceleration to filter gps jumps
double const kMaxAcceptableAcceleration = 2; // m / sec ^ 2

} // namespace

void GpsTrackNullFilter::Process(vector<location::GpsInfo> const & inPoints,
                                 vector<location::GpsTrackInfo> & outPoints)
{
  outPoints.insert(outPoints.end(), inPoints.begin(), inPoints.end());
}

void GpsTrackFilter::StoreMinHorizontalAccuracy(double value)
{
  Settings::Set(kMinHorizontalAccuracyKey, value);
}

GpsTrackFilter::GpsTrackFilter()
  : m_minAccuracy(kMinHorizontalAccuracyMeters)
  , m_hasLastInfo(false)
  , m_lastSpeed(0.0)
{
  Settings::Get(kMinHorizontalAccuracyKey, m_minAccuracy);
}

void GpsTrackFilter::Process(vector<location::GpsInfo> const & inPoints,
                             vector<location::GpsTrackInfo> & outPoints)
{
  outPoints.reserve(inPoints.size());

  for (location::GpsInfo const & currInfo : inPoints)
  {
    // Do not accept points from the predictor
    if (currInfo.m_source == location::EPredictor)
      continue;

    // Filter by point accuracy
    if (currInfo.m_horizontalAccuracy > m_minAccuracy)
      continue;

    // Skip any function without speed
    if (!currInfo.HasSpeed())
      continue;

    if (!m_hasLastInfo || currInfo.m_timestamp < m_lastAcceptedInfo.m_timestamp)
    {
      m_hasLastInfo = true;
      m_lastSpeed = (currInfo.HasSpeed()) ? currInfo.m_speed : m_lastSpeed;
      m_lastInfo = currInfo;
      m_lastAcceptedInfo = currInfo;
      continue;
    }

    // Distance in meters between last accepted and current point is, meters:
    double const distanceFromLastAccepted = ms::DistanceOnEarth(m_lastAcceptedInfo.m_latitude, m_lastAcceptedInfo.m_longitude,
                                                                currInfo.m_latitude, currInfo.m_longitude);

    // Filter point by close distance
    if (distanceFromLastAccepted < kClosePointDistanceMeters)
    {
      m_lastInfo = currInfo;
      m_lastSpeed = (currInfo.HasSpeed()) ? currInfo.m_speed : m_lastSpeed;
      continue;
    }

    // Filter point if accuracy areas are intersected
    if (distanceFromLastAccepted < m_lastAcceptedInfo.m_horizontalAccuracy &&
        currInfo.m_horizontalAccuracy > 0.5 * m_lastAcceptedInfo.m_horizontalAccuracy)
    {
      m_lastInfo = currInfo;
      m_lastSpeed = (currInfo.HasSpeed()) ? currInfo.m_speed : m_lastSpeed;
      continue;
    }

    // Distance in meters between last and current point is, meters:
    double const distanceFromLast = ms::DistanceOnEarth(m_lastInfo.m_latitude, m_lastInfo.m_longitude,
                                                        currInfo.m_latitude, currInfo.m_longitude);

    // Time spend to move from the last accepted point to the current point, sec:
    double const timeFromLastAccepted = currInfo.m_timestamp - m_lastAcceptedInfo.m_timestamp;
    if (timeFromLastAccepted == 0.0)
    {
      m_lastInfo = currInfo;
      m_lastSpeed = (currInfo.HasSpeed()) ? currInfo.m_speed : m_lastSpeed;
      continue;
    }

    // Time spend to move from the last point to the current point, sec:
    double const timeFromLast = currInfo.m_timestamp - m_lastInfo.m_timestamp;
    if (timeFromLast == 0.0)
    {
      m_lastInfo = currInfo;
      m_lastSpeed = (currInfo.HasSpeed()) ? currInfo.m_speed : m_lastSpeed;
      continue;
    }

    // Speed to move from the last point to the current point
    double const speedFromLast = distanceFromLast / timeFromLast;

    // Speed to move from the last accepted point to the current point
    double const speedFromLastAccepted = distanceFromLastAccepted / timeFromLastAccepted;

    // Filter by acceleration: skip point it jumps too far in short time
    double const accelerationFromLast = (speedFromLast - m_lastSpeed) / timeFromLast;
    if (accelerationFromLast > kMaxAcceptableAcceleration)
    {
      m_lastInfo = currInfo;
      m_lastSpeed = (currInfo.HasSpeed()) ? currInfo.m_speed : m_lastSpeed;
      continue;
    }

    m_lastSpeed = currInfo.HasSpeed() ? currInfo.m_speed : speedFromLastAccepted;
    m_lastAcceptedInfo = currInfo;
    m_lastInfo = currInfo;
    outPoints.emplace_back(currInfo);
  }
}
