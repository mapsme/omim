#include "testing/testing.hpp"

#include "map/framework.hpp"

#include "platform/platform.hpp"
#include "platform/platform_tests_support/write_dir_changer.hpp"

#include "coding/file_name_utils.hpp"

#include "std/algorithm.hpp"
#include "std/unique_ptr.hpp"

#include "defines.hpp"

using namespace platform;
using namespace storage;

namespace
{

string const kTestWebServer = "http://new-search.mapswithme.com/";

string const kMapTestDir = "map-tests";

string const kCountryId = "Germany"; // Germany has 3-levels hierachy

int GetLevelCount(Storage & storage, TCountryId const & countryId)
{
  TCountriesVec children;
  storage.GetChildren(countryId, children);
  int level = 0;
  for (auto const & child : children)
    level = max(level, GetLevelCount(storage, child));
  return 1 + level;
}

} // namespace

UNIT_TEST(SmallMwms_3levels_Test)
{
  WritableDirChanger writableDirChanger(kMapTestDir);

  Platform & platform = GetPlatform();

  Framework f;
  auto & storage = f.Storage();
  string const version = strings::to_string(storage.GetCurrentDataVersion());
  TEST(version::IsSingleMwm(storage.GetCurrentDataVersion()), ());

  TEST_EQUAL(3, GetLevelCount(storage, kCountryId), ());

  string const mapDir = my::JoinFoldersToPath(platform.WritableDir(), version);

  auto onProgressFn = [&](TCountryId const & countryId, TLocalAndRemoteSize const & mapSize)
  {
  };

  auto onChangeCountryFn = [&](TCountryId const & countryId)
  {
    if (!storage.IsDownloadInProgress())
      testing::StopEventLoop();
  };

  storage.Subscribe(onChangeCountryFn, onProgressFn);
  storage.SetDownloadingUrlsForTesting({kTestWebServer});

  NodeAttrs attrs;
  storage.GetNodeAttrs(kCountryId, attrs);
  TEST_EQUAL(attrs.m_status, NodeStatus::NotDownloaded, ());

  Platform::FilesList files;
  platform.GetFilesByExt(mapDir, DATA_FILE_EXTENSION, files);
  TEST_EQUAL(0, files.size(), ());

  storage.DownloadNode(kCountryId);
  testing::RunEventLoop();

  storage.GetNodeAttrs(kCountryId, attrs);
  TEST_EQUAL(attrs.m_status, NodeStatus::OnDisk, ());

  files.clear();
  platform.GetFilesByExt(mapDir, DATA_FILE_EXTENSION, files);
  TEST_GREATER(files.size(), 0, ());

  TCountriesVec children;
  storage.ForEachInSubtree(kCountryId, [&](TCountryId const & leaf, bool expandable)
  {
    if (!expandable)
      children.emplace_back(leaf);
  });
  for (auto const & child : children)
  {
    string const mwmFile = my::JoinFoldersToPath(mapDir, child + DATA_FILE_EXTENSION);
    TEST(platform.IsFileExistsByFullPath(mwmFile), ());
  }

  storage.DeleteNode(kCountryId);

  storage.GetNodeAttrs(kCountryId, attrs);
  TEST_EQUAL(attrs.m_status, NodeStatus::NotDownloaded, ());

  files.clear();
  platform.GetFilesByExt(mapDir, DATA_FILE_EXTENSION, files);
  TEST_EQUAL(0, files.size(), ());
}
