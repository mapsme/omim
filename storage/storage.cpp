#include "storage/http_map_files_downloader.hpp"
#include "storage/storage.hpp"

#include "defines.hpp"

#include "platform/local_country_file_utils.hpp"
#include "platform/mwm_version.hpp"
#include "platform/platform.hpp"
#include "platform/servers_list.hpp"

#include "coding/file_name_utils.hpp"
#include "coding/internal/file_data.hpp"
#include "coding/reader.hpp"
#include "coding/url_encode.hpp"

#include "base/logging.hpp"
#include "base/scope_guard.hpp"
#include "base/string_utils.hpp"

#include "std/algorithm.hpp"
#include "std/bind.hpp"
#include "std/sstream.hpp"
#include "std/target_os.hpp"

#include "3party/Alohalytics/src/alohalytics.h"

using namespace downloader;
using namespace platform;

namespace storage
{
namespace
{

template <typename T>
void RemoveIf(vector<T> & v, function<bool(T const & t)> const & p)
{
  v.erase(remove_if(v.begin(), v.end(), p), v.end());
}

uint64_t GetLocalSize(shared_ptr<LocalCountryFile> file, MapOptions opt)
{
  if (!file)
    return 0;
  uint64_t size = 0;
  for (MapOptions bit : {MapOptions::Map, MapOptions::CarRouting})
  {
    if (HasOptions(opt, bit))
      size += file->GetSize(bit);
  }
  return size;
}

uint64_t GetRemoteSize(CountryFile const & file, MapOptions opt, int64_t version)
{
  if (version::IsSingleMwm(version))
    return opt == MapOptions::Nothing ? 0 : file.GetRemoteSize(MapOptions::Map);

  uint64_t size = 0;
  for (MapOptions bit : {MapOptions::Map, MapOptions::CarRouting})
  {
    if (HasOptions(opt, bit))
      size += file.GetRemoteSize(bit);
  }
  return size;
}

void DeleteCountryIndexes(LocalCountryFile const & localFile)
{
  platform::CountryIndexes::DeleteFromDisk(localFile);
}

void DeleteFromDiskWithIndexes(LocalCountryFile const & localFile, MapOptions options)
{
  DeleteCountryIndexes(localFile);
  localFile.DeleteFromDisk(options);
}
}  // namespace

Storage::Storage() : m_downloader(new HttpMapFilesDownloader()), m_currentSlotId(0)
{
  LoadCountriesFile(false /* forceReload */);
}

Storage::Storage(string const & referenceCountriesTxtJsonForTesting,
                 unique_ptr<MapFilesDownloader> mapDownloaderForTesting)
  : m_downloader(move(mapDownloaderForTesting)), m_currentSlotId(0)
{
  m_currentVersion = LoadCountries(referenceCountriesTxtJsonForTesting, m_countries);
  CHECK_LESS_OR_EQUAL(0, m_currentVersion, ("Can't load test countries file"));
}

void Storage::Init(TUpdate const & update) { m_update = update; }

void Storage::Clear()
{
  m_downloader->Reset();
  m_queue.clear();
  m_failedCountries.clear();
  m_localFiles.clear();
  m_localFilesForFakeCountries.clear();
}

void Storage::RegisterAllLocalMaps()
{
  m_localFiles.clear();
  m_localFilesForFakeCountries.clear();

  vector<LocalCountryFile> localFiles;
  FindAllLocalMapsAndCleanup(GetCurrentDataVersion(), localFiles);

  auto compareByCountryAndVersion = [](LocalCountryFile const & lhs, LocalCountryFile const & rhs)
  {
    if (lhs.GetCountryFile() != rhs.GetCountryFile())
      return lhs.GetCountryFile() < rhs.GetCountryFile();
    return lhs.GetVersion() > rhs.GetVersion();
  };

  auto equalByCountry = [](LocalCountryFile const & lhs, LocalCountryFile const & rhs)
  {
    return lhs.GetCountryFile() == rhs.GetCountryFile();
  };

  sort(localFiles.begin(), localFiles.end(), compareByCountryAndVersion);

  auto i = localFiles.begin();
  while (i != localFiles.end())
  {
    auto j = i + 1;
    while (j != localFiles.end() && equalByCountry(*i, *j))
    {
      LocalCountryFile & localFile = *j;
      LOG(LINFO, ("Removing obsolete", localFile));
      localFile.SyncWithDisk();
      DeleteFromDiskWithIndexes(localFile, MapOptions::MapWithCarRouting);
      ++j;
    }

    LocalCountryFile const & localFile = *i;
    string const & name = localFile.GetCountryName();
    TCountryId countryId = FindCountryIdByFile(name);
    if (IsCountryIdValid(countryId) && IsCoutryIdInCountryTree(countryId))
      RegisterCountryFiles(countryId, localFile.GetDirectory(), localFile.GetVersion());
    else
      RegisterFakeCountryFiles(localFile);

    LOG(LINFO, ("Found file:", name, "in directory:", localFile.GetDirectory()));

    i = j;
  }
}

void Storage::GetLocalMaps(vector<TLocalFilePtr> & maps) const
{
  for (auto const & p : m_localFiles)
    maps.push_back(GetLatestLocalFile(p.first));

  for (auto const & p : m_localFilesForFakeCountries)
    maps.push_back(p.second);

  maps.erase(unique(maps.begin(), maps.end()), maps.end());
}

size_t Storage::GetDownloadedFilesCount() const
{
  return m_localFiles.size();
}

CountriesContainerT const & NodeFromIndex(CountriesContainerT const & root, TCountryId const & countryId)
{
  SimpleTree<Country> const * node = root.FindLeaf(Country(countryId));
  CHECK(node, ("Node with id =", countryId, "not found in country tree."));
  return *node;
}

Country const & Storage::CountryByCountryId(TCountryId const & countryId) const
{
  return NodeFromIndex(m_countries, countryId).Value();
}

void Storage::GetGroupAndCountry(TCountryId const & countryId, string & group, string & country) const
{
  // @TODO(bykoianko) This method can work faster and more correctly.
  // 1. To get id for filling group parameter it's better to use take the parent
  //    of countryId parameter in m_countries tree. Just fill group
  //    with its parent name if valid.
  // 2. Use countryId as id for filling country parameter.
  // 3. To translate the ids got in (1) and (2) into strings for filling group and country
  //    use platform/get_text_by_id subsystem based on twine.

  string fName = CountryByCountryId(countryId).GetFile().GetName();
  CountryInfo::FileName2FullName(fName);
  CountryInfo::FullName2GroupAndMap(fName, group, country);
}

size_t Storage::CountriesCount(TCountryId const & countryId) const
{
  return NodeFromIndex(m_countries, countryId).ChildrenCount();
}

string const & Storage::CountryName(TCountryId const & countryId) const
{
  return NodeFromIndex(m_countries, countryId).Value().Name();
}

bool Storage::IsCoutryIdInCountryTree(TCountryId const & countryId) const
{
  return m_countries.Find(Country(countryId)) != nullptr;
}

LocalAndRemoteSizeT Storage::CountrySizeInBytes(TCountryId const & countryId, MapOptions opt) const
{
  QueuedCountry const * queuedCountry = FindCountryInQueue(countryId);
  TLocalFilePtr localFile = GetLatestLocalFile(countryId);
  CountryFile const & countryFile = GetCountryFile(countryId);
  if (queuedCountry == nullptr)
  {
    return LocalAndRemoteSizeT(GetLocalSize(localFile, opt),
                               GetRemoteSize(countryFile, opt, GetCurrentDataVersion()));
  }

  LocalAndRemoteSizeT sizes(0, GetRemoteSize(countryFile, opt, GetCurrentDataVersion()));
  if (!m_downloader->IsIdle() && IsCountryFirstInQueue(countryId))
  {
    sizes.first = m_downloader->GetDownloadingProgress().first +
                  GetRemoteSize(countryFile, queuedCountry->GetDownloadedFiles(),
                                GetCurrentDataVersion());
  }
  return sizes;
}

CountryFile const & Storage::GetCountryFile(TCountryId const & countryId) const
{
  return CountryByCountryId(countryId).GetFile();
}

Storage::TLocalFilePtr Storage::GetLatestLocalFile(CountryFile const & countryFile) const
{
  TCountryId const countryId = FindCountryIdByFile(countryFile.GetName());
  if (IsCountryIdValid(countryId) && IsCoutryIdInCountryTree(countryId))
  {
    TLocalFilePtr localFile = GetLatestLocalFile(countryId);
    if (localFile)
      return localFile;
  }

  auto const it = m_localFilesForFakeCountries.find(countryFile);
  if (it != m_localFilesForFakeCountries.end())
    return it->second;

  return TLocalFilePtr();
}

Storage::TLocalFilePtr Storage::GetLatestLocalFile(TCountryId const & countryId) const
{
  auto const it = m_localFiles.find(countryId);
  if (it == m_localFiles.end() || it->second.empty())
    return TLocalFilePtr();

  list<TLocalFilePtr> const & files = it->second;
  TLocalFilePtr latest = files.front();
  for (TLocalFilePtr const & file : files)
  {
    if (file->GetVersion() > latest->GetVersion())
      latest = file;
  }
  return latest;
}

TStatus Storage::CountryStatus(TCountryId const & countryId) const
{
  // Check if we already downloading this country or have it in the queue
  if (IsCountryInQueue(countryId))
  {
    if (IsCountryFirstInQueue(countryId))
      return TStatus::EDownloading;
    else
      return TStatus::EInQueue;
  }

  // Check if this country has failed while downloading.
  if (m_failedCountries.count(countryId) > 0)
    return TStatus::EDownloadFailed;

  return TStatus::EUnknown;
}

TStatus Storage::CountryStatusEx(TCountryId const & countryId) const
{
  return CountryStatusFull(countryId, CountryStatus(countryId));
}

void Storage::CountryStatusEx(TCountryId const & countryId, TStatus & status, MapOptions & options) const
{
  status = CountryStatusEx(countryId);

  if (status == TStatus::EOnDisk || status == TStatus::EOnDiskOutOfDate)
  {
    options = MapOptions::Map;

    TLocalFilePtr localFile = GetLatestLocalFile(countryId);
    ASSERT(localFile, ("Invariant violation: local file out of sync with disk."));
    if (localFile->OnDisk(MapOptions::CarRouting))
      options = SetOptions(options, MapOptions::CarRouting);
  }
}

void Storage::DownloadCountry(TCountryId const & countryId, MapOptions opt)
{
  opt = NormalizeDownloadFileSet(countryId, opt);
  if (opt == MapOptions::Nothing)
    return;

  if (QueuedCountry * queuedCountry = FindCountryInQueue(countryId))
  {
    queuedCountry->AddOptions(opt);
    return;
  }

  m_failedCountries.erase(countryId);
  m_queue.push_back(QueuedCountry(countryId, opt));
  if (m_queue.size() == 1)
    DownloadNextCountryFromQueue();
  else
    NotifyStatusChanged(countryId);
}

void Storage::DeleteCountry(TCountryId const & countryId, MapOptions opt)
{
  opt = NormalizeDeleteFileSet(opt);
  DeleteCountryFiles(countryId, opt);
  DeleteCountryFilesFromDownloader(countryId, opt);

  TLocalFilePtr localFile = GetLatestLocalFile(countryId);
  if (localFile)
    m_update(*localFile);

  NotifyStatusChanged(countryId);
}

void Storage::DeleteCustomCountryVersion(LocalCountryFile const & localFile)
{
  CountryFile const countryFile = localFile.GetCountryFile();
  DeleteFromDiskWithIndexes(localFile, MapOptions::MapWithCarRouting);

  {
    auto it = m_localFilesForFakeCountries.find(countryFile);
    if (it != m_localFilesForFakeCountries.end())
    {
      m_localFilesForFakeCountries.erase(it);
      return;
    }
  }

  TCountryId const countryId = FindCountryIdByFile(countryFile.GetName());
  if (!(IsCountryIdValid(countryId) && IsCoutryIdInCountryTree(countryId)))
  {
    LOG(LERROR, ("Removed files for an unknown country:", localFile));
    return;
  }

  MY_SCOPE_GUARD(notifyStatusChanged, bind(&Storage::NotifyStatusChanged, this, countryId));

  // If file version equals to current data version, delete from downloader all pending requests for
  // the country.
  if (localFile.GetVersion() == GetCurrentDataVersion())
    DeleteCountryFilesFromDownloader(countryId, MapOptions::MapWithCarRouting);
  auto countryFilesIt = m_localFiles.find(countryId);
  if (countryFilesIt == m_localFiles.end())
  {
    LOG(LERROR, ("Deleted files of an unregistered country:", localFile));
    return;
  }

  auto const equalsToLocalFile = [&localFile](TLocalFilePtr const & rhs)
  {
    return localFile == *rhs;
  };
  countryFilesIt->second.remove_if(equalsToLocalFile);
}

void Storage::NotifyStatusChanged(TCountryId const & countryId)
{
  for (CountryObservers const & observer : m_observers)
    observer.m_changeCountryFn(countryId);
}

void Storage::DownloadNextCountryFromQueue()
{
  if (m_queue.empty())
    return;

  QueuedCountry & queuedCountry = m_queue.front();
  TCountryId const & countryId = queuedCountry.GetCountryId();

  // It's not even possible to prepare directory for files before
  // downloading.  Mark this country as failed and switch to next
  // country.
  if (!PreparePlaceForCountryFiles(GetCountryFile(countryId), GetCurrentDataVersion()))
  {
    OnMapDownloadFinished(countryId, false /* success */, queuedCountry.GetInitOptions());
    NotifyStatusChanged(countryId);
    m_queue.pop_front();
    DownloadNextCountryFromQueue();
    return;
  }

  DownloadNextFile(queuedCountry);

  // New status for the country, "Downloading"
  NotifyStatusChanged(queuedCountry.GetCountryId());
}

void Storage::DownloadNextFile(QueuedCountry const & country)
{
  TCountryId const & countryId = country.GetCountryId();
  CountryFile const & countryFile = GetCountryFile(countryId);

  string const filePath = GetFileDownloadPath(countryId, country.GetCurrentFile());
  uint64_t size;

  // It may happen that the file already was downloaded, so there're
  // no need to request servers list and download file.  Let's
  // switch to next file.
  if (GetPlatform().GetFileSizeByFullPath(filePath, size))
  {
    OnMapFileDownloadFinished(true /* success */, MapFilesDownloader::TProgress(size, size));
    return;
  }

  // send Country name for statistics
  m_downloader->GetServersList(GetCurrentDataVersion(), countryFile.GetName(),
                               bind(&Storage::OnServerListDownloaded, this, _1));
}

bool Storage::DeleteFromDownloader(TCountryId const & countryId)
{
  if (!DeleteCountryFilesFromDownloader(countryId, MapOptions::MapWithCarRouting))
    return false;
  NotifyStatusChanged(countryId);
  return true;
}

bool Storage::IsDownloadInProgress() const { return !m_queue.empty(); }

TCountryId Storage::GetCurrentDownloadingCountryIndex() const
{
  return IsDownloadInProgress() ? m_queue.front().GetCountryId() : storage::TCountryId();
}

void Storage::LoadCountriesFile(bool forceReload)
{
  if (forceReload)
    m_countries.Clear();

  if (m_countries.ChildrenCount() == 0)
  {
    string json;
    ReaderPtr<Reader>(GetPlatform().GetReader(COUNTRIES_FILE)).ReadAsString(json);
    m_currentVersion = LoadCountries(json, m_countries);
    if (m_currentVersion < 0)
      LOG(LERROR, ("Can't load countries file", COUNTRIES_FILE));
  }
}

int Storage::Subscribe(TChangeCountryFunction const & change, TProgressFunction const & progress)
{
  CountryObservers obs;

  obs.m_changeCountryFn = change;
  obs.m_progressFn = progress;
  obs.m_slotId = ++m_currentSlotId;

  m_observers.push_back(obs);

  return obs.m_slotId;
}

void Storage::Unsubscribe(int slotId)
{
  for (auto i = m_observers.begin(); i != m_observers.end(); ++i)
  {
    if (i->m_slotId == slotId)
    {
      m_observers.erase(i);
      return;
    }
  }
}

void Storage::OnMapFileDownloadFinished(bool success,
                                        MapFilesDownloader::TProgress const & progress)
{
  if (m_queue.empty())
    return;

  QueuedCountry & queuedCountry = m_queue.front();
  TCountryId const countryId = queuedCountry.GetCountryId();

  if (success && queuedCountry.SwitchToNextFile())
  {
    DownloadNextFile(queuedCountry);
    return;
  }

  OnMapDownloadFinished(countryId, success, queuedCountry.GetInitOptions());
  m_queue.pop_front();

  NotifyStatusChanged(countryId);
  m_downloader->Reset();
  DownloadNextCountryFromQueue();
}

void Storage::ReportProgress(TCountryId const & countryId, pair<int64_t, int64_t> const & p)
{
  for (CountryObservers const & o : m_observers)
    o.m_progressFn(countryId, p);
}

void Storage::OnServerListDownloaded(vector<string> const & urls)
{
  // Queue can be empty because countries were deleted from queue.
  if (m_queue.empty())
    return;

  QueuedCountry const & queuedCountry = m_queue.front();
  TCountryId const & countryId = queuedCountry.GetCountryId();
  MapOptions const file = queuedCountry.GetCurrentFile();

  vector<string> fileUrls;
  fileUrls.reserve(urls.size());
  for (string const & url : urls)
    fileUrls.push_back(GetFileDownloadUrl(url, countryId, file));

  string const filePath = GetFileDownloadPath(countryId, file);
  m_downloader->DownloadMapFile(fileUrls, filePath, GetDownloadSize(queuedCountry),
                                bind(&Storage::OnMapFileDownloadFinished, this, _1, _2),
                                bind(&Storage::OnMapFileDownloadProgress, this, _1));
}

void Storage::OnMapFileDownloadProgress(MapFilesDownloader::TProgress const & progress)
{
  // Queue can be empty because countries were deleted from queue.
  if (m_queue.empty())
    return;

  if (!m_observers.empty())
  {
    QueuedCountry & queuedCountry = m_queue.front();
    CountryFile const & countryFile = GetCountryFile(queuedCountry.GetCountryId());
    MapFilesDownloader::TProgress p = progress;
    p.first += GetRemoteSize(countryFile, queuedCountry.GetDownloadedFiles(), GetCurrentDataVersion());
    p.second = GetRemoteSize(countryFile, queuedCountry.GetInitOptions(), GetCurrentDataVersion());

    ReportProgress(m_queue.front().GetCountryId(), p);
  }
}

bool Storage::RegisterDownloadedFiles(TCountryId const & countryId, MapOptions files)
{
  CountryFile const countryFile = GetCountryFile(countryId);
  TLocalFilePtr localFile = GetLocalFile(countryId, GetCurrentDataVersion());
  if (!localFile)
    localFile = PreparePlaceForCountryFiles(countryFile, GetCurrentDataVersion());
  if (!localFile)
  {
    LOG(LERROR, ("Local file data structure can't be prepared for downloaded file(", countryFile,
                 files, ")."));
    return false;
  }

  bool ok = true;
  for (MapOptions file : {MapOptions::Map, MapOptions::CarRouting})
  {
    if (!HasOptions(files, file))
      continue;
    string const path = GetFileDownloadPath(countryId, file);
    if (!my::RenameFileX(path, localFile->GetPath(file)))
    {
      ok = false;
      break;
    }
  }
  localFile->SyncWithDisk();
  if (!ok)
  {
    localFile->DeleteFromDisk(files);
    return false;
  }
  RegisterCountryFiles(localFile);
  return true;
}

void Storage::OnMapDownloadFinished(TCountryId const & countryId, bool success, MapOptions files)
{
  ASSERT_NOT_EQUAL(MapOptions::Nothing, files,
                   ("This method should not be called for empty files set."));
  {
    alohalytics::LogEvent("$OnMapDownloadFinished",
        alohalytics::TStringMap({{"name", GetCountryFile(countryId).GetName()},
                                 {"status", success ? "ok" : "failed"},
                                 {"version", strings::to_string(GetCurrentDataVersion())},
                                 {"option", DebugPrint(files)}}));
  }

  success = success && RegisterDownloadedFiles(countryId, files);

  if (!success)
  {
    m_failedCountries.insert(countryId);
    return;
  }

  TLocalFilePtr localFile = GetLocalFile(countryId, GetCurrentDataVersion());
  ASSERT(localFile, ());
  DeleteCountryIndexes(*localFile);
  m_update(*localFile);
}

string Storage::GetFileDownloadUrl(string const & baseUrl, TCountryId const & countryId,
                                   MapOptions file) const
{
  CountryFile const & countryFile = GetCountryFile(countryId);

  string const fileName = GetFileName(countryFile.GetName(), file, GetCurrentDataVersion());
  return GetFileDownloadUrl(baseUrl, fileName);
}

string Storage::GetFileDownloadUrl(string const & baseUrl, string const & fName) const
{
  return baseUrl + OMIM_OS_NAME "/" + strings::to_string(GetCurrentDataVersion()) + "/" +
         UrlEncode(fName);
}

TCountryId Storage::FindCountryIdByFile(string const & name) const
{
  // @TODO(bykoianko) Probably it's worth to check here if name represent a node in the tree.
  return TCountryId(name);
}

vector<TCountryId> Storage::FindAllIndexesByFile(string const & name) const
{
  // @TODO(bykoianko) This method should be rewritten. At list now name and the param of Find
  // have different types: string and TCountryId.
  vector<TCountryId> result;
  if (m_countries.Find(name))
    result.push_back(name);
  return result;
}

void Storage::GetOutdatedCountries(vector<Country const *> & countries) const
{
  for (auto const & p : m_localFiles)
  {
    TCountryId const & countryId = p.first;
    string const name = GetCountryFile(countryId).GetName();
    TLocalFilePtr file = GetLatestLocalFile(countryId);
    if (file && file->GetVersion() != GetCurrentDataVersion() &&
        name != WORLD_COASTS_FILE_NAME && name != WORLD_FILE_NAME)
    {
      countries.push_back(&CountryByCountryId(countryId));
    }
  }
}

TStatus Storage::CountryStatusWithoutFailed(TCountryId const & countryId) const
{
  // First, check if we already downloading this country or have in in the queue.
  if (!IsCountryInQueue(countryId))
    return CountryStatusFull(countryId, TStatus::EUnknown);
  return IsCountryFirstInQueue(countryId) ? TStatus::EDownloading : TStatus::EInQueue;
}

TStatus Storage::CountryStatusFull(TCountryId const & countryId, TStatus const status) const
{
  if (status != TStatus::EUnknown)
    return status;

  TLocalFilePtr localFile = GetLatestLocalFile(countryId);
  if (!localFile || !localFile->OnDisk(MapOptions::Map))
    return TStatus::ENotDownloaded;

  CountryFile const & countryFile = GetCountryFile(countryId);
  if (GetRemoteSize(countryFile, MapOptions::Map, GetCurrentDataVersion()) == 0)
    return TStatus::EUnknown;

  if (localFile->GetVersion() != GetCurrentDataVersion())
    return TStatus::EOnDiskOutOfDate;
  return TStatus::EOnDisk;
}

MapOptions Storage::NormalizeDownloadFileSet(TCountryId const & countryId, MapOptions options) const
{
  auto const & country = GetCountryFile(countryId);

  // Car routing files are useless without map files.
  if (HasOptions(options, MapOptions::CarRouting))
    options = SetOptions(options, MapOptions::Map);

  TLocalFilePtr localCountryFile = GetLatestLocalFile(countryId);
  for (MapOptions option : {MapOptions::Map, MapOptions::CarRouting})
  {
    // Check whether requested files are on disk and up-to-date.
    if (HasOptions(options, option) && localCountryFile && localCountryFile->OnDisk(option) &&
        localCountryFile->GetVersion() == GetCurrentDataVersion())
    {
      options = UnsetOptions(options, option);
    }

    // Check whether requested file is not empty.
    if (GetRemoteSize(country, option, GetCurrentDataVersion()) == 0)
    {
      ASSERT_NOT_EQUAL(MapOptions::Map, option, ("Map can't be empty."));
      options = UnsetOptions(options, option);
    }
  }

  return options;
}

// @TODO(bykoianko) This method does nothing and should be removed.
MapOptions Storage::NormalizeDeleteFileSet(MapOptions options) const
{
  // Car routing files are useless without map files.
  if (HasOptions(options, MapOptions::Map))
    options = SetOptions(options, MapOptions::CarRouting);
  return options;
}

QueuedCountry * Storage::FindCountryInQueue(TCountryId const & countryId)
{
  auto it = find(m_queue.begin(), m_queue.end(), countryId);
  return it == m_queue.end() ? nullptr : &*it;
}

QueuedCountry const * Storage::FindCountryInQueue(TCountryId const & countryId) const
{
  auto it = find(m_queue.begin(), m_queue.end(), countryId);
  return it == m_queue.end() ? nullptr : &*it;
}

bool Storage::IsCountryInQueue(TCountryId const & countryId) const
{
  return FindCountryInQueue(countryId) != nullptr;
}

bool Storage::IsCountryFirstInQueue(TCountryId const & countryId) const
{
  return !m_queue.empty() && m_queue.front().GetCountryId() == countryId;
}

void Storage::SetDownloaderForTesting(unique_ptr<MapFilesDownloader> && downloader)
{
  m_downloader = move(downloader);
}

void Storage::SetCurrentDataVersionForTesting(int64_t currentVersion)
{
  m_currentVersion = currentVersion;
}

Storage::TLocalFilePtr Storage::GetLocalFile(TCountryId const & countryId, int64_t version) const
{
  auto const it = m_localFiles.find(countryId);
  if (it == m_localFiles.end() || it->second.empty())
    return TLocalFilePtr();

  for (auto const & file : it->second)
  {
    if (file->GetVersion() == version)
      return file;
  }
  return TLocalFilePtr();
}

void Storage::RegisterCountryFiles(TLocalFilePtr localFile)
{
  CHECK(localFile, ());
  localFile->SyncWithDisk();

  for (auto const & countryId : FindAllIndexesByFile(localFile->GetCountryName()))
  {
    TLocalFilePtr existingFile = GetLocalFile(countryId, localFile->GetVersion());
    if (existingFile)
      ASSERT_EQUAL(localFile.get(), existingFile.get(), ());
    else
      m_localFiles[countryId].push_front(localFile);
  }
}

void Storage::RegisterCountryFiles(TCountryId const & countryId, string const & directory, int64_t version)
{
  TLocalFilePtr localFile = GetLocalFile(countryId, version);
  if (localFile)
    return;

  CountryFile const & countryFile = GetCountryFile(countryId);
  localFile = make_shared<LocalCountryFile>(directory, countryFile, version);
  RegisterCountryFiles(localFile);
}

void Storage::RegisterFakeCountryFiles(platform::LocalCountryFile const & localFile)
{
  TLocalFilePtr fakeCountryLocalFile = make_shared<LocalCountryFile>(localFile);
  fakeCountryLocalFile->SyncWithDisk();
  m_localFilesForFakeCountries[fakeCountryLocalFile->GetCountryFile()] = fakeCountryLocalFile;
}

void Storage::DeleteCountryFiles(TCountryId const & countryId, MapOptions opt)
{
  auto const it = m_localFiles.find(countryId);
  if (it == m_localFiles.end())
    return;

  auto & localFiles = it->second;
  for (auto & localFile : localFiles)
  {
    DeleteFromDiskWithIndexes(*localFile, opt);
    localFile->SyncWithDisk();
    if (localFile->GetFiles() == MapOptions::Nothing)
      localFile.reset();
  }
  auto isNull = [](TLocalFilePtr const & localFile)
  {
    return !localFile;
  };
  localFiles.remove_if(isNull);
  if (localFiles.empty())
    m_localFiles.erase(countryId);
}

bool Storage::DeleteCountryFilesFromDownloader(TCountryId const & countryId, MapOptions opt)
{
  QueuedCountry * queuedCountry = FindCountryInQueue(countryId);
  if (!queuedCountry)
    return false;

  opt = IntersectOptions(opt, queuedCountry->GetInitOptions());

  if (IsCountryFirstInQueue(countryId))
  {
    // Abrupt downloading of the current file if it should be removed.
    if (HasOptions(opt, queuedCountry->GetCurrentFile()))
      m_downloader->Reset();

    // Remove all files downloader had been created for a country.
    DeleteDownloaderFilesForCountry(GetCountryFile(countryId), GetCurrentDataVersion());
  }

  queuedCountry->RemoveOptions(opt);

  // Remove country from the queue if there's nothing to download.
  if (queuedCountry->GetInitOptions() == MapOptions::Nothing)
    m_queue.erase(find(m_queue.begin(), m_queue.end(), countryId));

  if (!m_queue.empty() && m_downloader->IsIdle())
  {
    // Kick possibly interrupted downloader.
    if (IsCountryFirstInQueue(countryId))
      DownloadNextFile(m_queue.front());
    else
      DownloadNextCountryFromQueue();
  }
  return true;
}

uint64_t Storage::GetDownloadSize(QueuedCountry const & queuedCountry) const
{
  CountryFile const & file = GetCountryFile(queuedCountry.GetCountryId());
  return GetRemoteSize(file, queuedCountry.GetCurrentFile(), GetCurrentDataVersion());
}

string Storage::GetFileDownloadPath(TCountryId const & countryId, MapOptions file) const
{
  return platform::GetFileDownloadPath(GetCountryFile(countryId), file, GetCurrentDataVersion());
}

TCountryId const Storage::GetRootId() const
{
  return m_countries.Value().Name();
}

vector<TCountryId> const Storage::GetChildren(TCountryId const & parent) const
{
  CountriesContainerT const * parentNode = m_countries.Find(parent);
  if (parentNode == nullptr)
  {
    ASSERT(false, ("TCountryId =", parent, "not found in m_countries."));
    return vector<TCountryId>();
  }

  size_t const childrenCount = parentNode->ChildrenCount();
  vector<TCountryId> childrenVector;
  childrenVector.reserve(childrenCount);
  for (size_t i = 0; i < childrenCount; ++i)
    childrenVector.emplace_back(parentNode->Child(i).Value().Name());

  return childrenVector;
}
}  // namespace storage
