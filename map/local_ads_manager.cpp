#include "map/local_ads_manager.hpp"
#include "map/bookmark_manager.hpp"
#include "map/local_ads_mark.hpp"

#include "local_ads/campaign_serialization.hpp"
#include "local_ads/config.hpp"
#include "local_ads/file_helpers.hpp"
#include "local_ads/icons_info.hpp"

#include "drape_frontend/drape_engine.hpp"
#include "drape_frontend/visual_params.hpp"

#include "indexer/feature_algo.hpp"
#include "indexer/feature_data.hpp"
#include "indexer/scales.hpp"

#include "platform/http_client.hpp"
#include "platform/marketing_service.hpp"
#include "platform/platform.hpp"
#include "platform/preferred_languages.hpp"
#include "platform/settings.hpp"

#include "coding/file_name_utils.hpp"
#include "coding/multilang_utf8_string.hpp"
#include "coding/url_encode.hpp"
#include "coding/zlib.hpp"

#include "3party/jansson/myjansson.hpp"

#include "private.h"

#include <array>
#include <cstring>
#include <sstream>

// First implementation of local ads statistics serialization
// is deflated JSON.
#define TEMPORARY_LOCAL_ADS_JSON_SERIALIZATION

namespace
{
std::array<char const * const, 5> const kMarketingParameters = {marketing::kFrom, marketing::kType, marketing::kName,
                                                                marketing::kContent, marketing::kKeyword};
std::string const kServerUrl = LOCAL_ADS_SERVER_URL;
std::string const kCampaignPageUrl = LOCAL_ADS_COMPANY_PAGE_URL;

std::string const kCampaignFile = "local_ads_campaigns.dat";
std::string const kLocalAdsSymbolsFile = "local_ads_symbols.txt";
auto constexpr kWWanUpdateTimeout = std::chrono::hours(12);
uint8_t constexpr kRequestMinZoomLevel = 12;
auto constexpr kFailedDownloadingTimeout = std::chrono::seconds(2);
auto constexpr kMaxDownloadingAttempts = 5;

void SerializeCampaign(FileWriter & writer, std::string const & countryName,
                       LocalAdsManager::Timestamp const & ts,
                       std::vector<uint8_t> const & rawData)
{
  local_ads::WriteCountryName(writer, countryName);
  local_ads::WriteTimestamp<std::chrono::hours>(writer, ts);
  local_ads::WriteRawData(writer, rawData);
}

void DeserializeCampaign(ReaderSource<FileReader> & src, std::string & countryName,
                         LocalAdsManager::Timestamp & ts, std::vector<uint8_t> & rawData)
{
  countryName = local_ads::ReadCountryName(src);
  ts = local_ads::ReadTimestamp<std::chrono::hours>(src);
  rawData = local_ads::ReadRawData(src);
}

std::string GetPath(std::string const & fileName)
{
  return my::JoinFoldersToPath(GetPlatform().WritableDir(), fileName);
}

std::string MakeCampaignDownloadingURL(MwmSet::MwmId const & mwmId)
{
  if (kServerUrl.empty() || !mwmId.IsAlive())
    return {};

  std::ostringstream ss;
  auto const campaignDataVersion = static_cast<uint32_t>(local_ads::Version::Latest);
  ss << kServerUrl << "/"
     << campaignDataVersion << "/"
     << mwmId.GetInfo()->GetVersion() << "/"
     << UrlEncode(mwmId.GetInfo()->GetCountryName()) << ".ads";
  return ss.str();
}

using CampaignData = std::map<FeatureID, LocalAdsMarkData>;

CampaignData ParseCampaign(std::vector<uint8_t> const & rawData, MwmSet::MwmId const & mwmId,
                           LocalAdsManager::Timestamp timestamp)
{
  CampaignData data;
  auto campaigns = local_ads::Deserialize(rawData);
  for (local_ads::Campaign const & campaign : campaigns)
  {
    std::string const iconName = campaign.GetIconName();
    auto const expiration = timestamp + std::chrono::hours(24 * campaign.m_daysBeforeExpired);
    if (iconName.empty() || local_ads::Clock::now() > expiration)
      continue;

    LocalAdsMarkData markData;
    markData.m_symbolName = iconName;
    markData.m_minZoomLevel = 10; // TODO(@milchakov): Set value received from server.
    data.insert(std::make_pair(FeatureID(mwmId, campaign.m_featureId), std::move(markData)));
  }

  return data;
}

std::set<FeatureID> ReadCampaignFeatures(LocalAdsManager::ReadFeaturesFn const & reader,
                                         CampaignData & campaignData)
{
  ASSERT(reader != nullptr, ());

  std::set<FeatureID> features;
  for (auto const & data : campaignData)
    features.insert(data.first);

  auto const deviceLang = StringUtf8Multilang::GetLangIndex(languages::GetCurrentNorm());
  reader([&campaignData, deviceLang](FeatureType const & ft)
  {
    auto it = campaignData.find(ft.GetID());
    CHECK(it != campaignData.end(), ());
    it->second.m_position = feature::GetCenter(ft, scales::GetUpperScale());
    ft.GetPreferredNames(true /* allowTranslit */, deviceLang,
                         it->second.m_mainText, it->second.m_auxText);
  }, features);

  return features;
}

void CreateLocalAdsMarks(BookmarkManager * bmManager, CampaignData const & campaignData)
{
  if (bmManager == nullptr)
    return;

  // Here we copy campaign data, because we can create user marks only from UI thread.
  GetPlatform().RunOnGuiThread([bmManager, campaignData]()
  {
    UserMarkNotificationGuard guard(*bmManager, UserMarkType::LOCAL_ADS_MARK);
    for (auto const & data : campaignData)
    {
      auto userMark = guard.m_controller.CreateUserMark(data.second.m_position);
      ASSERT(dynamic_cast<LocalAdsMark *>(userMark) != nullptr, ());
      LocalAdsMark * mark = static_cast<LocalAdsMark *>(userMark);
      mark->SetData(LocalAdsMarkData(data.second));
      mark->SetFeatureId(data.first);
    }
  });
}

void DeleteLocalAdsMarks(BookmarkManager * bmManager, MwmSet::MwmId const & mwmId)
{
  if (bmManager == nullptr)
    return;

  GetPlatform().RunOnGuiThread([bmManager, mwmId]()
  {
    UserMarkNotificationGuard guard(*bmManager, UserMarkType::LOCAL_ADS_MARK);
    for (size_t i = 0; i < guard.m_controller.GetUserMarkCount();)
    {
      auto userMark = guard.m_controller.GetUserMark(i);
      ASSERT(dynamic_cast<LocalAdsMark const *>(userMark) != nullptr, ());
      LocalAdsMark const * mark = static_cast<LocalAdsMark const *>(userMark);
      if (mark->GetFeatureID().m_mwmId == mwmId)
        guard.m_controller.DeleteUserMark(i);
      else
        ++i;
    }
  });
}

void DeleteAllLocalAdsMarks(BookmarkManager * bmManager)
{
  if (bmManager == nullptr)
    return;

  GetPlatform().RunOnGuiThread([bmManager]()
  {
    UserMarkNotificationGuard guard(*bmManager, UserMarkType::LOCAL_ADS_MARK);
    guard.m_controller.Clear();
  });
}

#ifdef TEMPORARY_LOCAL_ADS_JSON_SERIALIZATION
std::vector<uint8_t> SerializeLocalAdsToJSON(std::list<local_ads::Event> const & events,
                                             std::string const & userId, std::string & contentType,
                                             std::string & contentEncoding)
{
  using namespace std::chrono;
  ASSERT(!events.empty(), ());
  auto root = my::NewJSONObject();
  ToJSONObject(*root, "userId", userId);
  ToJSONObject(*root, "countryId", events.front().m_countryId);
  ToJSONObject(*root, "mwmVersion", events.front().m_mwmVersion);
  auto eventsNode = my::NewJSONArray();
  for (auto const & event : events)
  {
    auto eventNode = my::NewJSONObject();
    auto s = duration_cast<seconds>(event.m_timestamp.time_since_epoch()).count();
    ToJSONObject(*eventNode, "type", static_cast<uint8_t>(event.m_type));
    ToJSONObject(*eventNode, "timestamp", static_cast<int64_t>(s));
    ToJSONObject(*eventNode, "featureId", static_cast<int32_t>(event.m_featureId));
    ToJSONObject(*eventNode, "zoomLevel", event.m_zoomLevel);
    ToJSONObject(*eventNode, "latitude", event.m_latitude);
    ToJSONObject(*eventNode, "longitude", event.m_longitude);
    ToJSONObject(*eventNode, "accuracyInMeters", event.m_accuracyInMeters);
    json_array_append_new(eventsNode.get(), eventNode.release());
  }
  json_object_set_new(root.get(), "events", eventsNode.release());
  std::unique_ptr<char, JSONFreeDeleter> buffer(
      json_dumps(root.get(), JSON_COMPACT | JSON_ENSURE_ASCII));
  std::vector<uint8_t> result;

  using Deflate = coding::ZLib::Deflate;
  Deflate deflate(Deflate::Format::ZLib, Deflate::Level::BestCompression);
  deflate(buffer.get(), strlen(buffer.get()), std::back_inserter(result));
  return result;
}
#endif

std::string MakeCampaignPageURL(FeatureID const & featureId)
{
  if (kCampaignPageUrl.empty() || !featureId.m_mwmId.IsAlive())
    return {};

  std::ostringstream ss;
  ss << kCampaignPageUrl << "/" << featureId.m_mwmId.GetInfo()->GetVersion() << "/"
     << UrlEncode(featureId.m_mwmId.GetInfo()->GetCountryName()) << "/" << featureId.m_index;

  bool isFirstParam = true;
  for (auto const & key : kMarketingParameters)
  {
    string value;
    if (!marketing::Settings::Get(key, value))
      continue;

    if (isFirstParam)
    {
      ss << "?";
      isFirstParam = false;
    }
    else
    {
      ss << "&";
    }
    ss << key << "=" << value;
  }

  return ss.str();
}
}  // namespace

