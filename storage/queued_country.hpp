#pragma once

#include "storage/index.hpp"
#include "platform/country_defines.hpp"

namespace storage
{
/// Country queued for downloading.
class QueuedCountry
{
public:
  QueuedCountry(TCountryId const & m_countryId, MapOptions opt);

  void AddOptions(MapOptions opt);
  void RemoveOptions(MapOptions opt);
  bool SwitchToNextFile();

  inline TCountryId const & GetCountryId() const { return m_countryId; }
  inline MapOptions GetInitOptions() const { return m_init; }
  inline MapOptions GetCurrentFile() const { return m_current; }
  inline MapOptions GetDownloadedFiles() const { return UnsetOptions(m_init, m_left); }

  inline bool operator==(TCountryId const & countryId) const { return m_countryId == countryId; }

private:
  TCountryId m_countryId;
  MapOptions m_init;
  MapOptions m_left;
  MapOptions m_current;
};
}  // namespace storage
