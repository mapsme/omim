#include "scoped_mwm.hpp"

#include "defines.hpp"

#include "indexer/data_header.hpp"

#include "platform/mwm_version.hpp"

#include "coding/file_writer.hpp"
#include "coding/file_container.hpp"
#include "coding/internal/file_data.hpp"

#include "base/timer.hpp"

using feature::DataHeader;
namespace platform
{
namespace tests_support
{
ScopedMwm::ScopedMwm(string const & relativePath) : m_file(relativePath, ScopedFile::Mode::Create)
{
  DataHeader header;
  {
    FilesContainerW container(m_file.GetFullPath());

    // Each writer must be in it's own scope to avoid conflicts on the final write.
    {
      FileWriter versionWriter = container.GetWriter(VERSION_FILE_TAG);
      version::WriteVersion(versionWriter, my::SecondsSinceEpoch());
    }

    FileWriter w = container.GetWriter(HEADER_FILE_TAG);
    header.Save(w);
  }
}
}  // namespace tests_support
}  // namespace platfotm
