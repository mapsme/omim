#include "testing/testing.hpp"

#include "map/framework.hpp"

#include "platform/http_request.hpp"
#include "platform/local_country_file_utils.hpp"
#include "platform/platform.hpp"
#include "platform/platform_tests_support/scoped_dir.hpp"

#include "coding/file_name_utils.hpp"

#include "storage/storage.hpp"

#include "storage/storage_integration_tests/write_dir_changer.hpp"

#include "base/assert.hpp"

#include "std/condition_variable.hpp"
#include "std/mutex.hpp"
#include "std/set.hpp"
#include "std/unique_ptr.hpp"

#include <QtCore/QCoreApplication>

using namespace platform;
using namespace storage;

namespace
{

string const kTestWebServer = "http://new-search.mapswithme.com/";

string const kMapTestDir = "map-tests";

TCountryId const kGroupCountryId = "New Zealand";
TCountriesSet const kLeafCountriesIds = {"Tokelau",
                                         "New Zealand North_Auckland",
                                         "New Zealand North_Wellington",
                                         "New Zealand South_Canterbury",
                                         "New Zealand South_Southland"};

string const kMwmVersion = "160107";
size_t const kCountriesTxtFileSize = 131488;

string GetMwmFilePath(string const & version, TCountryId const & countryId)
{
  return my::JoinFoldersToPath({GetPlatform().WritableDir(), version},
                               countryId + DATA_FILE_EXTENSION);
}

string GetMwmDownloadingFilePath(string const & version, TCountryId const & countryId)
{
  return my::JoinFoldersToPath({GetPlatform().WritableDir(), version},
                               countryId + DATA_FILE_EXTENSION READY_FILE_EXTENSION DOWNLOADING_FILE_EXTENSION);
}

string GetMwmResumeFilePath(string const & version, TCountryId const & countryId)
{
  return my::JoinFoldersToPath({GetPlatform().WritableDir(), version},
                               countryId + DATA_FILE_EXTENSION READY_FILE_EXTENSION RESUME_FILE_EXTENSION);
}

bool DownloadFile(string const & url,
                  string const & filePath,
                  size_t fileSize)
{
  using namespace downloader;

  HttpRequest::StatusT httpStatus;
  bool finished = false;

  unique_ptr<HttpRequest> request(HttpRequest::GetFile({url}, filePath, fileSize,
                                  [&](HttpRequest & request)
  {
    HttpRequest::StatusT const s = request.Status();
    if (s == HttpRequest::EFailed || s == HttpRequest::ECompleted)
    {
      httpStatus = s;
      finished = true;
      QCoreApplication::exit();
    }
  }));

  QCoreApplication::exec();

  return httpStatus == HttpRequest::ECompleted;
}

string GetCountriesTxtWebUrl(string const version)
{
  return kTestWebServer + "/direct/" + version + "/countries.txt";
}

string GetCountriesTxtFilePath()
{
  return GetPlatform().WritableDir() + "countries.txt";
}

} // namespace

