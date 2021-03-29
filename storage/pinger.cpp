#include "storage/pinger.hpp"

#include "platform/http_client.hpp"
#include "platform/preferred_languages.hpp"

#include "base/assert.hpp"
#include "base/logging.hpp"
#include "base/stl_helpers.hpp"
#include "base/thread_pool_delayed.hpp"

#include "3party/Alohalytics/src/alohalytics.h"

#include <chrono>
#include <map>
#include <sstream>
#include <utility>

using namespace std;
using namespace std::chrono;

namespace
{
auto constexpr kTimeoutInSeconds = 5.0;

using ResponseTimeAndUrl = pair<int32_t, string>;

void DoPing(string const & url, size_t index, vector<ResponseTimeAndUrl> & responseTimeAndUrls)
{
  if (url.empty())
  {
    ASSERT(false, ("Metaserver returned an empty url."));
    return;
  }

  platform::HttpClient request(url);
  request.SetHttpMethod("HEAD");
  request.SetTimeout(kTimeoutInSeconds);
  auto begin = high_resolution_clock::now();
  if (request.RunHttpRequest() && !request.WasRedirected() && request.ErrorCode() == 200)
  {
    auto end = high_resolution_clock::now();
    auto responseTime = duration_cast<milliseconds>(end - begin).count();
    responseTimeAndUrls[index] = {responseTime, url};
  }
  else
  {
    ostringstream ost;
    ost << "Request to server " << url << " failed. Code = " << request.ErrorCode()
        << ", redirection = " << request.WasRedirected();
    LOG(LINFO, (ost.str()));
  }
}

void SendStatistics(size_t serversLeft)
{
  alohalytics::Stats::Instance().LogEvent(
      "Downloader_ServerList_check",
      {{"lang", languages::GetCurrentNorm()}, {"servers", to_string(serversLeft)}});
}
}  // namespace

namespace storage
{
// static
Pinger::Endpoints Pinger::ExcludeUnavailableEndpoints(Endpoints const & urls)
{
  using base::thread_pool::delayed::ThreadPool;

  auto const size = urls.size();
  CHECK_GREATER(size, 0, ());

  vector<ResponseTimeAndUrl> responseTimeAndUrls(size);
  {
    ThreadPool t(size, ThreadPool::Exit::ExecPending);
    for (size_t i = 0; i < size; ++i)
      t.Push([url = urls[i], &responseTimeAndUrls, i] { DoPing(url, i, responseTimeAndUrls); });
  }

  sort(responseTimeAndUrls.begin(), responseTimeAndUrls.end());

  vector<string> readyUrls;
  for (auto & [_, url] : responseTimeAndUrls)
  {
    readyUrls.push_back(move(url));
  }

  SendStatistics(readyUrls.size());
  return readyUrls;
}
}  // namespace storage
