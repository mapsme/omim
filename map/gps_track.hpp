#pragma once

// NOTE!
// Temporary, to avoid cyclic dependencies between map and drape projects,
// we declared GpsTrackPoint in the drape project.
// Ideally, drape must use declaration GpsTrackPoint, instead of declaring it there.
#include "drape_frontend/gps_track_point.hpp"

#include "map/gps_track_container.hpp"
#include "map/gps_track_file.hpp"

#include "platform/location.hpp"

#include "std/chrono.hpp"
#include "std/mutex.hpp"
#include "std/unique_ptr.hpp"

class GpsTrack
{
public:
  using GpsTrackPoint = df::GpsTrackPoint;
  // See note above
  /*
  struct GpsTrackPoint
  {
    // Timestamp of the point, seconds from 1st Jan 1970
    double m_timestamp;

    // Point in the Mercator projection
    m2::PointD m_point;

    // Speed in the point, M/S
    double m_speedMPS;

    // Unique identifier of the point
    uint32_t m_id;
  };
  */

  GpsTrack(string const & filePath);

  /// Adds point or collection of points to gps tracking
  void AddPoint(location::GpsTrackInfo const & point);
  void AddPoints(vector<location::GpsTrackInfo> const & point);

  /// Clears any previous tracking info
  void Clear();

  /// Sets tracking duration in hours.
  /// @note Callback is called with 'toRemove' points, if some points were removed.
  /// By default, duration is 24h.
  void SetDuration(hours duration);

  /// Returns track duraion in hours
  hours GetDuration() const;

  /// Notification callback about a change of the gps track.
  /// @param toAdd - collection of points to add.
  /// @param toRemove - collection of point indices to remove.
  /// @note Calling of a GpsTrackContainer's function from the callback causes deadlock.
  using TGpsTrackDiffCallback = std::function<void(vector<GpsTrackPoint> && toAdd, vector<uint32_t> && toRemove)>;

  /// Sets callback on change of gps track.
  /// @param callback - callback callable object
  /// @note Only one callback is supported at time.
  /// @note When sink is attached, it receives all points in 'toAdd' at first time,
  /// next time callbacks it receives only modifications. It simplifies getter/callback model.
  void SetCallback(TGpsTrackDiffCallback callback);

private:
  void InitFile();
  void InitContainer();

  string const m_filePath;

  mutable mutex m_guard;

  hours m_duration;

  unique_ptr<GpsTrackFile> m_file;
  unique_ptr<GpsTrackContainer> m_container;
};

GpsTrack & GetDefaultGpsTrack();
