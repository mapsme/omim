#include "storage/storage.hpp"

#include "storage/country_tree_helpers.hpp"
#include "storage/diff_scheme/apply_diff.hpp"
#include "storage/diff_scheme/diff_scheme_loader.hpp"
#include "storage/http_map_files_downloader.hpp"
#include "storage/storage_helpers.hpp"

#include "platform/local_country_file_utils.hpp"
#include "platform/marketing_service.hpp"
#include "platform/mwm_version.hpp"
#include "platform/platform.hpp"
#include "platform/preferred_languages.hpp"
#include "platform/settings.hpp"

#include "coding/internal/file_data.hpp"

#include "base/exception.hpp"
#include "base/file_name_utils.hpp"
#include "base/gmtime.hpp"
#include "base/logging.hpp"
#include "base/scope_guard.hpp"
#include "base/stl_helpers.hpp"
#include "base/string_utils.hpp"

#include "defines.hpp"

#include <algorithm>
#include <chrono>
#include <limits>
#include <sstream>

#include "3party/Alohalytics/src/alohalytics.h"

using namespace downloader;
using namespace generator::mwm_diff;
using namespace platform;
using namespace std;
using namespace std::chrono;
using namespace std::placeholders;

namespace storage
{
namespace
{
string const kUpdateQueueKey = "UpdateQueue";
string const kDownloadQueueKey = "DownloadQueue";

void DeleteCountryIndexes(LocalCountryFile const & localFile)
{
  platform::CountryIndexes::DeleteFromDisk(localFile);
}

void DeleteFromDiskWithIndexes(LocalCountryFile const & localFile, MapFileType type)
{
  DeleteCountryIndexes(localFile);
  localFile.DeleteFromDisk(type);
}

CountryTree::Node const & LeafNodeFromCountryId(CountryTree const & root,
                                                CountryId const & countryId)
{
  CountryTree::Node const * node = root.FindFirstLeaf(countryId);
  CHECK(node, ("Node with id =", countryId, "not found in country tree as a leaf."));
  return *node;
}

bool ValidateIntegrity(LocalFilePtr mapLocalFile, string const & countryId, string const & source)
{
  int64_t const version = mapLocalFile->GetVersion();

  int64_t constexpr kMinSupportedVersion = 181030;
  if (version < kMinSupportedVersion)
    return true;

  if (mapLocalFile->ValidateIntegrity())
    return true;

  alohalytics::LogEvent("$MapIntegrityFailure",
                        alohalytics::TStringMap({{"mwm", countryId},
                                                 {"version", strings::to_string(version)},
                                                 {"source", source}}));
  return false;
}

bool IsFileDownloaded(string const fileDownloadPath, MapFileType type)
{
  // Since a downloaded valid diff file may be either with .diff or .diff.ready extension,
  // we have to check these both cases in order to find
  // the diff file which is ready to apply.
  // If there is such a file we have to cause the success download scenario.
  string const readyFilePath = fileDownloadPath;
  bool isDownloadedDiff = false;
  if (type == MapFileType::Diff)
  {
    string filePath = readyFilePath;
    base::GetNameWithoutExt(filePath);
    isDownloadedDiff = GetPlatform().IsFileExistsByFullPath(filePath);
  }

  // It may happen that the file already was downloaded, so there is
  // no need to request servers list and download file.  Let's
  // switch to next file.
  return isDownloadedDiff || GetPlatform().IsFileExistsByFullPath(readyFilePath);
}

void SendStatisticsAfterDownloading(CountryId const & countryId, DownloadStatus status,
                                    MapFileType type, int64_t dataVersion,
                                    std::map<CountryId, std::list<LocalFilePtr>> const & localFiles)
{
  // Send statistics to PushWoosh. We send these statistics only for the newly downloaded
  // mwms, not for updated ones.
  if (status == DownloadStatus::Completed && type != MapFileType::Diff)
  {
    auto const it = localFiles.find(countryId);
    if (it == localFiles.end())
    {
      auto & marketingService = GetPlatform().GetMarketingService();
      marketingService.SendPushWooshTag(marketing::kMapLastDownloaded, countryId);
      auto const nowStr = marketingService.GetPushWooshTimestamp();
      marketingService.SendPushWooshTag(marketing::kMapLastDownloadedTimestamp, nowStr);
    }
  }

  alohalytics::LogEvent("$OnMapDownloadFinished",
                        alohalytics::TStringMap(
                          {{"name", countryId},
                           {"status", status == DownloadStatus::Completed ? "ok" : "failed"},
                           {"version", strings::to_string(dataVersion)},
                           {"option", DebugPrint(type)}}));
  GetPlatform().GetMarketingService().SendMarketingEvent(marketing::kDownloaderMapActionFinished,
                                                         {{"action", "download"}});
}
}  // namespace

void GetQueuedCountries(Queue const & queue, CountriesSet & resultCountries)
{
  for (auto const & country : queue)
    resultCountries.insert(country.GetCountryId());
}

Progress Storage::GetOverallProgress(CountriesVec const & countries) const
{
  Progress overallProgress = {};
  for (auto const & country : countries)
  {
    NodeAttrs attr;
    GetNodeAttrs(country, attr);

    ASSERT_EQUAL(attr.m_mwmCounter, 1, ());

    if (attr.m_downloadingProgress.second != -1)
    {
      overallProgress.first += attr.m_downloadingProgress.first;
      overallProgress.second += attr.m_downloadingProgress.second;
    }
  }
  return overallProgress;
}

Storage::Storage(string const & pathToCountriesFile /* = COUNTRIES_FILE */,
                 string const & dataDir /* = string() */)
  : m_downloader(make_unique<HttpMapFilesDownloader>())
  , m_dataDir(dataDir)
{
  m_downloader->SetDownloadingPolicy(m_downloadingPolicy);
  m_downloader->Subscribe(this);

  SetLocale(languages::GetCurrentTwine());
  LoadCountriesFile(pathToCountriesFile);
  CalcMaxMwmSizeBytes();
}

Storage::Storage(string const & referenceCountriesTxtJsonForTesting,
                 unique_ptr<MapFilesDownloader> mapDownloaderForTesting)
  : m_downloader(move(mapDownloaderForTesting))
{
  m_downloader->SetDownloadingPolicy(m_downloadingPolicy);
  m_downloader->Subscribe(this);

  m_currentVersion =
      LoadCountriesFromBuffer(referenceCountriesTxtJsonForTesting, m_countries, m_affiliations,
                              m_countryNameSynonyms, m_mwmTopCityGeoIds, m_mwmTopCountryGeoIds);
  CHECK_LESS_OR_EQUAL(0, m_currentVersion, ("Can't load test countries file"));
  CalcMaxMwmSizeBytes();
}

void Storage::Init(UpdateCallback const & didDownload, DeleteCallback const & willDelete)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());

  m_didDownload = didDownload;
  m_willDelete = willDelete;
}

void Storage::SetDownloadingPolicy(DownloadingPolicy * policy)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());

  m_downloadingPolicy = policy;
  m_downloader->SetDownloadingPolicy(policy);
}

void Storage::DeleteAllLocalMaps(CountriesVec * existedCountries /* = nullptr */)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());

  for (auto const & localFiles : m_localFiles)
  {
    for (auto const & localFile : localFiles.second)
    {
      LOG_SHORT(LINFO, ("Remove:", localFiles.first, DebugPrint(*localFile)));
      if (existedCountries)
        existedCountries->push_back(localFiles.first);
      localFile->SyncWithDisk();
      DeleteFromDiskWithIndexes(*localFile, MapFileType::Map);
      DeleteFromDiskWithIndexes(*localFile, MapFileType::Diff);
    }
  }
}

