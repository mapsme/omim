#pragma once

#import "storage/background_downloading/downloader_subscriber_adapter_ios.h"

#include "storage/background_downloading/downloader_queue.hpp"
#include "storage/map_files_downloader_with_ping.hpp"

#include <cstdint>
#include <optional>

namespace storage
{
class BackgroundDownloaderAdapter : public MapFilesDownloaderWithPing
{
public:
  BackgroundDownloaderAdapter();

  // MapFilesDownloader overrides:
  void Remove(CountryId const & countryId) override;

  void Clear() override;

  QueueInterface const & GetQueue() const override;

private:
  // MapFilesDownloaderWithServerList overrides:
  void Download(QueuedCountry & queuedCountry) override;

  // Trying to download mwm from different servers recursively.
  void DownloadFromLastUrl(CountryId const & countryId, std::string const & downloadPath,
                           std::vector<std::string> const & urls);

  BackgroundDownloaderQueue<uint64_t> m_queue;
  SubscriberAdapter * m_subscriberAdapter;
};
}  // namespace storage
