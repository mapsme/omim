#pragma once

#include "platform/location.hpp"

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

  using TItem = location::GpsTrackInfo;

  /// @note Opens file with track data or constructs new
  /// @param filePath - path to the file on disk
  /// @param maxItemCount - max number of items in recycling file
  /// @exceptions CreateFileException if unable to create file, ReadFileException if read fails
  /// or WriteFileException if write fails.
  GpsTrackFile(string const & filePath, size_t maxItemCount);

  ~GpsTrackFile();

  /// Returns true if file is open, otherwise returns false
  bool IsOpen() const;

  /// Flushes all changes and closes file
  /// @exceptions WriteFileException if write fails.
  void Close();

  /// Flushes all changes in file
  /// @exceptions WriteFileException if write fails.
  void Flush();

  /// Appends new point in the file
  /// @param item - gps track point.
  /// @param poppedId - identifier of popped point due to recycling, kInvalidId if there is no popped point.
  /// @returns identifier of point, kInvalidId if point was not added.
  /// @note Timestamp must be not less than GetTimestamp(), otherwise function returns false.
  /// @note File is circular, when GetMaxItemCount() limit is reached, old point is popped out from file.
  /// @exceptions WriteFileException if write fails.
  size_t Append(TItem const & item, size_t & poppedId);

  /// Remove all points from the file
  /// @returns range of identifiers of removed points, or pair(kInvalidId,kInvalidId) if nothing was removed.
  /// @exceptions WriteFileException if write fails.
  pair<size_t, size_t> Clear();

  /// Returns max number of points in recycling file
  size_t GetMaxCount() const;

  /// Returns number of items in the file, this values is <= GetMaxItemCount()
  size_t GetCount() const;

  /// Returns true if file does not contain points
  bool IsEmpty() const;

  /// Returns upper bound timestamp, or zero if there is no points
  double GetTimestamp() const;

  /// Enumerates all points from the file in timestamp ascending order
  /// @param fn - callable object which receives points. If fn returns false then enumeration is stopped.
  /// @exceptions CorruptedFileException if file is corrupted or ReadFileException if read fails
  void ForEach(function<bool(TItem const & item, size_t id)> const & fn);

  /// Drops points earlier than specified date
  /// @param timestamp - timestamp of lower bound, number of seconds since 1.1.1970.
  /// @returns range of identifiers of removed points, or pair(kInvalidId,kInvalidId) if nothing was removed.
  /// @exceptions CorruptedFileException if file is corrupted, ReadFileException if read fails
  /// or WriteFileException if write fails.
  pair<size_t, size_t> DropEarlierThan(double timestamp);

private:
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

    explicit Header(size_t maxItemCount = 0)
      : m_maxItemCount(maxItemCount)
      , m_timestamp(0)
      , m_first(0), m_last(0)
      , m_lastId(0)
    {}
  };

  void LazyWriteHeader();

  bool ReadHeader(Header & header);
  void WriteHeader(Header const & header);

  bool ReadItem(size_t index, TItem & item);
  void WriteItem(size_t index, TItem const & item);

  static size_t ItemOffset(size_t index);

  size_t Distance(size_t first, size_t last) const;

  string const m_filePath;
  size_t const m_maxItemCount;

  Header m_header;
  uint32_t m_lazyWriteHeaderCount; // request count for write header

  fstream m_stream; // buffered file stream
};