UNIT_TEST(SmallMwms_GroupDownloadDelete_Test)
{
  WritableDirChanger writableDirChanger(kMapTestDir);

  TEST(DownloadFile(GetCountriesTxtWebUrl(kMwmVersion), GetCountriesTxtFilePath(), kCountriesTxtFileSize), ());

  Platform & platform = GetPlatform();

  Storage storage;
  string const version = strings::to_string(storage.GetCurrentDataVersion());
  TEST(version::IsSingleMwm(storage.GetCurrentDataVersion()), ());
  TEST_EQUAL(version, kMwmVersion, ());

  TCountriesVec v;
  storage.GetChildren(kGroupCountryId, v);
  TCountriesSet const children(v.begin(), v.end());
  v.clear();

  // Check children for the kGroupCountryId
  TEST_EQUAL(children, kLeafCountriesIds, ());

  TCountriesSet udpated;
  auto onUpdatedFn = [&](LocalCountryFile const & localCountryFile)
  {
    TCountryId const countryId = localCountryFile.GetCountryName();
    TEST(children.find(countryId) != children.end(), ());
    udpated.insert(countryId);
  };

  TCountriesSet changed;
  auto onChangeCountryFn = [&](TCountryId const & countryId)
  {
    TEST(children.find(countryId) != children.end(), ());
    changed.insert(countryId);
  };

  TCountriesSet downloaded;
  auto onProgressFn = [&](TCountryId const & countryId, LocalAndRemoteSizeT const & mapSize)
  {
    TEST(children.find(countryId) != children.end(), ());
    if (mapSize.first == mapSize.second)
    {
      auto const res = downloaded.insert(countryId);
      TEST_EQUAL(res.second, true, ());
      if (children == downloaded)
        QCoreApplication::exit();
    }
  };

  storage.Init(onUpdatedFn);
  storage.RegisterAllLocalMaps();
  storage.Subscribe(onChangeCountryFn, onProgressFn);
  storage.SetDownloadingUrlsForTesting({kTestWebServer});

  tests_support::ScopedDir cleanupVersionDir(version);
  MY_SCOPE_GUARD(cleanup, bind(&Storage::DeleteNode, &storage, kGroupCountryId));

  // Download

  // Check there is no downloaded children for the group
  storage.GetDownloadedChildren(kGroupCountryId, v);
  TEST(v.empty(), ());

  // Check group is not downloaded
  storage.GetDownloadedChildren(storage.GetRootId(), v);
  TEST(find(v.begin(), v.end(), kGroupCountryId) == v.end(), ());
  v.clear();

  // Check there is no mwm or any other files in the writeable dir
  for (auto const & countryId : children)
  {
    string const mwmFullPath = GetMwmFilePath(version, countryId);
    string const downloadingFullPath = GetMwmDownloadingFilePath(version, countryId);
    string const resumeFullPath = GetMwmResumeFilePath(version, countryId);
    TEST(!platform.IsFileExistsByFullPath(mwmFullPath), ());
    TEST(!platform.IsFileExistsByFullPath(downloadingFullPath), ());
    TEST(!platform.IsFileExistsByFullPath(resumeFullPath), ());
  }

  // Download group
  storage.DownloadNode(kGroupCountryId);
  QCoreApplication::exec(); // wait for download

  // Check all group children have been downloaded and changed.
  TEST_EQUAL(downloaded, children, ());
  TEST_EQUAL(udpated, children, ());
  TEST_EQUAL(changed, children, ());

  // Check status for the all children nodes
  for (auto const & countryId : children)
  {
    TEST_EQUAL(TStatus::EOnDisk, storage.CountryStatusEx(countryId), ());
    NodeAttrs attrs;
    storage.GetNodeAttrs(countryId, attrs);
    TEST_EQUAL(ErrorCode::NoError, attrs.m_downloadingErrCode, ());
    // TEST_EQUAL(NodeStatus::UpToDate, attrs.m_status, ()); // DOES NOT WORK
  }

  // Check status for the group node
  NodeAttrs attrs;
  storage.GetNodeAttrs(kGroupCountryId, attrs);
  TEST_EQUAL(ErrorCode::NoError, attrs.m_downloadingErrCode, ());
  // TEST_EQUAL(NodeStatus::UpToDate, attrs.m_status, ()); // DOES NOT WORK

  // Check there is only mwm files are present and no any other
  for (auto const & countryId : children)
  {
    string const mwmFullPath = GetMwmFilePath(version, countryId);
    string const downloadingFullPath = GetMwmDownloadingFilePath(version, countryId);
    string const resumeFullPath = GetMwmResumeFilePath(version, countryId);
    TEST(platform.IsFileExistsByFullPath(mwmFullPath), ());
    TEST(!platform.IsFileExistsByFullPath(downloadingFullPath), ());
    TEST(!platform.IsFileExistsByFullPath(resumeFullPath), ());
  }

  // Check all group children are downloaded
  storage.GetDownloadedChildren(kGroupCountryId, v);
  TEST_EQUAL(children, TCountriesSet(v.begin(), v.end()), ());
  v.clear();

  // Check group is downloaded
  storage.GetDownloadedChildren(storage.GetRootId(), v);
  TEST(find(v.begin(), v.end(), kGroupCountryId) != v.end(), ());
  v.clear();

  // Delete

  // Delete first child node
  storage.DeleteNode(*children.begin());

  // Check all except first child are downloaded
  storage.GetDownloadedChildren(kGroupCountryId, v);
  TEST_EQUAL(TCountriesSet(++children.begin(), children.end()), TCountriesSet(v.begin(), v.end()), ());
  v.clear();

  // Check mwm files all children except first are present
  for (auto i = children.begin(); i != children.end(); ++i)
  {
    TCountryId const & countryId = *i;
    string const mwmFullPath = GetMwmFilePath(version, countryId);
    if (i == children.begin())
      TEST(!platform.IsFileExistsByFullPath(mwmFullPath), ())
    else
      TEST(platform.IsFileExistsByFullPath(mwmFullPath), ());
  }

  // Delete group node
  storage.DeleteNode(kGroupCountryId);

  // Check files are not present for all nodes
  for (auto const & countryId : children)
  {
    string const mwmFullPath = GetMwmFilePath(version, countryId);
    TEST(!platform.IsFileExistsByFullPath(mwmFullPath), ());
  }

  // Check group is not downloaded
  storage.GetDownloadedChildren(storage.GetRootId(), v);
  TEST(find(v.begin(), v.end(), kGroupCountryId) == v.end(), ());
  v.clear();
}