bool Storage::HaveDownloadedCountries() const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());

  return !m_localFiles.empty();
}

void Storage::Clear()
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());

  m_downloader->Clear();
  m_justDownloaded.clear();
  m_failedCountries.clear();
  m_localFiles.clear();
  m_localFilesForFakeCountries.clear();
  SaveDownloadQueue();
}

void Storage::RegisterAllLocalMaps(bool enableDiffs)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());

  m_localFiles.clear();
  m_localFilesForFakeCountries.clear();

  vector<LocalCountryFile> localFiles;
  FindAllLocalMapsAndCleanup(GetCurrentDataVersion(), m_dataDir, localFiles);

  auto compareByCountryAndVersion = [](LocalCountryFile const & lhs, LocalCountryFile const & rhs) {
    if (lhs.GetCountryFile() != rhs.GetCountryFile())
      return lhs.GetCountryFile() < rhs.GetCountryFile();
    return lhs.GetVersion() > rhs.GetVersion();
  };

  auto equalByCountry = [](LocalCountryFile const & lhs, LocalCountryFile const & rhs) {
    return lhs.GetCountryFile() == rhs.GetCountryFile();
  };

  sort(localFiles.begin(), localFiles.end(), compareByCountryAndVersion);

  int64_t minVersion = std::numeric_limits<int64_t>().max();
  int64_t maxVersion = std::numeric_limits<int64_t>().min();

  auto i = localFiles.begin();
  while (i != localFiles.end())
  {
    auto j = i + 1;
    while (j != localFiles.end() && equalByCountry(*i, *j))
    {
      LocalCountryFile & localFile = *j;
      LOG(LINFO, ("Removing obsolete", localFile));
      localFile.SyncWithDisk();

      DeleteFromDiskWithIndexes(localFile, MapFileType::Map);
      DeleteFromDiskWithIndexes(localFile, MapFileType::Diff);
      ++j;
    }

    LocalCountryFile const & localFile = *i;
    string const & name = localFile.GetCountryName();
    if (name != WORLD_FILE_NAME && name != WORLD_COASTS_FILE_NAME)
    {
      auto const version = localFile.GetVersion();
      if (version < minVersion)
        minVersion = version;
      if (version > maxVersion)
        maxVersion = version;
    }

    CountryId countryId = FindCountryIdByFile(name);
    if (IsLeaf(countryId))
      RegisterCountryFiles(countryId, localFile.GetDirectory(), localFile.GetVersion());
    else
      RegisterFakeCountryFiles(localFile);

    LOG(LINFO, ("Found file:", name, "in directory:", localFile.GetDirectory()));

    i = j;
  }

  if (minVersion > maxVersion)
  {
    minVersion = 0;
    maxVersion = 0;
  }

  GetPlatform().GetMarketingService().SendPushWooshTag(marketing::kMapVersionMin,
                                                       strings::to_string(minVersion));
  GetPlatform().GetMarketingService().SendPushWooshTag(marketing::kMapVersionMax,
                                                       strings::to_string(maxVersion));

  FindAllDiffs(m_dataDir, m_notAppliedDiffs);
  if (enableDiffs)
    LoadDiffScheme();
  // Note: call order is important, diffs loading must be called first.
  // Since diffs info downloading and servers list downloading
  // are working on network thread, consecutive executing is guaranteed.
  RestoreDownloadQueue();
}

void Storage::GetLocalMaps(vector<LocalFilePtr> & maps) const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());

  for (auto const & p : m_localFiles)
    maps.push_back(GetLatestLocalFile(p.first));

  for (auto const & p : m_localFilesForFakeCountries)
    maps.push_back(p.second);

  maps.erase(unique(maps.begin(), maps.end()), maps.end());
}

size_t Storage::GetDownloadedFilesCount() const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());

  return m_localFiles.size();
}

Country const & Storage::CountryLeafByCountryId(CountryId const & countryId) const
{
  return LeafNodeFromCountryId(m_countries, countryId).Value();
}

Country const & Storage::CountryByCountryId(CountryId const & countryId) const
{
  CountryTree::Node const * node = m_countries.FindFirst(countryId);
  CHECK(node, ("Node with id =", countryId, "not found in country tree."));
  return node->Value();
}

bool Storage::IsNode(CountryId const & countryId) const
{
  if (!IsCountryIdValid(countryId))
    return false;
  return m_countries.FindFirst(countryId) != nullptr;
}

bool Storage::IsLeaf(CountryId const & countryId) const
{
  if (!IsCountryIdValid(countryId))
    return false;
  CountryTree::Node const * const node = m_countries.FindFirst(countryId);
  return node != nullptr && node->ChildrenCount() == 0;
}

bool Storage::IsInnerNode(CountryId const & countryId) const
{
  if (!IsCountryIdValid(countryId))
    return false;
  CountryTree::Node const * const node = m_countries.FindFirst(countryId);
  return node != nullptr && node->ChildrenCount() != 0;
}

LocalAndRemoteSize Storage::CountrySizeInBytes(CountryId const & countryId) const
{
  LocalFilePtr localFile = GetLatestLocalFile(countryId);
  CountryFile const & countryFile = GetCountryFile(countryId);
  LocalAndRemoteSize sizes(0, GetRemoteSize(countryFile));

  if (!IsCountryInQueue(countryId) && !IsDiffApplyingInProgressToCountry(countryId))
    sizes.first = localFile ? localFile->GetSize(MapFileType::Map) : 0;

  if (!m_downloader->IsIdle() && IsCountryFirstInQueue(countryId))
  {
    sizes.first = m_downloader->GetDownloadingProgress().first + GetRemoteSize(countryFile);
  }
  return sizes;
}

CountryFile const & Storage::GetCountryFile(CountryId const & countryId) const
{
  return CountryLeafByCountryId(countryId).GetFile();
}

LocalFilePtr Storage::GetLatestLocalFile(CountryFile const & countryFile) const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());

  CountryId const countryId = FindCountryIdByFile(countryFile.GetName());
  if (IsLeaf(countryId))
  {
    LocalFilePtr localFile = GetLatestLocalFile(countryId);
    if (localFile)
      return localFile;
  }

  auto const it = m_localFilesForFakeCountries.find(countryFile);
  if (it != m_localFilesForFakeCountries.end())
    return it->second;

  return LocalFilePtr();
}

LocalFilePtr Storage::GetLatestLocalFile(CountryId const & countryId) const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());

  auto const it = m_localFiles.find(countryId);
  if (it == m_localFiles.end() || it->second.empty())
    return LocalFilePtr();

  list<LocalFilePtr> const & files = it->second;
  LocalFilePtr latest = files.front();
  for (LocalFilePtr const & file : files)
  {
    if (file->GetVersion() > latest->GetVersion())
      latest = file;
  }
  return latest;
}

Status Storage::CountryStatus(CountryId const & countryId) const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());

  // Check if this country has failed while downloading.
  if (m_failedCountries.count(countryId) > 0)
    return Status::EDownloadFailed;

  // Check if we are already downloading this country or have it in the queue.
  if (IsCountryInQueue(countryId))
  {
    if (IsCountryFirstInQueue(countryId))
      return Status::EDownloading;

    return Status::EInQueue;
  }

  if (IsDiffApplyingInProgressToCountry(countryId))
    return Status::EApplying;

  return Status::EUnknown;
}

