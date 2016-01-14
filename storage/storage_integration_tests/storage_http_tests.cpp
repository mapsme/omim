#include "testing/testing.hpp"

#include "storage/storage.hpp"

#include "platform/local_country_file_utils.hpp"
#include "platform/platform.hpp"
#include "platform/platform_tests_support/scoped_dir.hpp"

#include "coding/file_name_utils.hpp"
#include "coding/internal/file_data.hpp"

#include "base/scope_guard.hpp"
#include "base/string_utils.hpp"
#include "base/thread.hpp"

#include "std/string.hpp"

#include <QtCore/QCoreApplication>

using namespace platform;
using namespace storage;

class WritableDirChanger
{
public:
  static string const kMapTestDir;
private:
  string const m_writableDirBeforeTest;
  string const m_mapTestDirFullPath;

public:
  WritableDirChanger()
    : m_writableDirBeforeTest(GetPlatform().WritableDir())
    , m_mapTestDirFullPath(m_writableDirBeforeTest + kMapTestDir)
  {
    Platform & platform = GetPlatform();
    Platform::EError const err = platform.MkDir(m_mapTestDirFullPath);
    switch (err)
    {
    case Platform::ERR_OK:
      break;
    case Platform::ERR_FILE_ALREADY_EXISTS:
      Platform::EFileType type;
      TEST_EQUAL(platform.GetFileType(m_mapTestDirFullPath, type), Platform::ERR_OK, ());
      TEST_EQUAL(type, Platform::FILE_TYPE_DIRECTORY, ());
      break;
    default:
      TEST(false, ("Can't create directory:", m_mapTestDirFullPath));
      break;
    }

    platform.SetWritableDirForTests(m_mapTestDirFullPath);
  }
  ~WritableDirChanger()
  {
    Platform & platform = GetPlatform();
    string const writableDirForTest = platform.WritableDir();
    platform.SetWritableDirForTests(m_writableDirBeforeTest);
    string const settingsFileFullPath = my::JoinFoldersToPath(writableDirForTest, SETTINGS_FILE_NAME);
    if (platform.IsFileExistsByFullPath(settingsFileFullPath))
      my::DeleteFileX(settingsFileFullPath);
    platform.RmDir(writableDirForTest);
  }
};

string const WritableDirChanger::kMapTestDir = "map-tests";

WritableDirChanger g_writableDirChanger;

string const kCountryId = string("Angola");

void ChangeCountryFunction(TCountryId const & countryId) {}

void ProgressFunction(TCountryId const & countryId, LocalAndRemoteSizeT const & mapSize)
{
  TEST_EQUAL(countryId, kCountryId, ());
  if (mapSize.first != mapSize.second)
    return;

  QCoreApplication::exit();
}

void Update(LocalCountryFile const & localCountryFile)
{
  TEST_EQUAL(localCountryFile.GetCountryName(), kCountryId, ());
}

UNIT_TEST(StorageDownloadNodeAndDeleteNodeTests)
{
  Storage storage;
  if (!version::IsSingleMwm(storage.GetCurrentDataVersion()))
    return;  // Test is valid for single mwm case only.

  storage.Init(Update);
  storage.RegisterAllLocalMaps();
  storage.Subscribe(ChangeCountryFunction, ProgressFunction);
  storage.SetDownloadingUrlsForTesting({"http://new-search.mapswithme.com/"});
  string const version = strings::to_string(storage.GetCurrentDataVersion());
  tests_support::ScopedDir cleanupVersionDir(version);
  MY_SCOPE_GUARD(cleanup,
                 bind(&Storage::DeleteNode, &storage, kCountryId));
  DeleteDownloaderFilesForCountry(storage.GetCurrentDataVersion(),
                                  WritableDirChanger::kMapTestDir, CountryFile(kCountryId));

  string const mwmFullPath = my::JoinFoldersToPath({GetPlatform().WritableDir(), version},
                                                   kCountryId + DATA_FILE_EXTENSION);
  string const downloadingFullPath = my::JoinFoldersToPath(
      {GetPlatform().WritableDir(), version},
      kCountryId + DATA_FILE_EXTENSION READY_FILE_EXTENSION DOWNLOADING_FILE_EXTENSION);
  string const resumeFullPath = my::JoinFoldersToPath(
      {GetPlatform().WritableDir(), version},
      kCountryId + DATA_FILE_EXTENSION READY_FILE_EXTENSION RESUME_FILE_EXTENSION);
  Platform & platform = GetPlatform();

  // Downloading to an empty directory.
  storage.DownloadNode(kCountryId);
  QCoreApplication::exec();

  TEST(platform.IsFileExistsByFullPath(mwmFullPath), ());
  TEST(!platform.IsFileExistsByFullPath(downloadingFullPath), ());
  TEST(!platform.IsFileExistsByFullPath(resumeFullPath), ());

  // Downloading to directory with Angola.mwm.
  storage.DownloadNode(kCountryId);
  QCoreApplication::exec();

  TEST(platform.IsFileExistsByFullPath(mwmFullPath), ());
  TEST(!platform.IsFileExistsByFullPath(downloadingFullPath), ());
  TEST(!platform.IsFileExistsByFullPath(resumeFullPath), ());

  storage.DeleteNode(kCountryId);
  TEST(!platform.IsFileExistsByFullPath(mwmFullPath), ());
}