LocalAdsManager::LocalAdsManager(GetMwmsByRectFn && getMwmsByRectFn,
                                 GetMwmIdByNameFn && getMwmIdByName,
                                 ReadFeaturesFn && readFeaturesFn)
  : m_getMwmsByRectFn(std::move(getMwmsByRectFn))
  , m_getMwmIdByNameFn(std::move(getMwmIdByName))
  , m_readFeaturesFn(std::move(readFeaturesFn))
  , m_bmManager(nullptr)
{
  CHECK(m_getMwmsByRectFn != nullptr, ());
  CHECK(m_getMwmIdByNameFn != nullptr, ());
  CHECK(m_readFeaturesFn != nullptr, ());

  m_statistics.SetUserId(GetPlatform().UniqueClientId());

#ifdef TEMPORARY_LOCAL_ADS_JSON_SERIALIZATION
  using namespace std::placeholders;
  m_statistics.SetCustomServerSerializer(std::bind(&SerializeLocalAdsToJSON, _1, _2, _3, _4));
#endif
}

LocalAdsManager::~LocalAdsManager()
{
  std::lock_guard<std::mutex> lock(m_mutex);
  ASSERT(!m_isRunning, ());
}

void LocalAdsManager::Startup()
{
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_isRunning)
      return;
    m_isRunning = true;
  }
  FillSupportedTypes();

  m_thread = threads::SimpleThread(&LocalAdsManager::ThreadRoutine, this);

  m_statistics.Startup();
}

