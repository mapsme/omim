#include "platform/country_file.hpp"
#include "platform/mwm_version.hpp"

#include "defines.hpp"

#include "base/assert.hpp"

#include <sstream>

namespace
{
/// \returns file name (m_name) with extension dependent on the file param.
/// The extension could be .mwm.routing or just .mwm.
/// The method is used for old (two components) mwm support.
std::string GetNameWithExt(std::string const & countryFile, MapOptions file)
{
  switch (file)
  {
    case MapOptions::Map:
      return countryFile + DATA_FILE_EXTENSION;
    case MapOptions::CarRouting:
      return countryFile + DATA_FILE_EXTENSION + ROUTING_FILE_EXTENSION;
    default:
      ASSERT(false, ("Can't get name for:", file));
      return std::string();
  }
}
} //  namespace

namespace platform
{
CountryFile::CountryFile() : m_mapSize(0), m_routingSize(0) {}

CountryFile::CountryFile(std::string const & name) : m_name(name), m_mapSize(0), m_routingSize(0) {}

std::string const & CountryFile::GetName() const { return m_name; }

void CountryFile::SetRemoteSizes(TMwmSize mapSize, TMwmSize routingSize)
{
  m_mapSize = mapSize;
  m_routingSize = routingSize;
}

TMwmSize CountryFile::GetRemoteSize(MapOptions filesMask) const
{
  uint32_t size = 0;
  if (HasOptions(filesMask, MapOptions::Map))
    size += m_mapSize;
  if (HasOptions(filesMask, MapOptions::CarRouting))
    size += m_routingSize;
  return size;
}


std::string GetFileName(std::string const & countryFile, MapOptions opt, int64_t version)
{
  return version::IsSingleMwm(version) ? GetNameWithExt(countryFile, MapOptions::Map)
                                       : GetNameWithExt(countryFile, opt);
}

std::string DebugPrint(CountryFile const & file)
{
  std::ostringstream os;
  os << "CountryFile [" << file.m_name << "]";
  return os.str();
}
}  // namespace platform
