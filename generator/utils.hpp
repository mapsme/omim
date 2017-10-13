#pragma once

#include "indexer/index.hpp"
#include "indexer/mwm_set.hpp"

#include "platform/local_country_file.hpp"

#include <string>

namespace generator
{
/// \brief This class is wrapper around |Index| if only one mwm is registered in Index.
class SingleMwmIndex
{
public:
  /// \param mwmPath is a path to mwm which should be registerd in Index.
  explicit SingleMwmIndex(std::string const & mwmPath);

  Index & GetIndex() { return m_index; }
  std::string GetPath(MapOptions file) const { return m_countryFile.GetPath(file); }
  MwmSet::MwmId const & GetMwmId() const { return m_mwmId; }

private:
  Index m_index;
  platform::LocalCountryFile m_countryFile;
  MwmSet::MwmId m_mwmId;
};

void LoadIndex(Index & index);
}  // namespace generator
