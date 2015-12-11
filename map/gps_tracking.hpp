#pragma once

#include "map/gps_track.hpp"

#include "std/atomic.hpp"

// TODO
// Rework design

class GpsTracking
{
public:
  /// Returns singletone instance
  static GpsTracking & Instance();

  /// Gets setting 'is gps tracking enabled'.
  /// This flag is used in daemon mode.
  bool IsEnabled() const;
  /// Sets setting 'is gps tracking enabled'
  void SetEnabled(bool enabled);

  /// Gets setting 'duration of gps track'
  hours GetDuration() const;
  /// Sets setting 'duration of gps track'
  void SetDuration(hours duration);

  /// Returns gps track object.
  GpsTrack & GetTrack();

private:
  GpsTracking();

  atomic<bool> m_enabled;
  hours m_duration;
};
