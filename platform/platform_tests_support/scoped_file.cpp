#include "platform/platform_tests_support/scoped_file.hpp"

#include "testing/testing.hpp"

#include "platform/country_file.hpp"
#include "platform/mwm_version.hpp"
#include "platform/platform_tests_support/scoped_dir.hpp"

#include "coding/file_name_utils.hpp"
#include "coding/file_writer.hpp"
#include "coding/internal/file_data.hpp"

#include "base/logging.hpp"

#include <sstream>

namespace platform
{
namespace tests_support
{
ScopedFile::ScopedFile(std::string const & relativePath)
    : m_fullPath(my::JoinFoldersToPath(GetPlatform().WritableDir(), relativePath)),
      m_reset(false)
{
}

ScopedFile::ScopedFile(std::string const & relativePath, std::string const & contents)
    : ScopedFile(relativePath)
{
  {
    FileWriter writer(GetFullPath());
    writer.Write(contents.data(), contents.size());
  }
  TEST(Exists(), ("Can't create test file", GetFullPath()));
}

ScopedFile::ScopedFile(ScopedDir const & dir, CountryFile const & countryFile, MapOptions file,
                       std::string const & contents)
  : ScopedFile(my::JoinFoldersToPath(dir.GetRelativePath(),
                                     GetFileName(countryFile.GetName(), file, version::FOR_TESTING_TWO_COMPONENT_MWM1)),
               contents)
{
}

ScopedFile::~ScopedFile()
{
  if (m_reset)
    return;
  if (!Exists())
  {
    LOG(LWARNING, ("File", GetFullPath(), "was deleted before dtor of ScopedFile."));
    return;
  }
  if (!my::DeleteFileX(GetFullPath()))
    LOG(LWARNING, ("Can't remove test file:", GetFullPath()));
}

std::string DebugPrint(ScopedFile const & file)
{
  std::ostringstream os;
  os << "ScopedFile [" << file.GetFullPath() << "]";
  return os.str();
}
}  // namespace tests_support
}  // namespace platform
