#pragma once

#include "geometry/point2d.hpp"

#include "std/algorithm.hpp"
#include "std/fstream.hpp"
#include "std/function.hpp"
#include "std/string.hpp"

class GpsTrackFile
{
public:
  /// @note Opens or constructs file with track data
  GpsTrackFile(string const & filePath, size_t maxItemCount);

  /// Delete move ctor and operator (will be implemented later)
  GpsTrackFile(GpsTrackFile &&) = delete;
  GpsTrackFile & operator=(GpsTrackFile &&) = delete;

  /// Delete copy ctor and operator (unsupported)
  GpsTrackFile(GpsTrackFile const &) = delete;
  GpsTrackFile & operator=(GpsTrackFile const &) = delete;

  /// Returns true if file is open, otherwise returns false
  bool IsOpen() const;

  /// Flushes all changes and closes file
  void Close();

  /// Appends new point in the file
  /// @note Timestamp must be not less than for the last added point, otherwise function returns false.
  /// @note File is circular, when maxItemCount limit is reached, old points are popped from file.
  bool Append(double timestamp, m2::PointD const & pt, double speed);

  /// Remove all points from the file
  void Clear();

  /// Flushes all changes in file
  void Flush();

  /// Returns true if file does not contain points
  bool IsEmpty() const;

  /// Returns last known timestamp, or zero if there is no points
  double GetTimestamp() const;

  /// Returns number of elements in the file
  size_t GetCount() const;

  /// Enums all points in the file
  /// If fn returns false then enumeration is stopped.
  void ForEach(function<bool(double timestamp, m2::PointD const & pt, double speed)> const & fn);

private:
  struct Item
  {
    double m_timestamp;
    double m_x;
    double m_y;
    double m_speed;
  };

  struct Header
  {
    size_t m_maxItemCount;
    double m_timestamp;
    size_t m_first;
    size_t m_last;

    Header()
      : m_maxItemCount(0), m_timestamp(0), m_first(0), m_last(0)
    {}
  };

  bool Append(Item const & item);

  bool ReadHeader(Header & header);
  void WriteHeader(Header const & header);

  bool ReadItem(size_t index, Item & item);
  void WriteItem(size_t index, Item const & item);

  static size_t ItemOffset(size_t index);

  string const m_filePath;
  size_t const m_maxItemCount;

  fstream m_stream;

  Header m_header;
};
