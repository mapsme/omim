#include "testing/testing.hpp"

#include "map/framework.hpp"

#include "platform/local_country_file_utils.hpp"
#include "platform/platform.hpp"
#include "platform/platform_tests_support/scoped_dir.hpp"
#include "platform/platform_tests_support/write_dir_changer.hpp"

#include "coding/file_name_utils.hpp"

#include "base/scope_guard.hpp"
#include "base/string_utils.hpp"
#include "base/thread.hpp"

#include "std/string.hpp"

using namespace platform;
using namespace storage;

namespace
{
static string const kMapTestDir = "map-tests";
}

UNIT_TEST(StorageFastMigrationTests)
{
  WritableDirChanger writableDirChanger(kMapTestDir);

  Framework f;
  auto & s = f.Storage();

  uint32_t version;
  TEST(Settings::Get("LastMigration", version), ("LastMigration didn't set"));

  TEST_GREATER_OR_EQUAL(s.GetCurrentDataVersion(), version, ());
}

UNIT_TEST(StorageMigrationTests)
{
  TCountriesVec const kOldCountries = {"Estonia"};
  TCountriesVec const kNewCountries = {"Estonia_East", "Estonia_West"};
  TCountriesVec const kPrefetchCountries = {"Russia_Moscow"};

  WritableDirChanger writableDirChanger(kMapTestDir);

  Settings::Set("DisableFastMigrate", true);

  Framework f;
  auto & s = f.Storage();

  auto statePrefetchChanged = [&](TCountryId const & id)
  {
    TStatus const nextStatus = f.Storage().m_prefetchStorage->CountryStatusEx(id);
    LOG_SHORT(LINFO, (id, "status :", nextStatus));
    if (!f.Storage().m_prefetchStorage->IsDownloadInProgress())
    {
      LOG_SHORT(LINFO, ("All prefetched. Ready to migrate."));
      testing::StopEventLoop();
    }
  };

  auto stateChanged = [&](TCountryId const & id)
  {
    if (!f.Storage().IsDownloadInProgress())
    {
      LOG_SHORT(LINFO, ("All downloaded. Check consistency."));
      testing::StopEventLoop();
    }
  };

  auto progressChanged = [](TCountryId const & id, TLocalAndRemoteSize const & sz)
  {
    LOG_SHORT(LINFO, (id, "downloading progress:", sz));
  };

  // Somewhere in Moscow, Russia
  ms::LatLon curPos(55.7, 37.7);

  s.SetDownloadingUrlsForTesting({"http://new-search.mapswithme.com/"});
  s.Subscribe(stateChanged, progressChanged);
  for (auto const & countryId : kOldCountries)
    s.DownloadNode(countryId);

  // Wait for downloading complete.
  testing::RunEventLoop();

  TEST_EQUAL(s.GetDownloadedFilesCount(), kOldCountries.size(), ());
  for (auto const & countryId : kOldCountries)
    TEST(s.IsNodeDownloaded(countryId), (countryId));

  f.PreMigrate(curPos, statePrefetchChanged, progressChanged);
  // Wait for downloading complete.
  testing::RunEventLoop();

  TEST_EQUAL(s.GetDownloadedFilesCount(), kPrefetchCountries.size(), ());
  for (auto const & countryId : kPrefetchCountries)
    TEST(s.m_prefetchStorage->IsNodeDownloaded(countryId), (countryId));

  f.Migrate();
  // Wait for downloading complete.
  testing::RunEventLoop();

  TEST_EQUAL(s.GetDownloadedFilesCount(), kPrefetchCountries.size() + kNewCountries.size(), ());
  for (auto const & countryId : kNewCountries)
    TEST(s.IsNodeDownloaded(countryId), (countryId));
  for (auto const & countryId : kPrefetchCountries)
    TEST(s.IsNodeDownloaded(countryId), (countryId));

  s.DeleteAllLocalMaps();
}