Status Storage::CountryStatusEx(CountryId const & countryId) const
{
  auto const status = CountryStatus(countryId);
  if (status != Status::EUnknown)
    return status;

  auto localFile = GetLatestLocalFile(countryId);
  if (!localFile || !localFile->OnDisk(MapFileType::Map))
    return Status::ENotDownloaded;

  auto const & countryFile = GetCountryFile(countryId);
  if (GetRemoteSize(countryFile) == 0)
    return Status::EUnknown;

  if (localFile->GetVersion() != GetCurrentDataVersion())
    return Status::EOnDiskOutOfDate;
  return Status::EOnDisk;
}

void Storage::SaveDownloadQueue()
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());

  ostringstream download;
  ostringstream update;
  for (auto const & item : m_downloader->GetQueue())
  {
    auto & ss = item.GetFileType() == MapFileType::Diff ? update : download;
    ss << (ss.str().empty() ? "" : ";") << item.GetCountryId();
  }

  settings::Set(kDownloadQueueKey, download.str());
  settings::Set(kUpdateQueueKey, update.str());
}

void Storage::RestoreDownloadQueue()
{
  string download, update;
  if (!settings::Get(kDownloadQueueKey, download) && !settings::Get(kUpdateQueueKey, update))
    return;

  auto parse = [this](string const & token, bool isUpdate) {
    if (token.empty())
      return;

    for (strings::SimpleTokenizer iter(token, ";"); iter; ++iter)
    {
      auto const diffIt = find_if(m_notAppliedDiffs.cbegin(), m_notAppliedDiffs.cend(),
                                  [this, &iter](LocalCountryFile const & localDiff) {
                                    return *iter == FindCountryIdByFile(localDiff.GetCountryName());
                                  });

      if (diffIt == m_notAppliedDiffs.end())
        DownloadNode(*iter, isUpdate);
    }
  };

  parse(download, false /* isUpdate */);
  parse(update, true /* isUpdate */);
}

void Storage::DownloadCountry(CountryId const & countryId, MapFileType type)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());

  if (IsCountryInQueue(countryId) || IsDiffApplyingInProgressToCountry(countryId))
    return;

  m_failedCountries.erase(countryId);
  auto const countryFile = GetCountryFile(countryId);
  // If it's not even possible to prepare directory for files before
  // downloading, then mark this country as failed and switch to next
  // country.
  if (!PreparePlaceForCountryFiles(GetCurrentDataVersion(), m_dataDir, countryFile))
  {
    OnMapDownloadFinished(countryId, DownloadStatus::Failed, type);
    return;
  }

  if (IsFileDownloaded(GetFileDownloadPath(countryId, type), type))
  {
    OnMapDownloadFinished(countryId, DownloadStatus::Completed, type);
    return;
  }

  QueuedCountry queuedCountry(countryFile, countryId, type, GetCurrentDataVersion(), m_dataDir,
                              m_diffsDataSource);
  queuedCountry.SetOnFinishCallback(bind(&Storage::OnMapFileDownloadFinished, this, _1, _2));
  queuedCountry.SetOnProgressCallback(bind(&Storage::OnMapFileDownloadProgress, this, _1, _2));

  m_downloader->DownloadMapFile(queuedCountry);
}

void Storage::DeleteCountry(CountryId const & countryId, MapFileType type)
{
  ASSERT(m_willDelete != nullptr, ("Storage::Init wasn't called"));

  LocalFilePtr localFile = GetLatestLocalFile(countryId);
  bool const deferredDelete = m_willDelete(countryId, localFile);
  DeleteCountryFiles(countryId, type, deferredDelete);
  DeleteCountryFilesFromDownloader(countryId);
  m_diffsDataSource->RemoveDiffForCountry(countryId);

  NotifyStatusChangedForHierarchy(countryId);

  m_downloader->Resume();
}

void Storage::DeleteCustomCountryVersion(LocalCountryFile const & localFile)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());

  CountryFile const countryFile = localFile.GetCountryFile();
  DeleteFromDiskWithIndexes(localFile, MapFileType::Map);
  DeleteFromDiskWithIndexes(localFile, MapFileType::Diff);

  {
    auto it = m_localFilesForFakeCountries.find(countryFile);
    if (it != m_localFilesForFakeCountries.end())
    {
      m_localFilesForFakeCountries.erase(it);
      return;
    }
  }

  CountryId const countryId = FindCountryIdByFile(countryFile.GetName());
  if (!IsLeaf(countryId))
  {
    LOG(LERROR, ("Removed files for an unknown country:", localFile));
    return;
  }
}

void Storage::NotifyStatusChanged(CountryId const & countryId)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());

  for (CountryObservers const & observer : m_observers)
    observer.m_changeCountryFn(countryId);
}

void Storage::NotifyStatusChangedForHierarchy(CountryId const & countryId)
{
  // Notification status changing for a leaf in country tree.
  NotifyStatusChanged(countryId);

  // Notification status changing for ancestors in country tree.
  ForEachAncestorExceptForTheRoot(countryId,
                                  [&](CountryId const & parentId, CountryTree::Node const &) {
                                    NotifyStatusChanged(parentId);
                                  });
}

bool Storage::IsDownloadInProgress() const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());

  return !m_downloader->GetQueue().empty();
}

CountryId Storage::GetCurrentDownloadingCountryId() const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());

  return IsDownloadInProgress() ? m_downloader->GetQueue().front().GetCountryId()
                                : storage::CountryId();
}

void Storage::LoadCountriesFile(string const & pathToCountriesFile)
{
  if (m_countries.IsEmpty())
  {
    m_currentVersion =
        LoadCountriesFromFile(pathToCountriesFile, m_countries, m_affiliations,
                              m_countryNameSynonyms, m_mwmTopCityGeoIds, m_mwmTopCountryGeoIds);
    LOG_SHORT(LINFO, ("Loaded countries list for version:", m_currentVersion));
    if (m_currentVersion < 0)
      LOG(LERROR, ("Can't load countries file", pathToCountriesFile));
  }
}

int Storage::Subscribe(ChangeCountryFunction const & change, ProgressFunction const & progress)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());

  CountryObservers obs;

  obs.m_changeCountryFn = change;
  obs.m_progressFn = progress;
  obs.m_slotId = ++m_currentSlotId;

  m_observers.push_back(obs);

  return obs.m_slotId;
}

void Storage::Unsubscribe(int slotId)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());

  for (auto i = m_observers.begin(); i != m_observers.end(); ++i)
  {
    if (i->m_slotId == slotId)
    {
      m_observers.erase(i);
      return;
    }
  }
}

void Storage::OnMapFileDownloadFinished(QueuedCountry const & queuedCountry, DownloadStatus status)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());

  OnMapDownloadFinished(queuedCountry.GetCountryId(), status, queuedCountry.GetFileType());
}

void Storage::ReportProgress(CountryId const & countryId, Progress const & p)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  for (CountryObservers const & o : m_observers)
    o.m_progressFn(countryId, p);
}

void Storage::ReportProgressForHierarchy(CountryId const & countryId, Progress const & leafProgress)
{
  // Reporting progress for a leaf in country tree.
  ReportProgress(countryId, leafProgress);

  // Reporting progress for the parents of the leaf with |countryId|.
  CountriesSet setQueue;
  GetQueuedCountries(m_downloader->GetQueue(), setQueue);

  auto calcProgress = [&](CountryId const & parentId, CountryTree::Node const & parentNode) {
    CountriesVec descendants;
    parentNode.ForEachDescendant([&descendants](CountryTree::Node const & container) {
      descendants.push_back(container.Value().Name());
    });

    Progress localAndRemoteBytes =
        CalculateProgress(countryId, descendants, leafProgress, setQueue);
    ReportProgress(parentId, localAndRemoteBytes);
  };

  ForEachAncestorExceptForTheRoot(countryId, calcProgress);
}

