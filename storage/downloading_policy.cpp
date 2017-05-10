#include "storage/downloading_policy.hpp"

#include "platform/platform.hpp"

void StorageDownloadingPolicy::EnableCellularDownload(bool enabled)
{
  m_cellularDownloadEnabled = enabled;
  m_disableCellularTime = std::chrono::steady_clock::now() + std::chrono::hours(1);
}

bool StorageDownloadingPolicy::IsCellularDownloadEnabled()
{
  if (m_cellularDownloadEnabled && std::chrono::steady_clock::now() > m_disableCellularTime)
    m_cellularDownloadEnabled = false;

  return m_cellularDownloadEnabled;
}

bool StorageDownloadingPolicy::IsDownloadingAllowed()
{
  return !(GetPlatform().ConnectionStatus() == Platform::EConnectionType::CONNECTION_WWAN &&
           !IsCellularDownloadEnabled());
}

void StorageDownloadingPolicy::ScheduleRetry(storage::TCountriesSet const & failedCountries,
                                             TProcessFunc const & func)
{
  if (IsDownloadingAllowed() && !failedCountries.empty() && m_autoRetryCounter > 0)
  {
    m_downloadRetryFailed = false;
    auto action = [this, func, failedCountries] {
      --m_autoRetryCounter;
      func(failedCountries);
    };
    m_autoRetryWorker.RestartWith([action]{ GetPlatform().RunOnGuiThread(action); });
  }
  else
  {
    if (!failedCountries.empty())
      m_downloadRetryFailed = true;
    m_autoRetryCounter = kAutoRetryCounterMax;
  }
}
