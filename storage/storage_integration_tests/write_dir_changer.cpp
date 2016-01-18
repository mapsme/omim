#include "testing/testing.hpp"

#include "write_dir_changer.hpp"

#include "platform/platform.hpp"

#include "coding/file_name_utils.hpp"
#include "coding/internal/file_data.hpp"

WritableDirChanger::WritableDirChanger(string testDir)
  : m_writableDirBeforeTest(GetPlatform().WritableDir())
  , m_testDirFullPath(m_writableDirBeforeTest + testDir)
{
  Platform & platform = GetPlatform();
  Platform::EError const err = platform.MkDir(m_testDirFullPath);
  switch (err)
  {
  case Platform::ERR_OK:
    break;
  case Platform::ERR_FILE_ALREADY_EXISTS:
    Platform::EFileType type;
    TEST_EQUAL(platform.GetFileType(m_testDirFullPath, type), Platform::ERR_OK, ());
    TEST_EQUAL(type, Platform::FILE_TYPE_DIRECTORY, ());
    break;
  default:
    TEST(false, ("Can't create directory:", m_testDirFullPath));
    break;
  }
  platform.SetWritableDirForTests(m_testDirFullPath);
}

WritableDirChanger::~WritableDirChanger()
{
  Platform & platform = GetPlatform();
  string const writableDirForTest = platform.WritableDir();
  platform.SetWritableDirForTests(m_writableDirBeforeTest);
  string const settingsFileFullPath = my::JoinFoldersToPath(writableDirForTest, SETTINGS_FILE_NAME);
  if (platform.IsFileExistsByFullPath(settingsFileFullPath))
    my::DeleteFileX(settingsFileFullPath);
  platform.RmDir(writableDirForTest);
}