void Storage::OnMapFileDownloadProgress(QueuedCountry const & queuedCountry,
                                        Progress const & progress)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());

  if (m_observers.empty())
    return;

  ReportProgressForHierarchy(queuedCountry.GetCountryId(), progress);
}

void Storage::RegisterDownloadedFiles(CountryId const & countryId, MapFileType type)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());

  auto const fn = [this, countryId, type](bool isSuccess) {
    CHECK_THREAD_CHECKER(m_threadChecker, ());

    LOG(LINFO, ("Registering downloaded file:", countryId, type, "; success:", isSuccess));

    if (!isSuccess)
    {
      m_justDownloaded.erase(countryId);
      OnMapDownloadFailed(countryId);
      return;
    }

    LocalFilePtr localFile = GetLocalFile(countryId, GetCurrentDataVersion());
    ASSERT(localFile, ());
    DeleteCountryIndexes(*localFile);
    m_didDownload(countryId, localFile);

    SaveDownloadQueue();

    NotifyStatusChangedForHierarchy(countryId);
  };

  if (type == MapFileType::Diff)
  {
    ApplyDiff(countryId, fn);
    return;
  }

  CountryFile const countryFile = GetCountryFile(countryId);
  LocalFilePtr localFile = GetLocalFile(countryId, GetCurrentDataVersion());
  if (!localFile)
    localFile = PreparePlaceForCountryFiles(GetCurrentDataVersion(), m_dataDir, countryFile);
  if (!localFile)
  {
    LOG(LERROR, ("Local file data structure can't be prepared for downloaded file(", countryFile,
                 type, ")."));
    fn(false /* isSuccess */);
    return;
  }

  string const path = GetFileDownloadPath(countryId, type);
  if (!base::RenameFileX(path, localFile->GetPath(type)))
  {
    localFile->DeleteFromDisk(type);
    fn(false);
    return;
  }

  static string const kSourceKey = "map";
  if (m_integrityValidationEnabled && !ValidateIntegrity(localFile, countryId, kSourceKey))
  {
    base::DeleteFileX(localFile->GetPath(MapFileType::Map));
    fn(false /* isSuccess */);
    return;
  }

  RegisterCountryFiles(localFile);
  fn(true);
}

void Storage::OnMapDownloadFinished(CountryId const & countryId, DownloadStatus status,
                                    MapFileType type)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  ASSERT(m_didDownload != nullptr, ("Storage::Init wasn't called"));

  SendStatisticsAfterDownloading(countryId, status, type, GetCurrentDataVersion(), m_localFiles);

  if (status != DownloadStatus::Completed)
  {
    if (status == DownloadStatus::FileNotFound && type == MapFileType::Diff)
    {
      m_diffsDataSource->AbortDiffScheme();
      NotifyStatusChanged(GetRootId());
    }

    OnMapDownloadFailed(countryId);
    return;
  }

  m_justDownloaded.insert(countryId);
  RegisterDownloadedFiles(countryId, type);
}

CountryId Storage::FindCountryIdByFile(string const & name) const
{
  // @TODO(bykoianko) Probably it's worth to check here if name represent a node in the tree.
  return CountryId(name);
}

CountriesVec Storage::FindAllIndexesByFile(CountryId const & name) const
{
  // @TODO(bykoianko) This method should be rewritten. At list now name and the param of Find
  // have different types: string and CountryId.
  CountriesVec result;
  if (m_countries.FindFirst(name))
    result.push_back(name);
  return result;
}

void Storage::GetOutdatedCountries(vector<Country const *> & countries) const
{
  for (auto const & p : m_localFiles)
  {
    CountryId const & countryId = p.first;
    string const name = GetCountryFile(countryId).GetName();
    LocalFilePtr file = GetLatestLocalFile(countryId);
    if (file && file->GetVersion() != GetCurrentDataVersion() && name != WORLD_COASTS_FILE_NAME
        && name != WORLD_FILE_NAME)
    {
      countries.push_back(&CountryLeafByCountryId(countryId));
    }
  }
}

bool Storage::IsCountryInQueue(CountryId const & countryId) const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());

  auto const & queue = m_downloader->GetQueue();
  return find(queue.cbegin(), queue.cend(), countryId) != queue.cend();
}

bool Storage::IsCountryFirstInQueue(CountryId const & countryId) const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());

  auto const & queue = m_downloader->GetQueue();
  return !queue.empty() && queue.front().GetCountryId() == countryId;
}

bool Storage::IsDiffApplyingInProgressToCountry(CountryId const & countryId) const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());

  return m_diffsBeingApplied.find(countryId) != m_diffsBeingApplied.cend();
}

void Storage::SetLocale(string const & locale) { m_countryNameGetter.SetLocale(locale); }
string Storage::GetLocale() const { return m_countryNameGetter.GetLocale(); }
void Storage::SetDownloaderForTesting(unique_ptr<MapFilesDownloader> downloader)
{
  if (m_downloader)
  {
    m_downloader->UnsubscribeAll();
    m_downloader->Clear();
  }

  m_downloader = move(downloader);
  m_downloader->Subscribe(this);
  m_downloader->SetDownloadingPolicy(m_downloadingPolicy);
}

void Storage::SetEnabledIntegrityValidationForTesting(bool enabled)
{
  m_integrityValidationEnabled = enabled;
}

void Storage::SetCurrentDataVersionForTesting(int64_t currentVersion)
{
  m_currentVersion = currentVersion;
}

void Storage::SetDownloadingServersForTesting(vector<string> const & downloadingUrls)
{
  m_downloader->SetServersList(downloadingUrls);
}

void Storage::SetLocaleForTesting(string const & jsonBuffer, string const & locale)
{
  m_countryNameGetter.SetLocaleForTesting(jsonBuffer, locale);
}

LocalFilePtr Storage::GetLocalFile(CountryId const & countryId, int64_t version) const
{
  auto const it = m_localFiles.find(countryId);
  if (it == m_localFiles.end() || it->second.empty())
    return LocalFilePtr();

  for (auto const & file : it->second)
  {
    if (file->GetVersion() == version)
      return file;
  }
  return LocalFilePtr();
}

void Storage::RegisterCountryFiles(LocalFilePtr localFile)
{
  CHECK(localFile, ());
  localFile->SyncWithDisk();

  for (auto const & countryId : FindAllIndexesByFile(localFile->GetCountryName()))
  {
    LocalFilePtr existingFile = GetLocalFile(countryId, localFile->GetVersion());
    if (existingFile)
      ASSERT_EQUAL(localFile.get(), existingFile.get(), ());
    else
      m_localFiles[countryId].push_front(localFile);
  }
}

void Storage::RegisterCountryFiles(CountryId const & countryId, string const & directory,
                                   int64_t version)
{
  LocalFilePtr localFile = GetLocalFile(countryId, version);
  if (localFile)
    return;

  CountryFile const & countryFile = GetCountryFile(countryId);
  localFile = make_shared<LocalCountryFile>(directory, countryFile, version);
  RegisterCountryFiles(localFile);
}

void Storage::RegisterFakeCountryFiles(platform::LocalCountryFile const & localFile)
{
  LocalFilePtr fakeCountryLocalFile = make_shared<LocalCountryFile>(localFile);
  fakeCountryLocalFile->SyncWithDisk();
  m_localFilesForFakeCountries[fakeCountryLocalFile->GetCountryFile()] = fakeCountryLocalFile;
}

