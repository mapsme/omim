#pragma once

#include <initializer_list>
#include <string>

#include <utility>

namespace my
{
  /// Remove extension from file name.
  void GetNameWithoutExt(std::string & name);
  std::string FilenameWithoutExt(std::string name);
  /// @return File extension with the dot or empty string if no extension found.
  std::string GetFileExtension(std::string const & name);

  /// Get file name from full path.
  void GetNameFromFullPath(std::string & name);

  /// Returns all but last components of the path. After dropping the last
  /// component, all trailing slashes are removed, unless the result is a
  /// root directory. If the argument is a single component, returns ".".
  std::string GetDirectory(std::string const & path);

  /// Get folder separator for specific platform
  std::string GetNativeSeparator();

  /// @deprecated use JoinPath instead.
  std::string JoinFoldersToPath(const std::string & folder, const std::string & file);
  std::string JoinFoldersToPath(std::initializer_list<std::string> const & folders, const std::string & file);

  /// Add the terminating slash to the folder path string if it's not already there.
  std::string AddSlashIfNeeded(std::string const & path);

  inline std::string JoinPath(std::string const & file) { return file; }

  /// Create full path from some folder using native folders separator.
  template<typename... Args>
  std::string JoinPath(std::string const & folder, Args&&... args)
  {
    return AddSlashIfNeeded(folder) + JoinPath(std::forward<Args>(args)...);
  }
}
