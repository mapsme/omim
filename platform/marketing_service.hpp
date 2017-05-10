#pragma once

#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace marketing
{
// Tags.
extern char const * const kMapVersion;
extern char const * const kMapListing;
extern char const * const kMapDownloadDiscovered;
extern char const * const kMapLastDownloaded;
extern char const * const kMapLastDownloadedTimestamp;
extern char const * const kRoutingP2PVehicleDiscovered;
extern char const * const kRoutingP2PPedestrianDiscovered;
extern char const * const kRoutingP2PBicycleDiscovered;
extern char const * const kRoutingP2PTaxiDiscovered;
extern char const * const kRoutingVehicleDiscovered;
extern char const * const kRoutingPedestrianDiscovered;
extern char const * const kRoutingBicycleDiscovered;
extern char const * const kRoutingTaxiDiscovered;
extern char const * const kEditorAddDiscovered;
extern char const * const kEditorEditDiscovered;
extern char const * const kTrafficDiscovered;

// Events.
extern char const * const kDownloaderMapActionFinished;
extern char const * const kSearchEmitResultsAndCoords;
extern char const * const kRoutingCalculatingRoute;
extern char const * const kBookmarksBookmarkAction;
extern char const * const kPlacepageHotelBook;
extern char const * const kEditorAddStart;
extern char const * const kEditorEditStart;
}  // marketing

class MarketingService
{
public:
  using PushWooshSenderFn = std::function<void(std::string const & tag, std::vector<std::string> const & values)>;
  using MarketingSenderFn = std::function<void(std::string const & tag, std::map<std::string, std::string> const & params)>;

  void ProcessFirstLaunch();

  void SetPushWooshSender(PushWooshSenderFn const & fn) { m_pushwooshSender = fn; }
  void SendPushWooshTag(std::string const & tag);
  void SendPushWooshTag(std::string const & tag, std::string const & value);
  void SendPushWooshTag(std::string const & tag, std::vector<std::string> const & values);

  void SetMarketingSender(MarketingSenderFn const & fn) { m_marketingSender = fn; }
  void SendMarketingEvent(std::string const & tag, std::map<std::string, std::string> const & params);

private:
  /// Callback fucntion for setting PushWoosh tags.
  PushWooshSenderFn m_pushwooshSender;

  /// Callback fucntion for sending marketing events.
  MarketingSenderFn m_marketingSender;
};
