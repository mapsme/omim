#include "platform/platform.hpp"
#include "platform/local_country_file.hpp"

#include "coding/base64.hpp"
#include "coding/file_name_utils.hpp"
#include "coding/internal/file_data.hpp"
#include "coding/writer.hpp"

#include "base/logging.hpp"
#include "base/string_utils.hpp"

#include "std/target_os.hpp"
#include <thread>

#include "private.h"

#include <errno.h>

namespace
{
bool IsSpecialDirName(std::string const & dirName)
{
  return dirName == "." || dirName == "..";
}
} // namespace

// static
Platform::EError Platform::ErrnoToError()
{
  switch (errno)
  {
    case ENOENT:
      return ERR_FILE_DOES_NOT_EXIST;
    case EACCES:
      return ERR_ACCESS_FAILED;
    case ENOTEMPTY:
      return ERR_DIRECTORY_NOT_EMPTY;
    case EEXIST:
      return ERR_FILE_ALREADY_EXISTS;
    case ENAMETOOLONG:
      return ERR_NAME_TOO_LONG;
    case ENOTDIR:
      return ERR_NOT_A_DIRECTORY;
    case ELOOP:
      return ERR_SYMLINK_LOOP;
    case EIO:
      return ERR_IO_ERROR;
    default:
      return ERR_UNKNOWN;
  }
}

// static
bool Platform::RmDirRecursively(std::string const & dirName)
{
  if (dirName.empty() || IsSpecialDirName(dirName))
    return false;

  bool res = true;

  FilesList allFiles;
  GetFilesByRegExp(dirName, ".*", allFiles);
  for (std::string const & file : allFiles)
  {
    std::string const path = my::JoinFoldersToPath(dirName, file);

    EFileType type;
    if (GetFileType(path, type) != ERR_OK)
      continue;

    if (type == FILE_TYPE_DIRECTORY)
    {
      if (!IsSpecialDirName(file) && !RmDirRecursively(path))
        res = false;
    }
    else
    {
      if (!my::DeleteFileX(path))
        res = false;
    }
  }

  if (RmDir(dirName) != ERR_OK)
    res = false;

  return res;
}

void Platform::SetSettingsDirForTests(std::string const & path)
{
  m_settingsDir = my::AddSlashIfNeeded(path);
}

std::string Platform::ReadPathForFile(std::string const & file, std::string searchScope) const
{
  if (searchScope.empty())
    searchScope = "wrf";

  std::string fullPath;
  for (size_t i = 0; i < searchScope.size(); ++i)
  {
    switch (searchScope[i])
    {
    case 'w': fullPath = m_writableDir + file; break;
    case 'r': fullPath = m_resourcesDir + file; break;
    case 's': fullPath = m_settingsDir + file; break;
    case 'f': fullPath = file; break;
    default : CHECK(false, ("Unsupported searchScope:", searchScope)); break;
    }
    if (IsFileExistsByFullPath(fullPath))
      return fullPath;
  }

  std::string const possiblePaths = m_writableDir  + "\n" + m_resourcesDir + "\n" + m_settingsDir;
  MYTHROW(FileAbsentException, ("File", file, "doesn't exist in the scope", searchScope,
                                "Have been looking in:\n", possiblePaths));
}

std::string Platform::ResourcesMetaServerUrl() const
{
  return RESOURCES_METASERVER_URL;
}

std::string Platform::MetaServerUrl() const
{
  return METASERVER_URL;
}

std::string Platform::DefaultUrlsJSON() const
{
  return DEFAULT_URLS_JSON;
}

void Platform::GetFontNames(FilesList & res) const
{
  ASSERT(res.empty(), ());

  /// @todo Actually, this list should present once in all our code.
  /// We can take it from data/external_resources.txt
  char const * arrDef[] = {
    "01_dejavusans.ttf",
    "02_droidsans-fallback.ttf",
    "03_jomolhari-id-a3d.ttf",
    "04_padauk.ttf",
    "05_khmeros.ttf",
    "06_code2000.ttf",
    "07_roboto_medium.ttf"
  };
  res.insert(res.end(), arrDef, arrDef + ARRAY_SIZE(arrDef));

  GetSystemFontNames(res);

  LOG(LINFO, ("Available font files:", (res)));
}

void Platform::GetFilesByExt(std::string const & directory, std::string const & ext, FilesList & outFiles)
{
  // Transform extension mask to regexp (.mwm -> \.mwm$)
  ASSERT ( !ext.empty(), () );
  ASSERT_EQUAL ( ext[0], '.' , () );

  GetFilesByRegExp(directory, '\\' + ext + '$', outFiles);
}

// static
void Platform::GetFilesByType(std::string const & directory, unsigned typeMask,
                              TFilesWithType & outFiles)
{
  FilesList allFiles;
  GetFilesByRegExp(directory, ".*", allFiles);
  for (std::string const & file : allFiles)
  {
    EFileType type;
    if (GetFileType(my::JoinFoldersToPath(directory, file), type) != ERR_OK)
      continue;
    if (typeMask & type)
      outFiles.emplace_back(file, type);
  }
}

std::string Platform::DeviceName() const
{
  return OMIM_OS_NAME;
}

void Platform::SetWritableDirForTests(std::string const & path)
{
  m_writableDir = my::AddSlashIfNeeded(path);
}

void Platform::SetResourceDir(std::string const & path)
{
  m_resourcesDir = my::AddSlashIfNeeded(path);
}

unsigned Platform::CpuCores() const
{
  unsigned const cores = std::thread::hardware_concurrency();
  return cores > 0 ? cores : 1;
}

std::string DebugPrint(Platform::EError err)
{
  switch (err)
  {
  case Platform::ERR_OK: return "Ok";
  case Platform::ERR_FILE_DOES_NOT_EXIST: return "File does not exist.";
  case Platform::ERR_ACCESS_FAILED: return "Access failed.";
  case Platform::ERR_DIRECTORY_NOT_EMPTY: return "Directory not empty.";
  case Platform::ERR_FILE_ALREADY_EXISTS: return "File already exists.";
  case Platform::ERR_NAME_TOO_LONG:
    return "The length of a component of path exceeds {NAME_MAX} characters.";
  case Platform::ERR_NOT_A_DIRECTORY:
    return "A component of the path prefix of Path is not a directory.";
  case Platform::ERR_SYMLINK_LOOP:
    return "Too many symbolic links were encountered in translating path.";
  case Platform::ERR_IO_ERROR: return "An I/O error occurred.";
  case Platform::ERR_UNKNOWN: return "Unknown";
  }
}
