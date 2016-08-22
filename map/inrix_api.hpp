#pragma once

#include "drape_frontend/traffic_generator.hpp"

#include "platform/http_request.hpp"

#include "geometry/point2d.hpp"

#include "std/chrono.hpp"
#include "std/function.hpp"
#include "std/initializer_list.hpp"
#include "std/mutex.hpp"
#include "std/string.hpp"
#include "std/unique_ptr.hpp"
#include "std/unordered_map.hpp"
#include "std/utility.hpp"
#include "std/vector.hpp"

class InrixApi
{
public:
  struct SegmentData
  {
    string m_id;
    vector<m2::PointD> m_points;
  };

  InrixApi();
  void Authenticate();

  using TTrafficCallback = function<void(vector<df::TrafficSegmentData> const & data)>;
  void GetTraffic(TTrafficCallback const & trafficCallback);

  vector<SegmentData> GetSegments();

private:
  string MakeApiUrl(string const & action, initializer_list<pair<string, string>> const & params);
  void GetTrafficInternal(TTrafficCallback const & trafficCallback);

  string m_regionId;
  string m_authToken;
  system_clock::time_point m_expirationTimestamp;
  unordered_map<string, string> m_serverUrls;

  unique_ptr<downloader::HttpRequest> m_authRequest;
  unique_ptr<downloader::HttpRequest> m_trafficRequest;

  mutex m_mutex;
  TTrafficCallback m_deferredCallback;
};