void Storage::DeleteCountryFiles(CountryId const & countryId, MapFileType type, bool deferredDelete)
{
  auto const it = m_localFiles.find(countryId);
  if (it == m_localFiles.end())
    return;

  if (deferredDelete)
  {
    m_localFiles.erase(countryId);
    return;
  }

  auto & localFiles = it->second;
  for (auto & localFile : localFiles)
  {
    DeleteFromDiskWithIndexes(*localFile, type);
    localFile->SyncWithDisk();
    if (!localFile->HasFiles())
      localFile.reset();
  }
  auto isNull = [](LocalFilePtr const & localFile) { return !localFile; };
  localFiles.remove_if(isNull);
  if (localFiles.empty())
    m_localFiles.erase(countryId);
}

bool Storage::DeleteCountryFilesFromDownloader(CountryId const & countryId)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());

  if (IsDiffApplyingInProgressToCountry(countryId))
    m_diffsBeingApplied[countryId]->Cancel();

  m_downloader->Remove(countryId);
  DeleteDownloaderFilesForCountry(GetCurrentDataVersion(), m_dataDir, GetCountryFile(countryId));
  SaveDownloadQueue();

  return true;
}

string Storage::GetFileDownloadPath(CountryId const & countryId, MapFileType type) const
{
  return platform::GetFileDownloadPath(GetCurrentDataVersion(), m_dataDir,
                                       GetCountryFile(countryId), type);
}

bool Storage::CheckFailedCountries(CountriesVec const & countries) const
{
  for (auto const & country : countries)
  {
    if (m_failedCountries.count(country))
      return true;
  }
  return false;
}

CountryId const Storage::GetRootId() const { return m_countries.GetRoot().Value().Name(); }
void Storage::GetChildren(CountryId const & parent, CountriesVec & childIds) const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());

  CountryTree::Node const * const parentNode = m_countries.FindFirst(parent);
  if (parentNode == nullptr)
  {
    ASSERT(false, ("CountryId =", parent, "not found in m_countries."));
    return;
  }

  size_t const childrenCount = parentNode->ChildrenCount();
  childIds.clear();
  childIds.reserve(childrenCount);
  for (size_t i = 0; i < childrenCount; ++i)
    childIds.emplace_back(parentNode->Child(i).Value().Name());
}

void Storage::GetLocalRealMaps(CountriesVec & localMaps) const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());

  localMaps.clear();
  localMaps.reserve(m_localFiles.size());

  for (auto const & keyValue : m_localFiles)
    localMaps.push_back(keyValue.first);
}

void Storage::GetChildrenInGroups(CountryId const & parent, CountriesVec & downloadedChildren,
                                  CountriesVec & availChildren, bool keepAvailableChildren) const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());

  CountryTree::Node const * const parentNode = m_countries.FindFirst(parent);
  if (parentNode == nullptr)
  {
    ASSERT(false, ("CountryId =", parent, "not found in m_countries."));
    return;
  }

  downloadedChildren.clear();
  availChildren.clear();

  // Vector of disputed territories which are downloaded but the other maps
  // in their group are not downloaded. Only mwm in subtree with root == |parent|
  // are taken into account.
  CountriesVec disputedTerritoriesWithoutSiblings;
  // All disputed territories in subtree with root == |parent|.
  CountriesVec allDisputedTerritories;
  parentNode->ForEachChild([&](CountryTree::Node const & childNode) {
    vector<pair<CountryId, NodeStatus>> disputedTerritoriesAndStatus;
    StatusAndError const childStatus = GetNodeStatusInfo(childNode, disputedTerritoriesAndStatus,
                                                         true /* isDisputedTerritoriesCounted */);

    CountryId const & childValue = childNode.Value().Name();
    ASSERT_NOT_EQUAL(childStatus.status, NodeStatus::Undefined, ());
    for (auto const & disputed : disputedTerritoriesAndStatus)
      allDisputedTerritories.push_back(disputed.first);

    if (childStatus.status == NodeStatus::NotDownloaded)
    {
      availChildren.push_back(childValue);
      for (auto const & disputed : disputedTerritoriesAndStatus)
      {
        if (disputed.second != NodeStatus::NotDownloaded)
          disputedTerritoriesWithoutSiblings.push_back(disputed.first);
      }
    }
    else
    {
      downloadedChildren.push_back(childValue);
      if (keepAvailableChildren)
        availChildren.push_back(childValue);
    }
  });

  CountriesVec uniqueDisputed(disputedTerritoriesWithoutSiblings.begin(),
                              disputedTerritoriesWithoutSiblings.end());
  base::SortUnique(uniqueDisputed);

  for (auto const & countryId : uniqueDisputed)
  {
    // Checks that the number of disputed territories with |countryId| in subtree with root ==
    // |parent|
    // is equal to the number of disputed territories with out downloaded sibling
    // with |countryId| in subtree with root == |parent|.
    if (count(disputedTerritoriesWithoutSiblings.begin(), disputedTerritoriesWithoutSiblings.end(),
              countryId) ==
        count(allDisputedTerritories.begin(), allDisputedTerritories.end(), countryId))
    {
      // |countryId| is downloaded without any other map in its group.
      downloadedChildren.push_back(countryId);
    }
  }
}

bool Storage::IsNodeDownloaded(CountryId const & countryId) const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());

  for (auto const & localeMap : m_localFiles)
  {
    if (countryId == localeMap.first)
      return true;
  }
  return false;
}

bool Storage::HasLatestVersion(CountryId const & countryId) const
{
  return CountryStatusEx(countryId) == Status::EOnDisk;
}

int64_t Storage::GetVersion(CountryId const & countryId) const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());

  auto const localMap = GetLatestLocalFile(countryId);
  if (localMap == nullptr)
    return 0;

  return localMap->GetVersion();
}

void Storage::DownloadNode(CountryId const & countryId, bool isUpdate /* = false */)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());

  LOG(LINFO, ("Downloading", countryId));

  CountryTree::Node const * const node = m_countries.FindFirst(countryId);

  if (!node)
    return;

  if (GetNodeStatus(*node).status == NodeStatus::OnDisk)
    return;

  auto downloadAction = [this, isUpdate](CountryTree::Node const & descendantNode) {
    if (descendantNode.ChildrenCount() == 0 &&
        GetNodeStatus(descendantNode).status != NodeStatus::OnDisk)
    {
      auto const countryId = descendantNode.Value().Name();

      DownloadCountry(countryId, isUpdate ? MapFileType::Diff : MapFileType::Map);
    }
  };

  node->ForEachInSubtree(downloadAction);
}

void Storage::DeleteNode(CountryId const & countryId)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());

  CountryTree::Node const * const node = m_countries.FindFirst(countryId);

  if (!node)
    return;

  auto deleteAction = [this](CountryTree::Node const & descendantNode) {
    bool onDisk = m_localFiles.find(descendantNode.Value().Name()) != m_localFiles.end();
    if (descendantNode.ChildrenCount() == 0 && onDisk)
      this->DeleteCountry(descendantNode.Value().Name(), MapFileType::Map);
  };
  node->ForEachInSubtree(deleteAction);
}

StatusAndError Storage::GetNodeStatus(CountryTree::Node const & node) const
{
  vector<pair<CountryId, NodeStatus>> disputedTerritories;
  return GetNodeStatusInfo(node, disputedTerritories, false /* isDisputedTerritoriesCounted */);
}