void LocalAdsManager::Teardown()
{
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_isRunning)
      return;
    m_isRunning = false;
  }
  m_condition.notify_one();
  m_thread.join();

  m_statistics.Teardown();
}

void LocalAdsManager::SetBookmarkManager(BookmarkManager * bmManager)
{
  m_bmManager = bmManager;
}

void LocalAdsManager::SetDrapeEngine(ref_ptr<df::DrapeEngine> engine)
{
  {
    std::lock_guard<std::mutex> lock(m_drapeEngineMutex);
    m_drapeEngine = engine;
  }
  UpdateFeaturesCache({});
}

void LocalAdsManager::UpdateViewport(ScreenBase const & screen)
{
  auto connectionStatus = GetPlatform().ConnectionStatus();
  if (kServerUrl.empty() || connectionStatus == Platform::EConnectionType::CONNECTION_NONE ||
      df::GetZoomLevel(screen.GetScale()) < kRequestMinZoomLevel)
  {
    return;
  }

  std::vector<std::string> requestedCampaigns;
  auto mwms = m_getMwmsByRectFn(screen.ClipRect());
  if (mwms.empty())
    return;

  // Request local ads campaigns.
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_isRunning)
      return;

    for (auto const & mwm : mwms)
    {
      auto info = mwm.GetInfo();
      if (!info)
        continue;

      // Skip downloading request if maximum attempts count has reached or
      // we are waiting for new attempt.
      auto const failedDownloadsIt = m_failedDownloads.find(mwm);
      if (failedDownloadsIt != m_failedDownloads.cend() &&
          (failedDownloadsIt->second.m_attemptsCount >= kMaxDownloadingAttempts ||
           std::chrono::steady_clock::now() <= failedDownloadsIt->second.m_lastDownloading +
                                               failedDownloadsIt->second.m_currentTimeout))
      {
        continue;
      }

      std::string const & mwmName = info->GetCountryName();
      auto campaignIt = m_campaigns.find(mwmName);
      if (campaignIt == m_campaigns.end())
      {
        requestedCampaigns.push_back(mwmName);
        continue;
      }

      // If a campaign has not been requested from server this session.
      if (!campaignIt->second)
      {
        auto const it = m_info.find(mwmName);
        bool needUpdateByTimeout = (connectionStatus == Platform::EConnectionType::CONNECTION_WIFI);
        if (!needUpdateByTimeout && it != m_info.end())
          needUpdateByTimeout = local_ads::Clock::now() > (it->second.m_created + kWWanUpdateTimeout);

        if (needUpdateByTimeout || it == m_info.end())
          requestedCampaigns.push_back(mwmName);
      }
    }

    if (!requestedCampaigns.empty())
    {
      for (auto const & campaign : requestedCampaigns)
        m_requestedCampaigns.insert(std::make_pair(m_getMwmIdByNameFn(campaign), RequestType::Download));
      m_condition.notify_one();
    }
  }
}

