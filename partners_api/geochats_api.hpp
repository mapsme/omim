#pragma once

#include "geometry/latlon.hpp"

#include "std/atomic.hpp"
#include "std/function.hpp"
#include "std/mutex.hpp"
#include "std/shared_ptr.hpp"
#include "std/string.hpp"
#include "std/vector.hpp"

namespace geochats
{
struct ChatInfo
{
  string m_id;
  string m_name;
  ms::LatLon m_pos;
  long long m_membersCount;
};

class RawApi
{
public:
  /// Returns vector of geochats which are located near propagated position.
  static string GetGeochats(ms::LatLon const & pos, uint32_t const radius,
                            string const & newTag = "");
};

using ChatsCallback = function<void(vector<ChatInfo> const & chats, uint64_t const requestId)>;

/// Class which contains actual request ID, generates next ID and contains last request result.
class RequestHolder
{
public:
  uint64_t ResetId();
  bool RequestIsActive(uint64_t const reqId) const;

  void AddToCash(ms::LatLon const & pos, uint32_t const radius, vector<ChatInfo> const & chats);
  bool GetFromCash(ms::LatLon const & pos, uint32_t const radius, vector<ChatInfo> & chats);

private:
  atomic<uint64_t> m_requestId{0};

  /// Synchronize AddToCash/GetFromCash
  mutex m_mutex;

  time_t m_cashStoredTime = 0;
  ms::LatLon m_pos = {0.0, 0.0};
  uint32_t m_radius = 0;
  vector<ChatInfo> m_chats;
};

using RequestHolderPtr = shared_ptr<RequestHolder>;

class Api
{
public:
  ~Api();
  /// Returns vector of geochats which are located near propagated position.
  uint64_t GetChats(ms::LatLon const & pos, uint32_t const radius, ChatsCallback const & fn);
  /// Get link to icq chat.
  static string GetChatLink(string const & chatId);

private:
  RequestHolderPtr m_requestHolder = make_shared<RequestHolder>();
};
}  // namespace geochats
