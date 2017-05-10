#include "testing/testing.hpp"

#include "indexer/index.hpp"

#include "platform/local_country_file_utils.hpp"
#include "platform/platform.hpp"

#include "base/scope_guard.hpp"

#ifndef OMIM_OS_WINDOWS
#include <sys/stat.h>
#endif


using namespace platform;
using namespace my;

/*
 * This test is useless because of we don't build offsets index from now.
#ifndef OMIM_OS_WINDOWS
UNIT_TEST(MwmSet_FileSystemErrors)
{
  std::string const dir = GetPlatform().WritableDir();

  CountryFile file("minsk-pass");
  LocalCountryFile localFile(dir, file, 0);
  TEST(CountryIndexes::DeleteFromDisk(localFile), ());

  // Maximum level to check exception handling logic.
  LogLevel oldLevel = g_LogAbortLevel;
  g_LogAbortLevel = LCRITICAL;

  // Remove writable permission.
  int const readOnlyMode = S_IRUSR | S_IRGRP | S_IROTH | S_IXUSR | S_IXGRP | S_IXOTH;
  TEST_EQUAL(chmod(dir.c_str(), readOnlyMode), 0, ());

  auto restoreFn = [oldLevel, &dir, readOnlyMode] ()
  {
    g_LogAbortLevel = oldLevel;
    TEST_EQUAL(chmod(dir.c_str(), readOnlyMode | S_IWUSR), 0, ());
  };
  MY_SCOPE_GUARD(restoreGuard, restoreFn);

  Index index;
  auto p = index.RegisterMap(localFile);
  TEST_EQUAL(p.second, Index::RegResult::Success, ());

  // Registering should pass ok.
  TEST(index.GetMwmIdByCountryFile(file) != Index::MwmId(), ());

  // Getting handle causes feature offsets index building which should fail
  // because of write permissions.
  TEST(!index.GetMwmHandleById(p.first).IsAlive(), ());

  // Map is automatically deregistered after the fail.
  vector<shared_ptr<MwmInfo>> infos;
  index.GetMwmsInfo(infos);
  TEST(infos.empty(), ());
}
#endif
*/
