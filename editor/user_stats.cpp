#include "editor/user_stats.hpp"

#include "platform/platform.hpp"
#include "platform/settings.hpp"

#include "coding/url_encode.hpp"

#include "base/logging.hpp"
#include "base/thread.hpp"
#include "base/timer.hpp"

#include "3party/Alohalytics/src/http_client.h"
#include "3party/pugixml/src/pugixml.hpp"

using TRequest = alohalytics::HTTPClientPlatformWrapper;

namespace
{
string const kUserStatsUrl = "http://py.osmz.ru/mmwatch/user?format=xml";
int32_t constexpr kUninitialized = -1;

auto constexpr kSettingsUserName = "LastLoggedUser";
auto constexpr kSettingsRating = "UserEditorRating";
auto constexpr kSettingsChangesCount = "UserEditorChangesCount";
auto constexpr kSettingsLastUpdate = "UserEditorLastUpdate";

auto constexpr kSecondsInHour = 60 * 60;
}  // namespace

namespace editor
{
// UserStat ----------------------------------------------------------------------------------------

UserStats::UserStats()
  : m_changesCount(kUninitialized), m_rank(kUninitialized)
  , m_updateTime(my::SecondsSinceEpochToTimeT(0)), m_valid(false)
{
}

UserStats::UserStats(time_t const updateTime, uint32_t const rating,
                     uint32_t const changesCount, string const & levelUpFeat)
  : m_changesCount(changesCount), m_rank(rating)
  , m_updateTime(updateTime), m_levelUpRequiredFeat(levelUpFeat)
  , m_valid(true)
{
}

bool UserStats::GetChangesCount(int32_t & changesCount) const
{
  if (m_changesCount == kUninitialized)
    return false;
  changesCount = m_changesCount;
  return true;
}

bool UserStats::GetRank(int32_t & rank) const
{
  if (m_rank == kUninitialized)
    return false;
  rank = m_rank;
  return true;
}

bool UserStats::GetLevelUpRequiredFeat(string & levelUpFeat)  const
{
  if (m_levelUpRequiredFeat.empty())
    return false;
  levelUpFeat = m_levelUpRequiredFeat;
  return true;
}

// UserStatsLoader ---------------------------------------------------------------------------------

UserStatsLoader::UserStatsLoader()
  : m_lastUpdate(my::SecondsSinceEpochToTimeT(0))
{
  if (!LoadFromSettings())
    LOG(LINFO, ("There is no cached user stats info in settings"));
  else
    LOG(LINFO, ("User stats info was loaded successfully"));
}

bool UserStatsLoader::Update(string const & userName)
{
  {
    lock_guard<mutex> g(m_mutex);
    m_userName = userName;
  }

  auto const url = kUserStatsUrl + "&name=" + UrlEncode(userName);
  TRequest request(url);

  if (!request.RunHTTPRequest())
  {
    LOG(LWARNING, ("Network error while connecting to", url));
    return false;
  }

  if (request.error_code() != 200)
  {
    LOG(LWARNING, ("Server returned", request.error_code(), "for url", url));
    return false;
  }

  auto const response = request.server_response();

  pugi::xml_document document;
  if (!document.load_buffer(response.data(), response.size()))
  {
    LOG(LWARNING, ("Cannot parse server response:", response));
    return false;
  }

  auto changesCount = document.select_node("mmwatch/edits/@value").attribute().as_int(-1);
  auto rank = document.select_node("mmwatch/rank/@value").attribute().as_int(-1);
  auto levelUpFeat = document.select_node("mmwatch/levelUpFeat/@value").attribute().as_string();

  lock_guard<mutex> g(m_mutex);
  if (m_userName != userName)
    return false;

  m_lastUpdate = time(nullptr);
  m_userStats = UserStats(m_lastUpdate, rank, changesCount, levelUpFeat);
  SaveToSettings();

  return true;
}

void UserStatsLoader::Update(string const & userName, TOnUpdateCallback fn)
{
  auto nothingToUpdate = false;
  {
    lock_guard<mutex> g(m_mutex);
    nothingToUpdate = m_userStats && m_userName == userName && m_userStats &&
                      difftime(m_lastUpdate, time(nullptr)) < kSecondsInHour;
  }

  if (nothingToUpdate)
  {
    GetPlatform().RunOnGuiThread(fn);
    return;
  }

  threads::SimpleThread([this, userName, fn] {
    if (Update(userName))
      GetPlatform().RunOnGuiThread(fn);
  }).detach();
}

void UserStatsLoader::DropStats(string const & userName)
{
  lock_guard<mutex> g(m_mutex);
  if (m_userName != userName)
    return;
  m_userStats = {};
  DropSettings();
}

UserStats UserStatsLoader::GetStats(string const & userName) const
{
  lock_guard<mutex> g(m_mutex);
  if (m_userName == userName)
    return m_userStats;
  return {};
}

string UserStatsLoader::GetUserName() const
{
  lock_guard<mutex> g(m_mutex);
  return m_userName;
}

bool UserStatsLoader::LoadFromSettings()
{
  uint32_t rating, changesCount;
  uint64_t lastUpdate;
  if (!settings::Get(kSettingsUserName, m_userName) ||
      !settings::Get(kSettingsChangesCount, changesCount) ||
      !settings::Get(kSettingsRating, rating) ||
      !settings::Get(kSettingsLastUpdate, lastUpdate))
  {
    return false;
  }

  m_lastUpdate = my::SecondsSinceEpochToTimeT(lastUpdate);
  m_userStats = UserStats(m_lastUpdate, rating, changesCount, "");
  return true;
}

void UserStatsLoader::SaveToSettings()
{
  if (!m_userStats)
    return;

  settings::Set(kSettingsUserName, m_userName);
  int32_t rank;
  if (m_userStats.GetRank(rank))
    settings::Set(kSettingsRating, rank);
  int32_t changesCount;
  if (m_userStats.GetChangesCount(changesCount))
    settings::Set(kSettingsChangesCount, changesCount);
  settings::Set(kSettingsLastUpdate, my::TimeTToSecondsSinceEpoch(m_lastUpdate));
  // Do not save m_requiredLevelUpFeat for it becomes obsolete very fast.
}

void UserStatsLoader::DropSettings()
{
  settings::Delete(kSettingsUserName);
  settings::Delete(kSettingsRating);
  settings::Delete(kSettingsChangesCount);
  settings::Delete(kSettingsLastUpdate);
}
}  // namespace editor
