#pragma once

#include "platform/location.hpp"

#include "coding/internal/file_data.hpp"

#include "base/exception.hpp"

#include "std/function.hpp"
#include "std/limits.hpp"
#include "std/string.hpp"
#include "std/unique_ptr.hpp"

class GpsTrackFile final
{
public:
  using TItem = location::GpsTrackInfo;

  GpsTrackFile();
  ~GpsTrackFile();

  /// Opens file with track data.
  /// @param filePath - path to the file on disk
  /// @param maxItemCount - max number of items in recycling file
  /// @exception RootException based exception on error.
  void Open(string const & filePath, size_t maxItemCount);

  /// Creates new file.
  /// @param filePath - path to the file on disk
  /// @param maxItemCount - max number of items in recycling file
  /// @exception RootException based exception on error.
  void Create(string const & filePath, size_t maxItemCount);

  /// Returns true if file is open, otherwise returns false.
  bool IsOpen() const;

  /// Flushes all changes and closes file.
  /// @exception RootException based exception on error.
  void Close();

  /// Flushes all changes in file.
  /// @exception RootException based exception on error.
  void Flush();

  /// Appends new point in the file.
  /// @param items - collection of gps track points.
  /// @exception RootException based exception on error.
  void Append(vector<TItem> const & items);

  /// Removes all data from file.
  /// @exception RootException based exception on error.
  void Clear();

  /// Reads file and calls functor for each item.
  /// @param fn - callable function, return true to stop ForEach
  /// @exception RootException based exception on error.
  void ForEach(std::function<bool(TItem const & item)> const & fn);

private:
  void TruncFile();
  size_t GetFirstItemIndex() const;

  unique_ptr<my::FileData> m_file;
  size_t m_maxItemCount; // max count valid items in file, read note
  size_t m_itemCount; // current number of items in file, read note
};
