#include "geochats_api.hpp"

#include "platform/http_client.hpp"

#include "base/exception.hpp"
#include "base/logging.hpp"
#include "base/thread.hpp"

#include "std/ctime.hpp"

#include "3party/jansson/myjansson.hpp"

using namespace platform;

namespace
{
using namespace geochats;

string const kIcqServerUrl = "https://rapi.icq.net";
string const kIcqChatUrl = "https://icq.com/chat/";
auto const kCashExpirySec = 30;
auto const kMinCountOfMembers = 5;

string MakeRequest(ms::LatLon const pos, uint32_t const radius, string newTag)
{
  /// Request id should be unique.
  static atomic<size_t> requestId(0);
  stringstream ss;
  ss << R"({"method": "findChatByGeoMapsMe",)"
     << R"("reqId" : ")" << ++requestId << "-" << time(nullptr)
     << R"(", "params" :  {"geo": { )"
     << R"("lon": )" << pos.lon <<","
     << R"("lat": )" << pos.lat
     << R"(},"radius": )" << radius << ","
     << R"("token": "mapsme_fa14dca1e894072d1e36c046e7f9e074")";

  if (!newTag.empty())
    ss << R"(,"tag": ")" << newTag << "\"";

  ss << "}}";
  return ss.str();
}

bool CheckResponseStatus(my::Json const & response)
{
  auto const status = json_object_get(response.get(), "status");

  if (status == nullptr)
  {
    LOG(LERROR, ("Status field is not found"));
    return false;
  }

  json_int_t code;
  my::FromJSONObject(status, "code", code);
  auto errorCode = strings::to_string(code);

  if (!errorCode.empty() && errorCode.front() == '2')  // 2 - ok, 4 - client error, 5 - server error
    return true;

  return false;
}

json_t * GetResultsSection(my::Json const & response)
{
  if (!CheckResponseStatus(response))
    return nullptr;

  json_t * results = json_object_get(response.get(), "results");

  if (results == nullptr)
  {
    LOG(LERROR, ("Results section is not found"));
    return nullptr;
  }

  return results;
}

json_t * GetChatsJsonArray(json_t const * source)
{
  if (source == nullptr)
    return nullptr;

  json_t * chats = json_object_get(source, "chats");

  if (chats == nullptr)
  {
    LOG(LERROR, ("Chats section is not found"));
    return nullptr;
  }

  if (!json_is_array(chats))
  {
    LOG(LERROR, ("Chats section is not an array"));
    return nullptr;
  }

  return chats;
}

void LogJsonWarning(string const & msg, json_t const * item)
{
  ASSERT(item, ());

  auto const itemDump = json_dumps(item, JSON_ENCODE_ANY);
  if (itemDump != nullptr)
    LOG(LWARNING, (msg, "Full json item dump:", itemDump));
  else
    LOG(LWARNING, (msg));
}

bool AddChats(json_t const * source, vector<geochats::ChatInfo> & geochatsResult)
{
  auto const chats = GetChatsJsonArray(source);
  if (chats == nullptr)
    return false;

  for (size_t i = 0; i < json_array_size(chats); ++i)
  {
    auto const item = json_array_get(chats, i);
    ASSERT(item, ());
    // no need to load in case when no members
    if (json_object_get(item, "membersCount") == nullptr)
      continue;

    geochats::ChatInfo chatInfo;
    try
    {
      my::FromJSONObject(item, "membersCount", chatInfo.m_membersCount);
      /// Do not show chats with small count of members.
      if (chatInfo.m_membersCount < kMinCountOfMembers)
        continue;

      auto const geo = json_object_get(item, "geo");
      my::FromJSONObject(item, "stamp", chatInfo.m_id);
      my::FromJSONObject(item, "name", chatInfo.m_name);
      my::FromJSONObject(geo, "lat", chatInfo.m_pos.lat);
      my::FromJSONObject(geo, "lon", chatInfo.m_pos.lon);
    }
    catch (RootException const & ex)
    {
      LogJsonWarning(ex.Msg(), item);
      continue;
    }
    geochatsResult.push_back(move(chatInfo));
  }

  return true;
}

void GetChatsAsync(ms::LatLon const pos, uint32_t const radius, uint64_t const reqId,
                   RequestHolderPtr requestHolder, ChatsCallback const fn)
{
  vector<ChatInfo> geochats;
  bool finish = false;
  string newTag = "";

  if (requestHolder->GetFromCash(pos, radius, geochats))
  {
    fn(geochats, reqId);
    return;
  }

  /// Load chats page by page.
  do
  {
    if (!requestHolder->RequestIsActive(reqId))
      return;

    auto const rawResponse = RawApi::GetGeochats(pos, radius, newTag);
    if (rawResponse.empty())
      break;

    my::Json answer(rawResponse.c_str());

    auto const results = GetResultsSection(answer);
    if (results == nullptr)
      break;

    bool restart = false;
    my::FromJSONObjectOptionalField(results, "restart", restart);

    if (restart)
    {
      LOG(LINFO, ("Restart command was received, chats will be cleared:"));
      geochats.clear();
    }

    my::FromJSONObjectOptionalField(results, "finish", finish);
    my::FromJSONObjectOptionalField(results, "newTag", newTag);

    if (!AddChats(results, geochats))
      break;
  } while (!finish && !newTag.empty());

  requestHolder->AddToCash(pos, radius, geochats);
  fn(geochats, reqId);
}
}  // namespace

namespace geochats
{
// static
string RawApi::GetGeochats(ms::LatLon const & pos, uint32_t const radius,
                           string const & newTag /* = "" */)
{
  HttpClient request(kIcqServerUrl);
  request.SetHttpMethod("POST");
  request.SetBodyData(MakeRequest(pos, radius, newTag), "application/json", "POST", "gzip");

  if (request.RunHttpRequest() && request.ErrorCode() == 200)
    return request.ServerResponse();

  return {};
}

uint64_t RequestHolder::ResetId()
{
  return ++m_requestId;
}

bool RequestHolder::RequestIsActive(uint64_t const reqId) const
{
  return m_requestId == reqId;
}

void RequestHolder::AddToCash(ms::LatLon const & pos, uint32_t const radius,
                              vector<ChatInfo> const & chats)
{
  lock_guard<mutex> lock(m_mutex);

  m_pos = pos;
  m_radius = radius;
  m_chats = chats;
  m_cashStoredTime = time(nullptr);
}

bool RequestHolder::GetFromCash(ms::LatLon const & pos, uint32_t const radius,
                                vector<ChatInfo> & chats)
{
  lock_guard<mutex> lock(m_mutex);

  if (!m_pos.EqualDxDy(pos, 0.000005) || m_radius != radius ||
      m_cashStoredTime - time(nullptr) > kCashExpirySec)
    return false;

  chats = m_chats;
  return true;
}

Api::~Api()
{
  m_requestHolder->ResetId();
}

uint64_t Api::GetChats(ms::LatLon const & pos, uint32_t const radius, ChatsCallback const & fn)
{
  uint64_t const reqId = m_requestHolder->ResetId();

  threads::SimpleThread(GetChatsAsync, pos, radius, reqId, m_requestHolder, fn).detach();
  return reqId;
}

// static
string Api::GetChatLink(string const & chatId)
{
  return kIcqChatUrl + chatId;
}
}  // geochats
