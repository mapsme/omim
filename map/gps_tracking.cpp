#include "map/gps_tracking.hpp"

#include "coding/file_name_utils.hpp"

#include "platform/platform.hpp"

#include "std/atomic.hpp"

#include "defines.hpp"

namespace
{

char const kEnabledKey[] = "GpsTrackingEnabled";
char const kDurationHours[] = "GpsTrackingDuration";
uint32_t const kDefaultDurationHours = 24;

size_t const kMaxItemCount = 100000; // > 24h with 1point/s

inline string GetFilePath()
{
  return my::JoinFoldersToPath(GetPlatform().WritableDir(), GPS_TRACK_FILENAME);
}

} // namespace

GpsTracking & GpsTracking::Instance()
{
  static GpsTracking instance;
  return instance;
}

GpsTracking::GpsTracking()
{
  bool enabled = false;
  Settings::Get(kEnabledKey, enabled);

  uint32_t duration = kDefaultDurationHours;
  Settings::Get(kDurationHours, duration);

  m_enabled = enabled;
  m_duration = hours(duration);
}

void GpsTracking::SetEnabled(bool enabled)
{  
  Settings::Set(kEnabledKey, enabled);
  m_enabled = enabled;
}

bool GpsTracking::IsEnabled() const
{
  return m_enabled;
}

void GpsTracking::SetDuration(hours duration)
{
  uint32_t const hours = duration.count();
  Settings::Set(kDurationHours, hours);
  m_duration = duration;
}

hours GpsTracking::GetDuration() const
{
  return m_duration;
}

GpsTrack & GpsTracking::GetTrack()
{
  static GpsTrack instance(GetFilePath(), kMaxItemCount, GetDuration());
  return instance;
}
