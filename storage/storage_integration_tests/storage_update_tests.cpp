#include "testing/testing.hpp"

#include "storage/storage_integration_tests/test_defines.hpp"

#include "map/framework.hpp"

#include "platform/http_request.hpp"
#include "platform/local_country_file_utils.hpp"
#include "platform/platform.hpp"
#include "platform/platform_tests_support/writable_dir_changer.hpp"

#include "coding/file_name_utils.hpp"
#include "coding/internal/file_data.hpp"

#include "storage/storage.hpp"

#include "std/unique_ptr.hpp"

using namespace platform;
using namespace storage;

namespace
{
static FrameworkParams const kFrameworkParams(false /* m_enableLocalAds */, false /* m_enableDiffs */);

string const kCountriesTxtFile = COUNTRIES_FILE;

string const kMwmVersion1 = "160316";
size_t const kCountriesTxtFileSize1 = 353091;

string const kMwmVersion2 = "160317";
size_t const kCountriesTxtFileSize2 = 348972;

string const kGroupCountryId = "Belarus";

bool DownloadFile(string const & url,
                  string const & filePath,
                  size_t fileSize)
{
  using namespace downloader;

  HttpRequest::Status httpStatus;
  bool finished = false;

  unique_ptr<HttpRequest> request(HttpRequest::GetFile({url}, filePath, fileSize,
                                  [&](HttpRequest & request)
  {
    HttpRequest::Status const s = request.GetStatus();
    if (s != HttpRequest::Status::InProgress)
    {
      httpStatus = s;
      finished = true;
      testing::StopEventLoop();
    }
  }));

  testing::RunEventLoop();

  return httpStatus == HttpRequest::Status::Completed;
}

string GetCountriesTxtWebUrl(string const version)
{
  return kTestWebServer + "direct/" + version + "/" + kCountriesTxtFile;
}

string GetCountriesTxtFilePath()
{
  return my::JoinFoldersToPath(GetPlatform().WritableDir(), kCountriesTxtFile);
}

string GetMwmFilePath(string const & version, TCountryId const & countryId)
{
  return my::JoinFoldersToPath({GetPlatform().WritableDir(), version},
                               countryId + DATA_FILE_EXTENSION);
}

} // namespace

UNIT_TEST(SmallMwms_Update_Test)
{
  WritableDirChanger writableDirChanger(kMapTestDir);

  Platform & platform = GetPlatform();

  auto onProgressFn = [&](TCountryId const & countryId, TLocalAndRemoteSize const & mapSize) {};

  // Download countries.txt for version 1
  TEST(DownloadFile(GetCountriesTxtWebUrl(kMwmVersion1), GetCountriesTxtFilePath(), kCountriesTxtFileSize1), ());

  {
    Framework f(kFrameworkParams);
    auto & storage = f.GetStorage();
    string const version = strings::to_string(storage.GetCurrentDataVersion());
    TEST(version::IsSingleMwm(storage.GetCurrentDataVersion()), ());
    TEST_EQUAL(version, kMwmVersion1, ());
    auto onChangeCountryFn = [&](TCountryId const & countryId)
    {
      if (!storage.IsDownloadInProgress())
        testing::StopEventLoop();
    };
    storage.Subscribe(onChangeCountryFn, onProgressFn);
    storage.SetDownloadingUrlsForTesting({kTestWebServer});

    TCountriesVec children;
    storage.GetChildren(kGroupCountryId, children);

    // Download group
    storage.DownloadNode(kGroupCountryId);
    testing::RunEventLoop();

    // Check group node status is EOnDisk
    NodeAttrs attrs;
    storage.GetNodeAttrs(kGroupCountryId, attrs);
    TEST_EQUAL(NodeStatus::OnDisk, attrs.m_status, ());

    // Check mwm files for version 1 are present
    for (auto const & child : children)
    {
      string const mwmFullPathV1 = GetMwmFilePath(kMwmVersion1, child);
      TEST(platform.IsFileExistsByFullPath(mwmFullPathV1), ());
    }
  }

  // Replace countries.txt by version 2
  TEST(my::DeleteFileX(GetCountriesTxtFilePath()), ());
  TEST(DownloadFile(GetCountriesTxtWebUrl(kMwmVersion2), GetCountriesTxtFilePath(), kCountriesTxtFileSize2), ());

  {
    Framework f(kFrameworkParams);
    auto & storage = f.GetStorage();
    string const version = strings::to_string(storage.GetCurrentDataVersion());
    TEST(version::IsSingleMwm(storage.GetCurrentDataVersion()), ());
    TEST_EQUAL(version, kMwmVersion2, ());
    auto onChangeCountryFn = [&](TCountryId const & countryId)
    {
      if (!storage.IsDownloadInProgress())
        testing::StopEventLoop();
    };
    storage.Subscribe(onChangeCountryFn, onProgressFn);
    storage.SetDownloadingUrlsForTesting({kTestWebServer});

    TCountriesVec children;
    storage.GetChildren(kGroupCountryId, children);

    // Check group node status is EOnDiskOutOfDate
    NodeAttrs attrs;
    storage.GetNodeAttrs(kGroupCountryId, attrs);
    TEST_EQUAL(NodeStatus::OnDiskOutOfDate, attrs.m_status, ());

    // Check children node status is EOnDiskOutOfDate
    for (auto const & child : children)
    {
      NodeAttrs attrs;
      storage.GetNodeAttrs(child, attrs);
      TEST_EQUAL(NodeStatus::OnDiskOutOfDate, attrs.m_status, ());
    }

    // Check mwm files for version 1 are present
    for (auto const & child : children)
    {
      string const mwmFullPathV1 = GetMwmFilePath(kMwmVersion1, child);
      TEST(platform.IsFileExistsByFullPath(mwmFullPathV1), ());
    }

    // Download group, new version
    storage.DownloadNode(kGroupCountryId);
    testing::RunEventLoop();

    // Check group node status is EOnDisk
    storage.GetNodeAttrs(kGroupCountryId, attrs);
    TEST_EQUAL(NodeStatus::OnDisk, attrs.m_status, ());

    // Check children node status is EOnDisk
    for (auto const & child : children)
    {
      NodeAttrs attrs;
      storage.GetNodeAttrs(child, attrs);
      TEST_EQUAL(NodeStatus::OnDisk, attrs.m_status, ());
    }

    // Check mwm files for version 2 are present and not present for version 1
    for (auto const & child : children)
    {
      string const mwmFullPathV1 = GetMwmFilePath(kMwmVersion1, child);
      string const mwmFullPathV2 = GetMwmFilePath(kMwmVersion2, child);
      TEST(platform.IsFileExistsByFullPath(mwmFullPathV2), ());
      TEST(!platform.IsFileExistsByFullPath(mwmFullPathV1), ());
    }
  }
}
