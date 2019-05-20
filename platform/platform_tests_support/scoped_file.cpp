#include "platform/platform_tests_support/scoped_file.hpp"

#include "testing/testing.hpp"

#include "platform/country_file.hpp"
#include "platform/mwm_version.hpp"
#include "platform/platform_tests_support/scoped_dir.hpp"

#include "coding/file_writer.hpp"
#include "coding/internal/file_data.hpp"
#include "coding/writer.hpp"

#include "base/assert.hpp"
#include "base/file_name_utils.hpp"
#include "base/logging.hpp"

#include "std/sstream.hpp"

namespace platform
{
namespace tests_support
{
ScopedFile::ScopedFile(string const & relativePath, Mode mode)
  : ScopedFile(relativePath, {} /* contents */, mode)
{
}

ScopedFile::ScopedFile(string const & relativePath, string const & contents)
  : ScopedFile(relativePath, contents, Mode::Create)
{
}

ScopedFile::ScopedFile(ScopedDir const & dir, CountryFile const & countryFile,
                       MapOptions mapOptions)
  : ScopedFile(
        base::JoinPath(dir.GetRelativePath(), GetFileName(countryFile.GetName(), mapOptions,
                                                          version::FOR_TESTING_TWO_COMPONENT_MWM1)),
        Mode::Create)
{
}

ScopedFile::ScopedFile(string const & relativePath, string const & contents, Mode mode)
  : m_fullPath(base::JoinPath(GetPlatform().WritableDir(), relativePath))
{
  if (mode == Mode::DoNotCreate)
    return;

  try
  {
    FileWriter writer(GetFullPath());
    writer.Write(contents.data(), contents.size());
  }
  catch (Writer::Exception const & e)
  {
    LOG(LERROR, ("Can't create test file:", e.what()));
  }

  CHECK(Exists(), ("Can't create test file", GetFullPath()));
}

ScopedFile::~ScopedFile()
{
  if (m_reset)
    return;
  if (!Exists())
  {
    LOG(LERROR, ("File", GetFullPath(), "did not exist or was deleted before dtor of ScopedFile."));
    return;
  }
  if (!base::DeleteFileX(GetFullPath()))
    LOG(LERROR, ("Can't remove test file:", GetFullPath()));
}

string DebugPrint(ScopedFile const & file)
{
  ostringstream os;
  os << "ScopedFile [" << file.GetFullPath() << "]";
  return os.str();
}
}  // namespace tests_support
}  // namespace platform
