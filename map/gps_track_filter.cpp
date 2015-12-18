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

// Periodicy to check points in wifi afea, sec
size_t const kWifiAreaGpsPeriodicyCheckSec = 30;

// Max acceptable moving speed in wifi area, required to prevent jumping between wifi, m/s
double const kWifiAreaAcceptableMovingSpeedMps = 15; // pedestrian, bicycle

bool IsRealGpsPoint(location::GpsInfo const & info)
{
  // TODO use flag from device about gps type (real gps/wifi/cellular)
  // we guess real gps says speed and bearing other than zero
  return info.m_speed > 0 && info.m_bearing > 0;
}

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
{
  Settings::Get(kMinHorizontalAccuracyKey, m_minAccuracy);
}

void GpsTrackFilter::Process(vector<location::GpsInfo> const & inPoints,
                             vector<location::GpsTrackInfo> & outPoints)
{
  outPoints.reserve(inPoints.size());

  for (location::GpsInfo const & currInfo : inPoints)
  {
    if (!m_hasLastInfo || IsGoodPoint(currInfo))
    {
      m_hasLastInfo = true;
      m_lastInfo = currInfo;
      outPoints.emplace_back(currInfo);
    }
  }
}

bool GpsTrackFilter::IsGoodPoint(location::GpsInfo const & currInfo) const
{
  // Do not accept points from the predictor
  if (currInfo.m_source == location::EPredictor)
    return false;

  // Distance in meters between last and current point is, meters:
  double const distance = ms::DistanceOnEarth(m_lastInfo.m_latitude, m_lastInfo.m_longitude,
                                              currInfo.m_latitude, currInfo.m_longitude);

  // Filter point by close distance
  if (distance < kClosePointDistanceMeters)
    return false;

  // Filter point if accuracy areas are intersected
  if (distance < m_lastInfo.m_horizontalAccuracy && distance < currInfo.m_horizontalAccuracy)
    return false;

  // Filter by point accuracy
  if (currInfo.m_horizontalAccuracy > m_minAccuracy)
    return false;

  bool const lastRealGps = IsRealGpsPoint(m_lastInfo);
  bool const currRealGps = IsRealGpsPoint(currInfo);

  bool const gpsToWifi = lastRealGps && !currRealGps;
  bool const wifiToWifi = !lastRealGps && !currRealGps;

  if (gpsToWifi || wifiToWifi)
  {
    if (currInfo.m_timestamp < m_lastInfo.m_timestamp)
    {
      // Switched to wifi without time, phone time is used,
      // phone time may be less than last known gps time. in this case
      // accept point.
      return true;
    }

    double const elapsedTime = currInfo.m_timestamp - m_lastInfo.m_timestamp;

    // Wait before switch gps to wifi or switch between wifi points
    if (elapsedTime < kWifiAreaGpsPeriodicyCheckSec)
      return false;

    // Skip point if moving to it was too fast, we guess it was a jump from wifi to another wifi
    double const speed = distance / elapsedTime;
    if (speed > kWifiAreaAcceptableMovingSpeedMps)
      return false;
  }

  return true;
}
