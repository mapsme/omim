#pragma once

#include "geometry/point2d.hpp"

#include "base/exception.hpp"

#include "std/fstream.hpp"
#include "std/function.hpp"
#include "std/limits.hpp"
#include "std/string.hpp"

class GpsTrackFile final
{
public:
  DECLARE_EXCEPTION(CreateFileException, RootException);
  DECLARE_EXCEPTION(WriteFileException, RootException);
  DECLARE_EXCEPTION(ReadFileException, RootException);
  DECLARE_EXCEPTION(CorruptedFileException, RootException);

  /// Invalid identifier for point
  static size_t const kInvalidId; // = numeric_limits<size_t>::max();

  /// @note Opens file with track data or constructs new
  /// @param filePath - path to the file on disk
  /// @param maxItemCount - max number of items in recycling file
  GpsTrackFile(string const & filePath, size_t maxItemCount);

  /// Delete move ctor and operator (will be implemented later)
  GpsTrackFile(GpsTrackFile &&) = delete;
  GpsTrackFile & operator=(GpsTrackFile &&) = delete;

  /// Delete copy ctor and operator (unsupported by nature)
  GpsTrackFile(GpsTrackFile const &) = delete;
  GpsTrackFile & operator=(GpsTrackFile const &) = delete;

  /// Returns true if file is open, otherwise returns false
  bool IsOpen() const;

  /// Flushes all changes and closes file
  void Close();

  /// Flushes all changes in file
  void Flush();

  /// Appends new point in the file
  /// @param pt - point in the Mercator projection.
  /// @param speedMPS - current speed in the new point in M/S.
  /// @param timestamp - timestamp of the point, number of seconds since 1.1.1970.
  /// @param poppedId - identifier of popped point due to recycling, kInvalidId if there is no popped point.
  /// @returns identifier of point, kInvalidId if point was not added.
  /// @note Timestamp must be not less than GetTimestamp(), otherwise function returns false.
  /// @note File is circular, when GetMaxItemCount() limit is reached, old point is popped out from file.
  size_t Append(double timestamp, m2::PointD const & pt, double speedMPS, size_t & poppedId);

  /// Remove all points from the file
  /// @returns range of identifiers of removed points, or pair(kInvalidId,kInvalidId) if nothing was removed.
  pair<size_t, size_t> Clear();

  /// Returns max number of points in recycling file
  size_t GetMaxItemCount() const;

  /// Returns number of items in the file, this values is <= GetMaxItemCount()
  size_t GetCount() const;

  /// Returns true if file does not contain points
  bool IsEmpty() const;

  /// Returns upper bound timestamp, or zero if there is no points
  double GetTimestamp() const;

  /// Enumerates all points from the file in timestamp ascending order
  /// If fn returns false then enumeration is stopped.
  void ForEach(function<bool(double timestamp, m2::PointD const & pt, double speedMPS, size_t id)> const & fn);

  /// Drops points earlier than specified date
  /// @param timestamp - timestamp of lower bound, number of seconds since 1.1.1970.
  /// @returns range of identifiers of removed points, or pair(kInvalidId,kInvalidId) if nothing was removed.
  pair<size_t, size_t> DropEarlierThan(double timestamp);

private:
  /// Item, information for point, stored in recycling file.
  struct Item
  {
    double m_timestamp;
    double m_speed;
    double m_x;
    double m_y;
  };

  /// Header, stored in beginning of file
  /// @note Due to recycling, indexes of items are reused, but idendifiers are unique
  /// until Clear or create new file
  struct Header
  {
    size_t m_maxItemCount; // max number of items in recycling file
    double m_timestamp; // upper bound timestamp
    size_t m_first; // index of first item
    size_t m_last; // index of last item, items are in range [first,last)
    size_t m_lastId; // identifier of the last item

    Header()
      : m_maxItemCount(0), m_timestamp(0), m_first(0), m_last(0), m_lastId(0)
    {}
  };

  size_t Append(Item const & item, size_t & poppedId);

  bool ReadHeader(Header & header);
  void WriteHeader(Header const & header);

  bool ReadItem(size_t index, Item & item);
  void WriteItem(size_t index, Item const & item);

  static size_t ItemOffset(size_t index);

  size_t Distance(size_t first, size_t last) const;

  string const m_filePath;
  size_t const m_maxItemCount;

  fstream m_stream; // buffered file stream

  Header m_header;
};