bool LocalAdsManager::DownloadCampaign(MwmSet::MwmId const & mwmId, std::vector<uint8_t> & bytes)
{
  bytes.clear();

  std::string const url = MakeCampaignDownloadingURL(mwmId);
  if (url.empty())
    return true; // In this case empty result is valid.

  // Skip already downloaded campaigns. We do not lock whole method because RunHttpRequest
  // is a synchronous method and may take a lot of time. The case in which countryName will
  // be added to m_campaigns between locks is neglected.
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto const & countryName = mwmId.GetInfo()->GetCountryName();
    auto const it = m_campaigns.find(countryName);
    if (it != m_campaigns.cend() && it->second)
      return false;
  }

  platform::HttpClient request(url);
  bool const success = request.RunHttpRequest() && request.ErrorCode() == 200;

  std::lock_guard<std::mutex> lock(m_mutex);
  if (!success)
  {
    auto const it = m_failedDownloads.find(mwmId);
    if (it == m_failedDownloads.cend())
    {
      m_failedDownloads.insert(std::make_pair(mwmId, BackoffStats(std::chrono::steady_clock::now(),
                                                                  kFailedDownloadingTimeout,
                                                                  1 /* m_attemptsCount */)));
    }
    else
    {
      // Here we increase timeout multiplying by 2.
      it->second.m_currentTimeout = std::chrono::seconds(it->second.m_currentTimeout.count() * 2);
      it->second.m_attemptsCount++;
    }
    return false;
  }

  m_failedDownloads.erase(mwmId);
  auto const & response = request.ServerResponse();
  bytes.assign(response.cbegin(), response.cend());
  return true;
}

void LocalAdsManager::ThreadRoutine()
{
  local_ads::IconsInfo::Instance().SetSourceFile(kLocalAdsSymbolsFile);

  std::string const campaignFile = GetPath(kCampaignFile);

  // Read persistence data.
  ReadCampaignFile(campaignFile);
  Invalidate();

  std::set<Request> campaignMwms;
  while (WaitForRequest(campaignMwms))
  {
    for (auto const & mwm : campaignMwms)
    {
      if (!mwm.first.IsAlive())
        continue;

      std::string const countryName = mwm.first.GetInfo()->GetCountryName();
      if (mwm.second == RequestType::Download)
      {
        // Download campaign data from server.
        CampaignInfo info;
        info.m_created = local_ads::Clock::now();
        if (!DownloadCampaign(mwm.first, info.m_data))
          continue;

        // Parse data and recreate marks.
        ClearLocalAdsForMwm(mwm.first);
        if (!info.m_data.empty())
        {
          auto campaignData = ParseCampaign(std::move(info.m_data), mwm.first, info.m_created);
          if (!campaignData.empty())
          {
            UpdateFeaturesCache(ReadCampaignFeatures(m_readFeaturesFn, campaignData));
            CreateLocalAdsMarks(m_bmManager, campaignData);
          }
        }

        // Update run-time data.
        {
          std::lock_guard<std::mutex> lock(m_mutex);
          m_campaigns[countryName] = true;
          m_info[countryName] = info;
        }
      }
      else if (mwm.second == RequestType::Delete)
      {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_campaigns.erase(countryName);
        m_info.erase(countryName);
        ClearLocalAdsForMwm(mwm.first);
      }
    }
    campaignMwms.clear();

    // Save data persistently.
    WriteCampaignFile(campaignFile);
  }
}

