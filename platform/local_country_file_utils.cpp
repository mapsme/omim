#include "platform/local_country_file_utils.hpp"

#include "platform/country_file.hpp"
#include "platform/mwm_version.hpp"
#include "platform/platform.hpp"
#include "platform/settings.hpp"

#include "coding/file_name_utils.hpp"
#include "coding/internal/file_data.hpp"
#include "coding/reader.hpp"

#include "base/assert.hpp"
#include "base/string_utils.hpp"
#include "base/logging.hpp"

#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>
#include <memory>
#include <unordered_set>

#include "defines.hpp"

namespace platform
{

namespace migrate
{
// Set of functions to support migration between different versions of MWM
// with totaly incompatible formats.
// 160302 - Migrate to small single file MWM
uint32_t constexpr kMinRequiredVersion = 160302;
bool NeedMigrate()
{
  uint32_t version;
  if (!settings::Get("LastMigration", version))
    return true;

  if (version >= kMinRequiredVersion)
    return false;

  return true;
}

void SetMigrationFlag()
{
  settings::Set("LastMigration", kMinRequiredVersion);
}
}  // namespace migrate

namespace
{
char const kBitsExt[] = ".bftsegbits";
char const kNodesExt[] = ".bftsegnodes";
char const kOffsetsExt[] = ".offsets";

size_t const kMaxTimestampLength = 18;

bool GetFileTypeChecked(std::string const & path, Platform::EFileType & type)
{
  Platform::EError const ret = Platform::GetFileType(path, type);
  if (ret != Platform::ERR_OK)
  {
    LOG(LERROR, ("Can't determine file type for", path, ":", ret));
    return false;
  }
  return true;
}

bool MkDirChecked(std::string const & directory)
{
  Platform & platform = GetPlatform();
  Platform::EError const ret = platform.MkDir(directory);
  switch (ret)
  {
    case Platform::ERR_OK:
      return true;
    case Platform::ERR_FILE_ALREADY_EXISTS:
    {
      Platform::EFileType type;
      if (!GetFileTypeChecked(directory, type))
        return false;
      if (type != Platform::FILE_TYPE_DIRECTORY)
      {
        LOG(LERROR, (directory, "exists, but not a directory:", type));
        return false;
      }
      return true;
    }
    default:
      LOG(LERROR, (directory, "can't be created:", ret));
      return false;
  }
}

std::string GetSpecialFilesSearchScope()
{
#if defined(OMIM_OS_ANDROID)
  return "er";
#else
  return "r";
#endif  // defined(OMIM_OS_ANDROID)
}

bool IsSpecialName(std::string const & name) { return name == "." || name == ".."; }

bool IsDownloaderFile(std::string const & name)
{
  static std::regex const filter(".*\\.(downloading|resume|ready)[0-9]?$");
  return std::regex_match(name.begin(), name.end(), filter);
}

bool DirectoryHasIndexesOnly(std::string const & directory)
{
  Platform::TFilesWithType fwts;
  Platform::GetFilesByType(directory, Platform::FILE_TYPE_REGULAR | Platform::FILE_TYPE_DIRECTORY,
                           fwts);
  for (auto const & fwt : fwts)
  {
    auto const & name = fwt.first;
    auto const & type = fwt.second;
    if (type == Platform::FILE_TYPE_DIRECTORY)
    {
      if (!IsSpecialName(name))
        return false;
      continue;
    }
    if (!CountryIndexes::IsIndexFile(name))
      return false;
  }
  return true;
}

inline std::string GetDataDirFullPath(std::string const & dataDir)
{
  Platform & platform = GetPlatform();
  return dataDir.empty() ? platform.WritableDir()
                         : my::JoinFoldersToPath(platform.WritableDir(), dataDir);
}
}  // namespace

void DeleteDownloaderFilesForCountry(int64_t version, CountryFile const & countryFile)
{
  DeleteDownloaderFilesForCountry(version, std::string(), countryFile);
}

void DeleteDownloaderFilesForCountry(int64_t version, std::string const & dataDir,
                                     CountryFile const & countryFile)
{
  for (MapOptions file : {MapOptions::Map, MapOptions::CarRouting})
  {
    std::string const path = GetFileDownloadPath(version, dataDir, countryFile, file);
    ASSERT(strings::EndsWith(path, READY_FILE_EXTENSION), ());
    my::DeleteFileX(path);
    my::DeleteFileX(path + RESUME_FILE_EXTENSION);
    my::DeleteFileX(path + DOWNLOADING_FILE_EXTENSION);
  }
}

void FindAllLocalMapsInDirectoryAndCleanup(std::string const & directory, int64_t version,
                                           int64_t latestVersion,
                                           std::vector<LocalCountryFile> & localFiles)
{
  std::vector<std::string> files;
  Platform & platform = GetPlatform();

  Platform::TFilesWithType fwts;
  platform.GetFilesByType(directory, Platform::FILE_TYPE_REGULAR | Platform::FILE_TYPE_DIRECTORY,
                          fwts);

  std::unordered_set<std::string> names;
  for (auto const & fwt : fwts)
  {
    if (fwt.second != Platform::FILE_TYPE_REGULAR)
      continue;

    std::string name = fwt.first;

    // Remove downloader files for old version directories.
    if (IsDownloaderFile(name) && version < latestVersion)
    {
      my::DeleteFileX(my::JoinFoldersToPath(directory, name));
      continue;
    }

    if (!strings::EndsWith(name, DATA_FILE_EXTENSION))
      continue;

    // Remove DATA_FILE_EXTENSION and use base name as a country file name.
    my::GetNameWithoutExt(name);
    names.insert(name);
    LocalCountryFile localFile(directory, CountryFile(name), version);

    // Delete Brazil.mwm and Japan.mwm maps, because they were
    // replaced with smaller regions after osrm routing
    // implementation.
    if (name == "Japan" || name == "Brazil")
    {
      localFile.SyncWithDisk();
      localFile.DeleteFromDisk(MapOptions::MapWithCarRouting);
      continue;
    }

    localFiles.push_back(localFile);
  }

  for (auto const & fwt : fwts)
  {
    if (fwt.second != Platform::FILE_TYPE_DIRECTORY)
      continue;

    std::string name = fwt.first;
    if (IsSpecialName(name))
      continue;

    if (names.count(name) == 0 && DirectoryHasIndexesOnly(my::JoinFoldersToPath(directory, name)))
    {
      // Directory which looks like a directory with indexes for absent country. It's OK to remove
      // it.
      LocalCountryFile absentCountry(directory, CountryFile(name), version);
      CountryIndexes::DeleteFromDisk(absentCountry);
    }
  }
}

void FindAllLocalMapsAndCleanup(int64_t latestVersion, std::vector<LocalCountryFile> & localFiles)
{
  FindAllLocalMapsAndCleanup(latestVersion, std::string(), localFiles);
}

void FindAllLocalMapsAndCleanup(int64_t latestVersion, std::string const & dataDir,
                                std::vector<LocalCountryFile> & localFiles)
{
  std::string const dir = GetDataDirFullPath(dataDir);
  FindAllLocalMapsInDirectoryAndCleanup(dir, 0 /* version */, latestVersion, localFiles);

  Platform::TFilesWithType fwts;
  Platform::GetFilesByType(dir, Platform::FILE_TYPE_DIRECTORY, fwts);
  for (auto const & fwt : fwts)
  {
    std::string const & subdir = fwt.first;
    int64_t version;
    if (!ParseVersion(subdir, version) || version > latestVersion)
      continue;

    std::string const fullPath = my::JoinFoldersToPath(dir, subdir);
    FindAllLocalMapsInDirectoryAndCleanup(fullPath, version, latestVersion, localFiles);
    Platform::EError err = Platform::RmDir(fullPath);
    if (err != Platform::ERR_OK && err != Platform::ERR_DIRECTORY_NOT_EMPTY)
      LOG(LWARNING, ("Can't remove directory:", fullPath, err));
  }

  // World and WorldCoasts can be stored in app bundle or in resources
  // directory, thus it's better to get them via Platform.
  for (std::string const & file : { WORLD_FILE_NAME,
    (migrate::NeedMigrate() ? WORLD_COASTS_OBSOLETE_FILE_NAME : WORLD_COASTS_FILE_NAME) })
  {
    auto i = localFiles.begin();
    for (; i != localFiles.end(); ++i)
    {
      if (i->GetCountryFile().GetName() == file)
        break;
    }

    try
    {
      Platform & platform = GetPlatform();
      ModelReaderPtr reader(
          platform.GetReader(file + DATA_FILE_EXTENSION, GetSpecialFilesSearchScope()));

      // Assume that empty path means the resource file.
      LocalCountryFile worldFile{string(), CountryFile(file),
                                 version::ReadVersionDate(reader)};
      worldFile.m_files = MapOptions::Map;
      if (i != localFiles.end())
      {
        // Always use resource World files instead of local on disk.
        *i = worldFile;
      }
      else
        localFiles.push_back(worldFile);
    }
    catch (RootException const & ex)
    {
      if (i == localFiles.end())
      {
        // This warning is possible on android devices without pre-downloaded Worlds/fonts files.
        LOG(LWARNING, ("Can't std::find any:", file, "Reason:", ex.Msg()));
      }
    }
  }
}

void CleanupMapsDirectory(int64_t latestVersion)
{
  std::vector<LocalCountryFile> localFiles;
  FindAllLocalMapsAndCleanup(latestVersion, localFiles);
}

bool ParseVersion(std::string const & s, int64_t & version)
{
  if (s.empty() || s.size() > kMaxTimestampLength)
    return false;

  int64_t v = 0;
  for (char const c : s)
  {
    if (!std::isdigit(c))
      return false;
    v = v * 10 + c - '0';
  }
  version = v;
  return true;
}

shared_ptr<LocalCountryFile> PreparePlaceForCountryFiles(int64_t version, CountryFile const & countryFile)
{
  return PreparePlaceForCountryFiles(version, std::string(), countryFile);
}

shared_ptr<LocalCountryFile> PreparePlaceForCountryFiles(int64_t version, std::string const & dataDir,
                                                         CountryFile const & countryFile)
{
  std::string const dir = GetDataDirFullPath(dataDir);
  if (version == 0)
    return std::make_shared<LocalCountryFile>(dir, countryFile, version);
  std::string const directory = my::JoinFoldersToPath(dir, strings::to_string(version));
  if (!MkDirChecked(directory))
    return std::shared_ptr<LocalCountryFile>();
  return std::make_shared<LocalCountryFile>(directory, countryFile, version);
}

std::string GetFileDownloadPath(int64_t version, CountryFile const & countryFile, MapOptions file)
{
  return GetFileDownloadPath(version, std::string(), countryFile, file);
}

std::string GetFileDownloadPath(int64_t version, std::string const & dataDir,
                           CountryFile const & countryFile, MapOptions file)
{
  std::string const readyFile = GetFileName(countryFile.GetName(), file, version) + READY_FILE_EXTENSION;
  std::string const dir = GetDataDirFullPath(dataDir);
  if (version == 0)
    return my::JoinFoldersToPath(dir, readyFile);
  return my::JoinFoldersToPath({dir, strings::to_string(version)}, readyFile);
}

unique_ptr<ModelReader> GetCountryReader(platform::LocalCountryFile const & file, MapOptions options)
{
  Platform & platform = GetPlatform();
  // See LocalCountryFile comment for explanation.
  if (file.GetDirectory().empty())
  {
    return platform.GetReader(file.GetCountryName() + DATA_FILE_EXTENSION,
                              GetSpecialFilesSearchScope());
  }
  return platform.GetReader(file.GetPath(options), "f");
}

// static
void CountryIndexes::PreparePlaceOnDisk(LocalCountryFile const & localFile)
{
  std::string const dir = IndexesDir(localFile);
  if (!MkDirChecked(dir))
    MYTHROW(FileSystemException, ("Can't create directory", dir));
}

// static
bool CountryIndexes::DeleteFromDisk(LocalCountryFile const & localFile)
{
  std::string const directory = IndexesDir(localFile);
  bool ok = true;

  for (auto index : {Index::Bits, Index::Nodes, Index::Offsets})
  {
    std::string const path = GetPath(localFile, index);
    if (Platform::IsFileExistsByFullPath(path) && !my::DeleteFileX(path))
    {
      LOG(LWARNING, ("Can't remove country index:", path));
      ok = false;
    }
  }

  Platform::EError const ret = Platform::RmDir(directory);
  if (ret != Platform::ERR_OK && ret != Platform::ERR_FILE_DOES_NOT_EXIST)
  {
    LOG(LWARNING, ("Can't remove indexes directory:", directory, ret));
    ok = false;
  }
  return ok;
}

// static
std::string CountryIndexes::GetPath(LocalCountryFile const & localFile, Index index)
{
  char const * ext = nullptr;
  switch (index)
  {
    case Index::Bits:
      ext = kBitsExt;
      break;
    case Index::Nodes:
      ext = kNodesExt;
      break;
    case Index::Offsets:
      ext = kOffsetsExt;
      break;
  }
  return my::JoinFoldersToPath(IndexesDir(localFile), localFile.GetCountryName() + ext);
}

// static
void CountryIndexes::GetIndexesExts(std::vector<std::string> & exts)
{
  exts.push_back(kBitsExt);
  exts.push_back(kNodesExt);
  exts.push_back(kOffsetsExt);
}

// static
bool CountryIndexes::IsIndexFile(std::string const & file)
{
  return strings::EndsWith(file, kBitsExt) || strings::EndsWith(file, kNodesExt) ||
         strings::EndsWith(file, kOffsetsExt);
}

// static
std::string CountryIndexes::IndexesDir(LocalCountryFile const & localFile)
{
  std::string dir = localFile.GetDirectory();
  CountryFile const & file = localFile.GetCountryFile();

  /// @todo It's a temporary code until we will put fIndex->fOffset into mwm container.
  /// IndexesDir should not throw any exceptions.
  if (dir.empty())
  {
    // Local file is stored in resources. Need to prepare index folder in the writable directory.
    int64_t const version = localFile.GetVersion();
    ASSERT_GREATER(version, 0, ());

    dir = my::JoinFoldersToPath(GetPlatform().WritableDir(), strings::to_string(version));
    if (!MkDirChecked(dir))
      MYTHROW(FileSystemException, ("Can't create directory", dir));
  }

  return my::JoinFoldersToPath(dir, file.GetName());
}

std::string DebugPrint(CountryIndexes::Index index)
{
  switch (index)
  {
    case CountryIndexes::Index::Bits:
      return "Bits";
    case CountryIndexes::Index::Nodes:
      return "Nodes";
    case CountryIndexes::Index::Offsets:
      return "Offsets";
  }
}
}  // namespace platform
