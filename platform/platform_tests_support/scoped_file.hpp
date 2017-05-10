#pragma once

#include "platform/country_defines.hpp"
#include "platform/platform.hpp"

#include "base/macros.hpp"

#include <string>

namespace platform
{
class CountryFile;

namespace tests_support
{
class ScopedDir;

class ScopedFile
{
public:
  ScopedFile(std::string const & relativePath);
  ScopedFile(std::string const & relativePath, std::string const & contents);

  ScopedFile(ScopedDir const & dir, CountryFile const & countryFile, MapOptions file,
             std::string const & contents);

  ~ScopedFile();

  inline std::string const & GetFullPath() const { return m_fullPath; }

  inline void Reset() { m_reset = true; }

  inline bool Exists() const { return GetPlatform().IsFileExistsByFullPath(GetFullPath()); }

private:
  std::string const m_fullPath;
  bool m_reset;

  DISALLOW_COPY_AND_MOVE(ScopedFile);
};

std::string DebugPrint(ScopedFile const & file);
}  // namespace tests_support
}  // namespace platform