bool LocalAdsManager::WaitForRequest(std::set<Request> & campaignMwms)
{
  std::unique_lock<std::mutex> lock(m_mutex);

  m_condition.wait(lock, [this] {return !m_isRunning || !m_requestedCampaigns.empty();});
  if (!m_isRunning)
    return false;

  campaignMwms = std::move(m_requestedCampaigns);
  m_requestedCampaigns.clear();
  return true;
}

void LocalAdsManager::OnDownloadCountry(std::string const & countryName)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  m_campaigns.erase(countryName);
  m_info.erase(countryName);
}

void LocalAdsManager::OnDeleteCountry(std::string const & countryName)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  m_requestedCampaigns.insert(std::make_pair(m_getMwmIdByNameFn(countryName), RequestType::Delete));
  m_condition.notify_one();
}

void LocalAdsManager::ReadCampaignFile(std::string const & campaignFile)
{
  if (!GetPlatform().IsFileExistsByFullPath(campaignFile))
    return;

  std::lock_guard<std::mutex> lock(m_mutex);
  try
  {
    FileReader reader(campaignFile, true /* withExceptions */);
    ReaderSource<FileReader> src(reader);
    while (src.Size() > 0)
    {
      std::string countryName;
      CampaignInfo info;
      DeserializeCampaign(src, countryName, info.m_created, info.m_data);
      m_info[countryName] = info;
      m_campaigns[countryName] = false;
    }
  }
  catch (Reader::Exception const & ex)
  {
    LOG(LWARNING, ("Error reading file:", campaignFile, ex.Msg()));
    FileWriter::DeleteFileX(campaignFile);
    m_info.clear();
    m_campaigns.clear();
  }
}

void LocalAdsManager::WriteCampaignFile(std::string const & campaignFile)
{
  try
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    FileWriter writer(campaignFile);
    for (auto const & info : m_info)
      SerializeCampaign(writer, info.first, info.second.m_created, info.second.m_data);
  }
  catch (RootException const & ex)
  {
    LOG(LWARNING, ("Error writing file:", campaignFile, ex.Msg()));
  }
}

void LocalAdsManager::Invalidate()
{
  DeleteAllLocalAdsMarks(m_bmManager);
  {
    std::lock_guard<std::mutex> lock(m_drapeEngineMutex);
    if (m_drapeEngine != nullptr)
      m_drapeEngine->RemoveAllCustomFeatures();
  }

  CampaignData campaignData;
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto const & info : m_info)
    {
      auto data = ParseCampaign(info.second.m_data, m_getMwmIdByNameFn(info.first),
                                info.second.m_created);
      campaignData.insert(data.begin(), data.end());
    }
  }
  UpdateFeaturesCache(ReadCampaignFeatures(m_readFeaturesFn, campaignData));
  CreateLocalAdsMarks(m_bmManager, campaignData);
}

void LocalAdsManager::UpdateFeaturesCache(std::set<FeatureID> && ids)
{
  std::set<FeatureID> featuresCache;
  {
    std::lock_guard<std::mutex> lock(m_featuresCacheMutex);
    if (!ids.empty())
      m_featuresCache.insert(ids.begin(), ids.end());
    featuresCache = m_featuresCache;
  }
  {
    std::lock_guard<std::mutex> lock(m_drapeEngineMutex);
    if (m_drapeEngine != nullptr)
      m_drapeEngine->SetCustomFeatures(std::move(featuresCache));
  }
}

void LocalAdsManager::ClearLocalAdsForMwm(MwmSet::MwmId const &mwmId)
{
  // Clear feature cache.
  {
    std::lock_guard<std::mutex> lock(m_featuresCacheMutex);
    for (auto it = m_featuresCache.begin(); it != m_featuresCache.end();)
    {
      if (it->m_mwmId == mwmId)
        it = m_featuresCache.erase(it);
      else
        ++it;
    }
  }

  // Remove custom features in graphics engine.
  {
    std::lock_guard<std::mutex> lock(m_drapeEngineMutex);
    if (m_drapeEngine != nullptr)
      m_drapeEngine->RemoveCustomFeatures(mwmId);
  }

  // Delete marks.
  DeleteLocalAdsMarks(m_bmManager, mwmId);
}

bool LocalAdsManager::Contains(FeatureID const & featureId) const
{
  std::lock_guard<std::mutex> lock(m_featuresCacheMutex);
  return m_featuresCache.find(featureId) != m_featuresCache.cend();
}

bool LocalAdsManager::IsSupportedType(feature::TypesHolder const & types) const
{
  return m_supportedTypes.Contains(types);
}

std::string LocalAdsManager::GetCompanyUrl(FeatureID const & featureId) const
{
  return MakeCampaignPageURL(featureId);
}