bool Storage::IsDisputed(CountryTree::Node const & node) const
{
  vector<CountryTree::Node const *> found;
  m_countries.Find(node.Value().Name(), found);
  return found.size() > 1;
}

void Storage::CalcMaxMwmSizeBytes()
{
  m_maxMwmSizeBytes = 0;
  m_countries.GetRoot().ForEachInSubtree([&](CountryTree::Node const & node) {
    if (node.ChildrenCount() == 0)
    {
      MwmSize mwmSizeBytes = node.Value().GetSubtreeMwmSizeBytes();
      if (mwmSizeBytes > m_maxMwmSizeBytes)
        m_maxMwmSizeBytes = mwmSizeBytes;
    }
  });
}

void Storage::LoadDiffScheme()
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  diffs::LocalMapsInfo localMapsInfo;
  auto const currentVersion = GetCurrentDataVersion();
  localMapsInfo.m_currentDataVersion = currentVersion;

  CountriesVec localMaps;
  GetLocalRealMaps(localMaps);
  for (auto const & countryId : localMaps)
  {
    auto const localFile = GetLatestLocalFile(countryId);
    auto const mapVersion = localFile->GetVersion();
    if (mapVersion != currentVersion && mapVersion > 0)
      localMapsInfo.m_localMaps.emplace(localFile->GetCountryName(), mapVersion);
  }

  if (localMapsInfo.m_localMaps.empty())
  {
    m_diffsDataSource->AbortDiffScheme();
    return;
  }

  diffs::Loader::Load(move(localMapsInfo), [this](diffs::NameDiffInfoMap && diffs)
  {
    OnDiffStatusReceived(move(diffs));
  });
}

void Storage::ApplyDiff(CountryId const & countryId, function<void(bool isSuccess)> const & fn)
{
  LOG(LINFO, ("Applying diff for", countryId));

  if (IsDiffApplyingInProgressToCountry(countryId))
    return;

  auto const diffLocalFile = PreparePlaceForCountryFiles(GetCurrentDataVersion(), m_dataDir,
                                                         GetCountryFile(countryId));
  uint64_t version;
  if (!diffLocalFile || !m_diffsDataSource->VersionFor(countryId, version))
  {
    fn(false);
    return;
  }

  auto const emplaceResult =
      m_diffsBeingApplied.emplace(countryId, std::make_unique<base::Cancellable>());
  CHECK_EQUAL(emplaceResult.second, true, ());

  NotifyStatusChangedForHierarchy(countryId);

  diffs::ApplyDiffParams params;
  params.m_diffFile = diffLocalFile;
  params.m_diffReadyPath = GetFileDownloadPath(countryId, MapFileType::Diff);
  params.m_oldMwmFile = GetLocalFile(countryId, version);

  LocalFilePtr & diffFile = params.m_diffFile;
  diffs::ApplyDiff(
      move(params), *emplaceResult.first->second,
      [this, fn, countryId, diffFile](DiffApplicationResult result) {
        CHECK_THREAD_CHECKER(m_threadChecker, ());
        static string const kSourceKey = "diff";
        if (result == DiffApplicationResult::Ok && m_integrityValidationEnabled &&
            !ValidateIntegrity(diffFile, diffFile->GetCountryName(), kSourceKey))
        {
          GetPlatform().RunTask(Platform::Thread::File,
            [path = diffFile->GetPath(MapFileType::Map)] { base::DeleteFileX(path); });
          result = DiffApplicationResult::Failed;
        }

        if (m_diffsBeingApplied[countryId]->IsCancelled() && result == DiffApplicationResult::Ok)
          result = DiffApplicationResult::Cancelled;

        LOG(LINFO, ("Diff application result for", countryId, ":", result));

        m_diffsBeingApplied.erase(countryId);
        switch (result)
        {
        case DiffApplicationResult::Ok:
        {
          RegisterCountryFiles(diffFile);
          Platform::DisableBackupForFile(diffFile->GetPath(MapFileType::Map));
          m_diffsDataSource->MarkAsApplied(countryId);
          fn(true);
          break;
        }
        case DiffApplicationResult::Cancelled:
        {
          break;
        }
        case DiffApplicationResult::Failed:
        {
          m_diffsDataSource->RemoveDiffForCountry(countryId);
          fn(false);
          break;
        }
        }

        if (m_diffsBeingApplied.empty() && m_downloader->GetQueue().empty())
          OnFinishDownloading();
      });
}

bool Storage::IsPossibleToAutoupdate() const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  if (m_diffsDataSource->GetStatus() != diffs::Status::Available)
    return false;

  auto const currentVersion = GetCurrentDataVersion();
  CountriesVec localMaps;
  GetLocalRealMaps(localMaps);
  for (auto const & countryId : localMaps)
  {
    auto const localFile = GetLatestLocalFile(countryId);
    auto const mapVersion = localFile->GetVersion();
    if (mapVersion != currentVersion && mapVersion > 0 &&
        !m_diffsDataSource->HasDiffFor(localFile->GetCountryName()))
    {
      return false;
    }
  }

  return true;
}

void Storage::SetStartDownloadingCallback(StartDownloadingCallback const & cb)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());

  m_startDownloadingCallback = cb;
}

void Storage::OnStartDownloading()
{
  if (m_startDownloadingCallback)
    m_startDownloadingCallback();
}

void Storage::OnFinishDownloading()
{
  // When downloading is finished but diffs applying in progress
  // it will be called from ApplyDiff callback.
  if (!m_diffsBeingApplied.empty())
    return;

  m_justDownloaded.clear();

  m_downloadingPolicy->ScheduleRetry(m_failedCountries, [this](CountriesSet const & needReload) {
    for (auto const & country : needReload)
    {
      NodeStatuses status;
      GetNodeStatuses(country, status);
      if (status.m_error == NodeErrorCode::NoInetConnection)
        RetryDownloadNode(country);
    }
  });
  CountriesVec localMaps;
  GetLocalRealMaps(localMaps);
  GetPlatform().GetMarketingService().SendPushWooshTag(marketing::kMapListing, localMaps);
  if (!localMaps.empty())
    GetPlatform().GetMarketingService().SendPushWooshTag(marketing::kMapDownloadDiscovered);
  return;
}

void Storage::OnCountryInQueue(CountryId const & id)
{
  NotifyStatusChangedForHierarchy(id);
  SaveDownloadQueue();
}

void Storage::OnStartDownloadingCountry(CountryId const & id)
{
  NotifyStatusChangedForHierarchy(id);
}

void Storage::OnDiffStatusReceived(diffs::NameDiffInfoMap && diffs)
{
  m_diffsDataSource->SetDiffInfo(move(diffs));
  if (m_diffsDataSource->GetStatus() == diffs::Status::Available)
  {
    for (auto const & localDiff : m_notAppliedDiffs)
    {
      auto const countryId = FindCountryIdByFile(localDiff.GetCountryName());

      if (m_diffsDataSource->HasDiffFor(countryId))
        UpdateNode(countryId);
      else
        localDiff.DeleteFromDisk(MapFileType::Diff);
    }

    m_notAppliedDiffs.clear();
  }
}

