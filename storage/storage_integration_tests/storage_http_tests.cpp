#include "testing/testing.hpp"

#include "storage/storage.hpp"

#include "platform/platform.hpp"

#include "platform/platform_tests_support/scoped_dir.hpp"

#include "coding/file_name_utils.hpp"

#include "base/scope_guard.hpp"
#include "base/string_utils.hpp"
#include "base/thread.hpp"

#include "std/string.hpp"

#include <QtCore/QCoreApplication>

using namespace platform;
using namespace storage;

class WritableDirChanger
{
  static string const kMapTestDir;
  string const m_writableDirBeforeTest;
  string const m_mapTestDirFullPath;
public:
  WritableDirChanger()
    : m_writableDirBeforeTest(GetPlatform().WritableDir()),
      m_mapTestDirFullPath(m_writableDirBeforeTest + kMapTestDir)
  {
    Platform & platform = GetPlatform();
    platform.MkDir(m_mapTestDirFullPath);
    platform.SetWritableDirForTests(m_mapTestDirFullPath);
  }
  ~WritableDirChanger()
  {
    Platform & platform = GetPlatform();
    string const writableDirForTest = platform.WritableDir();
    platform.SetWritableDirForTests(m_writableDirBeforeTest);
    platform.RmDir(writableDirForTest);
  }
};
string const WritableDirChanger::kMapTestDir = string("map-tests");

void ChangeCountryFunctionForAngola(TCountryId const &countryId) {}

void ProgressFunctionForAngola(TCountryId const &countryId, LocalAndRemoteSizeT const & mapSize)
{
  if (mapSize.first != mapSize.second)
    return;

  QCoreApplication::exit();
}

void UpdateForAngola(platform::LocalCountryFile const & localCountryFile) {}

UNIT_TEST(StorageDownloadNodeAndDeleteNodeTests)
{
  Storage storage;
  if (!version::IsSingleMwm(storage.GetCurrentDataVersion()))
    return; // Test is valid for single mwm case only.

  WritableDirChanger writableDirChanger;
  UNUSED_VALUE(writableDirChanger);

  storage.Init(UpdateForAngola);
  storage.RegisterAllLocalMaps();
  storage.Subscribe(ChangeCountryFunctionForAngola, ProgressFunctionForAngola);
  storage.SetDownloadingUrlsForTesting({ "http://eu1.mapswithme.com/" });
  string const angolaCountryId = "Angola";
  string const versionStr = strings::to_string(storage.GetCurrentDataVersion());
  platform::tests_support::ScopedDir cleanupVersionDir(versionStr);
  UNUSED_VALUE(cleanupVersionDir);
  MY_SCOPE_GUARD(cleanupAngola,
                 bind(&Storage::DeleteCountry, &storage, angolaCountryId, MapOptions::Map));

  string const angolaMwmFullPath = my::JoinFoldersToPath({GetPlatform().WritableDir(), versionStr },
                                                         angolaCountryId + string(".mwm"));
  string const angolaDownloadingFullPath = my::JoinFoldersToPath({GetPlatform().WritableDir(), versionStr },
                                                                 angolaCountryId + string(".mwm.ready.downloading"));
  string const angolaResumeFullPath = my::JoinFoldersToPath({GetPlatform().WritableDir(), versionStr },
                                                            angolaCountryId + string(".mwm.ready.resume"));
  Platform & platform = GetPlatform();
  TEST(!platform.IsFileExistsByFullPath(angolaMwmFullPath), ());
  TEST(!platform.IsFileExistsByFullPath(angolaDownloadingFullPath), ());
  TEST(!platform.IsFileExistsByFullPath(angolaResumeFullPath), ());

  // Downloading to an empty directory.
  storage.DownloadNode(angolaCountryId);
  QCoreApplication::exec();

  TEST(platform.IsFileExistsByFullPath(angolaMwmFullPath), ());
  TEST(!platform.IsFileExistsByFullPath(angolaDownloadingFullPath), ());
  TEST(!platform.IsFileExistsByFullPath(angolaResumeFullPath), ());

  // Downloading to directory with Angola.mwm.
  storage.DownloadNode(angolaCountryId);
  QCoreApplication::exec();

  TEST(platform.IsFileExistsByFullPath(angolaMwmFullPath), ());
  TEST(!platform.IsFileExistsByFullPath(angolaDownloadingFullPath), ());
  TEST(!platform.IsFileExistsByFullPath(angolaResumeFullPath), ());

  storage.DeleteNode(angolaCountryId);
  TEST(!platform.IsFileExistsByFullPath(angolaMwmFullPath), ());
}