StatusAndError Storage::GetNodeStatusInfo(CountryTree::Node const & node,
                                          vector<pair<CountryId, NodeStatus>> & disputedTerritories,
                                          bool isDisputedTerritoriesCounted) const
{
  // Leaf node status.
  if (node.ChildrenCount() == 0)
  {
    StatusAndError const statusAndError = ParseStatus(CountryStatusEx(node.Value().Name()));
    if (IsDisputed(node))
      disputedTerritories.push_back(make_pair(node.Value().Name(), statusAndError.status));
    return statusAndError;
  }

  // Group node status.
  NodeStatus result = NodeStatus::NotDownloaded;
  bool allOnDisk = true;

  auto groupStatusCalculator = [&](CountryTree::Node const & nodeInSubtree) {
    StatusAndError const statusAndError =
        ParseStatus(CountryStatusEx(nodeInSubtree.Value().Name()));

    if (IsDisputed(nodeInSubtree) && isDisputedTerritoriesCounted)
    {
      disputedTerritories.push_back(make_pair(nodeInSubtree.Value().Name(), statusAndError.status));
      return;
    }

    if (result == NodeStatus::Downloading || nodeInSubtree.ChildrenCount() != 0)
      return;

    if (statusAndError.status != NodeStatus::OnDisk)
      allOnDisk = false;

    if (static_cast<size_t>(statusAndError.status) < static_cast<size_t>(result))
      result = statusAndError.status;
  };

  node.ForEachDescendant(groupStatusCalculator);
  if (allOnDisk)
    return ParseStatus(Status::EOnDisk);
  if (result == NodeStatus::OnDisk)
    return {NodeStatus::Partly, NodeErrorCode::NoError};

  ASSERT_NOT_EQUAL(result, NodeStatus::Undefined, ());
  return {result, NodeErrorCode::NoError};
}

void Storage::GetNodeAttrs(CountryId const & countryId, NodeAttrs & nodeAttrs) const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());

  vector<CountryTree::Node const *> nodes;
  m_countries.Find(countryId, nodes);
  CHECK(!nodes.empty(), (countryId));
  // If nodes.size() > 1 countryId corresponds to a disputed territories.
  // In that case it's guaranteed that most of attributes are equal for
  // each element of nodes. See Country class description for further details.
  CountryTree::Node const * const node = nodes[0];

  Country const & nodeValue = node->Value();
  nodeAttrs.m_mwmCounter = nodeValue.GetSubtreeMwmCounter();
  nodeAttrs.m_mwmSize = nodeValue.GetSubtreeMwmSizeBytes();
  StatusAndError statusAndErr = GetNodeStatus(*node);
  nodeAttrs.m_status = statusAndErr.status;
  nodeAttrs.m_error = statusAndErr.error;
  nodeAttrs.m_nodeLocalName = m_countryNameGetter(countryId);
  nodeAttrs.m_nodeLocalDescription =
      m_countryNameGetter.Get(countryId + LOCALIZATION_DESCRIPTION_SUFFIX);

  // Progress.
  if (nodeAttrs.m_status == NodeStatus::OnDisk)
  {
    // Group or leaf node is on disk and up to date.
    MwmSize const subTreeSizeBytes = node->Value().GetSubtreeMwmSizeBytes();
    nodeAttrs.m_downloadingProgress.first = subTreeSizeBytes;
    nodeAttrs.m_downloadingProgress.second = subTreeSizeBytes;
  }
  else
  {
    CountriesVec subtree;
    node->ForEachInSubtree(
        [&subtree](CountryTree::Node const & d) { subtree.push_back(d.Value().Name()); });
    CountryId const & downloadingMwm =
        IsDownloadInProgress() ? GetCurrentDownloadingCountryId() : kInvalidCountryId;
    Progress downloadingMwmProgress(0, 0);
    if (!m_downloader->IsIdle())
    {
      downloadingMwmProgress = m_downloader->GetDownloadingProgress();
      // If we don't know estimated file size then we ignore its progress.
      if (downloadingMwmProgress.second == -1)
        downloadingMwmProgress = {};
    }

    CountriesSet setQueue;
    GetQueuedCountries(m_downloader->GetQueue(), setQueue);
    nodeAttrs.m_downloadingProgress =
        CalculateProgress(downloadingMwm, subtree, downloadingMwmProgress, setQueue);
  }

  // Local mwm information and information about downloading mwms.
  nodeAttrs.m_localMwmCounter = 0;
  nodeAttrs.m_localMwmSize = 0;
  nodeAttrs.m_downloadingMwmCounter = 0;
  nodeAttrs.m_downloadingMwmSize = 0;
  CountriesSet visitedLocalNodes;
  node->ForEachInSubtree([this, &nodeAttrs, &visitedLocalNodes](CountryTree::Node const & d) {
    CountryId const countryId = d.Value().Name();
    if (visitedLocalNodes.count(countryId) != 0)
      return;
    visitedLocalNodes.insert(countryId);

    // Downloading mwm information.
    StatusAndError const statusAndErr = GetNodeStatus(d);
    ASSERT_NOT_EQUAL(statusAndErr.status, NodeStatus::Undefined, ());
    if (statusAndErr.status != NodeStatus::NotDownloaded &&
        statusAndErr.status != NodeStatus::Partly && d.ChildrenCount() == 0)
    {
      nodeAttrs.m_downloadingMwmCounter += 1;
      nodeAttrs.m_downloadingMwmSize += d.Value().GetSubtreeMwmSizeBytes();
    }

    // Local mwm information.
    LocalFilePtr const localFile = GetLatestLocalFile(countryId);
    if (localFile == nullptr)
      return;

    nodeAttrs.m_localMwmCounter += 1;
    nodeAttrs.m_localMwmSize += localFile->GetSize(MapFileType::Map);
  });
  nodeAttrs.m_present = m_localFiles.find(countryId) != m_localFiles.end();

  // Parents information.
  nodeAttrs.m_parentInfo.clear();
  nodeAttrs.m_parentInfo.reserve(nodes.size());
  for (auto const & n : nodes)
  {
    Country const & nValue = n->Value();
    CountryIdAndName countryIdAndName;
    countryIdAndName.m_id = nValue.GetParent();
    if (countryIdAndName.m_id.empty())  // The root case.
      countryIdAndName.m_localName = string();
    else
      countryIdAndName.m_localName = m_countryNameGetter(countryIdAndName.m_id);
    nodeAttrs.m_parentInfo.emplace_back(move(countryIdAndName));
  }
  // Parents country.
  nodeAttrs.m_topmostParentInfo.clear();
  ForEachAncestorExceptForTheRoot(
      nodes, [&](CountryId const & ancestorId, CountryTree::Node const & node) {
        if (node.Value().GetParent() == GetRootId())
          nodeAttrs.m_topmostParentInfo.push_back({ancestorId, m_countryNameGetter(ancestorId)});
      });
}

void Storage::GetNodeStatuses(CountryId const & countryId, NodeStatuses & nodeStatuses) const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());

  CountryTree::Node const * const node = m_countries.FindFirst(countryId);
  CHECK(node, (countryId));

  StatusAndError statusAndErr = GetNodeStatus(*node);
  nodeStatuses.m_status = statusAndErr.status;
  nodeStatuses.m_error = statusAndErr.error;
  nodeStatuses.m_groupNode = (node->ChildrenCount() != 0);
}

void Storage::SetCallbackForClickOnDownloadMap(DownloadFn & downloadFn)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());

  m_downloadMapOnTheMap = downloadFn;
}

void Storage::DoClickOnDownloadMap(CountryId const & countryId)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());

  if (m_downloadMapOnTheMap)
    m_downloadMapOnTheMap(countryId);
}

Progress Storage::CalculateProgress(CountryId const & downloadingMwm, CountriesVec const & mwms,
                                    Progress const & downloadingMwmProgress,
                                    CountriesSet const & mwmsInQueue) const
{
  // Function calculates progress correctly ONLY if |downloadingMwm| is leaf.

  Progress localAndRemoteBytes = {};

  for (auto const & d : mwms)
  {
    if (downloadingMwm == d && downloadingMwm != kInvalidCountryId)
    {
      localAndRemoteBytes.first += downloadingMwmProgress.first;
      localAndRemoteBytes.second += GetRemoteSize(GetCountryFile(d));
    }
    else if (mwmsInQueue.count(d) != 0)
    {
      localAndRemoteBytes.second += GetRemoteSize(GetCountryFile(d));
    }
    else if (m_justDownloaded.count(d) != 0)
    {
      MwmSize const localCountryFileSz = GetRemoteSize(GetCountryFile(d));
      localAndRemoteBytes.first += localCountryFileSz;
      localAndRemoteBytes.second += localCountryFileSz;
    }
  }

  return localAndRemoteBytes;
}

void Storage::UpdateNode(CountryId const & countryId)
{
  ForEachInSubtree(countryId, [this](CountryId const & descendantId, bool groupNode) {
    if (!groupNode && m_localFiles.find(descendantId) != m_localFiles.end())
      this->DownloadNode(descendantId, true /* isUpdate */);
  });
}

void Storage::CancelDownloadNode(CountryId const & countryId)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());

  LOG(LINFO, ("Cancelling the downloading of", countryId));

  CountriesSet setQueue;
  GetQueuedCountries(m_downloader->GetQueue(), setQueue);

  ForEachInSubtree(countryId, [&](CountryId const & descendantId, bool /* groupNode */) {
    auto needNotify = false;
    if (setQueue.count(descendantId) != 0)
      needNotify = DeleteCountryFilesFromDownloader(descendantId);

    if (m_failedCountries.count(descendantId) != 0)
    {
      m_failedCountries.erase(descendantId);
      needNotify = true;
    }

    if (needNotify)
      NotifyStatusChangedForHierarchy(countryId);
  });

  m_downloader->Resume();
}

void Storage::RetryDownloadNode(CountryId const & countryId)
{
  ForEachInSubtree(countryId, [this](CountryId const & descendantId, bool groupNode) {
    if (!groupNode && m_failedCountries.count(descendantId) != 0)
    {
      bool const isUpdateRequest = m_diffsDataSource->HasDiffFor(descendantId);
      DownloadNode(descendantId, isUpdateRequest);
    }
  });
}

bool Storage::GetUpdateInfo(CountryId const & countryId, UpdateInfo & updateInfo) const
{
  auto const updateInfoAccumulator = [&updateInfo, this](CountryTree::Node const & node) {
    if (node.ChildrenCount() != 0 || GetNodeStatus(node).status != NodeStatus::OnDiskOutOfDate)
      return;

    updateInfo.m_numberOfMwmFilesToUpdate += 1;  // It's not a group mwm.
    if (m_diffsDataSource->HasDiffFor(node.Value().Name()))
    {
      uint64_t size;
      m_diffsDataSource->SizeToDownloadFor(node.Value().Name(), size);
      updateInfo.m_totalUpdateSizeInBytes += size;
    }
    else
    {
      updateInfo.m_totalUpdateSizeInBytes += node.Value().GetSubtreeMwmSizeBytes();
    }

    LocalAndRemoteSize sizes = CountrySizeInBytes(node.Value().Name());
    updateInfo.m_sizeDifference +=
        static_cast<int64_t>(sizes.second) - static_cast<int64_t>(sizes.first);
  };

  CountryTree::Node const * const node = m_countries.FindFirst(countryId);
  if (!node)
  {
    ASSERT(false, ());
    return false;
  }
  updateInfo = UpdateInfo();
  node->ForEachInSubtree(updateInfoAccumulator);
  return true;
}

std::vector<base::GeoObjectId> Storage::GetTopCountryGeoIds(CountryId const & countryId) const
{
  std::vector<base::GeoObjectId> result;

  auto const collector = [this, &result](CountryId const & id, CountryTree::Node const &) {
    auto const it = m_mwmTopCountryGeoIds.find(id);
    if (it != m_mwmTopCountryGeoIds.cend())
      result.insert(result.end(), it->second.cbegin(), it->second.cend());
  };

  ForEachAncestorExceptForTheRoot(countryId, collector);

  return result;
}

void Storage::GetQueuedChildren(CountryId const & parent, CountriesVec & queuedChildren) const
{
  CountryTree::Node const * const node = m_countries.FindFirst(parent);
  if (!node)
  {
    ASSERT(false, ());
    return;
  }

  queuedChildren.clear();
  node->ForEachChild([&queuedChildren, this](CountryTree::Node const & child) {
    NodeStatus status = GetNodeStatus(child).status;
    ASSERT_NOT_EQUAL(status, NodeStatus::Undefined, ());
    if (status == NodeStatus::Downloading || status == NodeStatus::InQueue)
      queuedChildren.push_back(child.Value().Name());
  });
}

void Storage::GetGroupNodePathToRoot(CountryId const & groupNode, CountriesVec & path) const
{
  path.clear();

  vector<CountryTree::Node const *> nodes;
  m_countries.Find(groupNode, nodes);
  if (nodes.empty())
  {
    LOG(LWARNING, ("CountryId =", groupNode, "not found in m_countries."));
    return;
  }

  if (nodes.size() != 1)
  {
    LOG(LWARNING, (groupNode, "Group node can't have more than one parent."));
    return;
  }

  if (nodes[0]->ChildrenCount() == 0)
  {
    LOG(LWARNING, (nodes[0]->Value().Name(), "is a leaf node."));
    return;
  }

  ForEachAncestorExceptForTheRoot(
      nodes, [&path](CountryId const & id, CountryTree::Node const &) { path.push_back(id); });
  path.push_back(m_countries.GetRoot().Value().Name());
}

void Storage::GetTopmostNodesFor(CountryId const & countryId, CountriesVec & nodes,
                                 size_t level) const
{
  nodes.clear();

  vector<CountryTree::Node const *> treeNodes;
  m_countries.Find(countryId, treeNodes);
  if (treeNodes.empty())
  {
    LOG(LWARNING, ("CountryId =", countryId, "not found in m_countries."));
    return;
  }

  nodes.resize(treeNodes.size());
  for (size_t i = 0; i < treeNodes.size(); ++i)
  {
    nodes[i] = countryId;
    CountriesVec path;
    ForEachAncestorExceptForTheRoot(
        {treeNodes[i]},
        [&path](CountryId const & id, CountryTree::Node const &) { path.emplace_back(id); });
    if (!path.empty() && level < path.size())
      nodes[i] = path[path.size() - 1 - level];
  }
}

CountryId const Storage::GetParentIdFor(CountryId const & countryId) const
{
  vector<CountryTree::Node const *> nodes;
  m_countries.Find(countryId, nodes);
  if (nodes.empty())
  {
    LOG(LWARNING, ("CountryId =", countryId, "not found in m_countries."));
    return string();
  }

  if (nodes.size() > 1)
  {
    // Disputed territory. Has multiple parents.
    return string();
  }

  return nodes[0]->Value().GetParent();
}

CountryId const Storage::GetTopmostParentFor(CountryId const & countryId) const
{
  return ::storage::GetTopmostParentFor(m_countries, countryId);
}

MwmSize Storage::GetRemoteSize(CountryFile const & file) const
{
  ASSERT(m_diffsDataSource != nullptr, ());
  return storage::GetRemoteSize(*m_diffsDataSource, file);
}

void Storage::OnMapDownloadFailed(CountryId const & countryId)
{
  m_failedCountries.insert(countryId);
  NotifyStatusChangedForHierarchy(countryId);
}
}  // namespace storage
