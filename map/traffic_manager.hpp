#pragma once

#include "traffic/traffic_info.hpp"

#include "drape_frontend/traffic_generator.hpp"

#include "drape/pointers.hpp"

#include "geometry/point2d.hpp"
#include "geometry/polyline2d.hpp"
#include "geometry/screenbase.hpp"

#include "indexer/index.hpp"
#include "indexer/mwm_set.hpp"

#include "base/thread.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace df
{
class DrapeEngine;
}  // namespace df

class TrafficManager final
{
public:
  enum class TrafficState
  {
    Disabled,
    Enabled,
    WaitingData,
    Outdated,
    NoData,
    NetworkError,
    ExpiredData,
    ExpiredApp
  };

  struct MyPosition
  {
    m2::PointD m_position = m2::PointD(0.0, 0.0);
    bool m_knownPosition = false;

    MyPosition() = default;
    MyPosition(m2::PointD const & position)
      : m_position(position),
        m_knownPosition(true)
    {}
  };

  using TrafficStateChangedFn = function<void(TrafficState)>;
  using GetMwmsByRectFn = function<std::vector<MwmSet::MwmId>(m2::RectD const &)>;

  TrafficManager(GetMwmsByRectFn const & getMwmsByRectFn, size_t maxCacheSizeBytes,
                 traffic::TrafficObserver & observer);
  TrafficManager(TrafficManager && /* trafficManager */) = default;
  ~TrafficManager();

  void Teardown();

  void SetStateListener(TrafficStateChangedFn const & onStateChangedFn);
  void SetDrapeEngine(ref_ptr<df::DrapeEngine> engine);
  void SetCurrentDataVersion(int64_t dataVersion);

  void SetEnabled(bool enabled);

  void UpdateViewport(ScreenBase const & screen);
  void UpdateMyPosition(MyPosition const & myPosition);

  void Invalidate();

  void OnDestroyGLContext();
  void OnRecoverGLContext();
  void OnMwmDelete(MwmSet::MwmId const & mwmId);

  void OnEnterForeground();
  void OnEnterBackground();

  void SetSimplifiedColorScheme(bool simplified);

private:
  struct CacheEntry
  {
    CacheEntry();
    CacheEntry(std::chrono::time_point<std::chrono::steady_clock> const & requestTime);

    bool m_isLoaded;
    size_t m_dataSize;

    std::chrono::time_point<std::chrono::steady_clock> m_lastActiveTime;
    std::chrono::time_point<std::chrono::steady_clock> m_lastRequestTime;
    std::chrono::time_point<std::chrono::steady_clock> m_lastResponseTime;

    int m_retriesCount;
    bool m_isWaitingForResponse;

    traffic::TrafficInfo::Availability m_lastAvailability;
  };

  void ThreadRoutine();
  bool WaitForRequest(std::vector<MwmSet::MwmId> & mwms);

  void OnTrafficDataResponse(traffic::TrafficInfo && info);
  void OnTrafficRequestFailed(traffic::TrafficInfo && info);

  /// \brief Updates |activeMwms| and request traffic data.
  /// \param rect is a rectangle covering a new active mwm set.
  /// \note |lastMwmsByRect|/|activeMwms| may be either |m_lastDrapeMwmsByRect/|m_activeDrapeMwms|
  /// or |m_lastRoutingMwmsByRect|/|m_activeRoutingMwms|.
  /// \note |m_mutex| is locked inside the method. So the method should be called without |m_mutex|.
  void UpdateActiveMwms(m2::RectD const & rect, std::vector<MwmSet::MwmId> & lastMwmsByRect,
                        std::set<MwmSet::MwmId> & activeMwms);

  // This is a group of methods that haven't their own synchronization inside.
  void RequestTrafficData();
  void RequestTrafficData(MwmSet::MwmId const & mwmId, bool force);

  void Clear();
  void ClearCache(MwmSet::MwmId const & mwmId);
  void ShrinkCacheToAllowableSize();

  void UpdateState();
  void ChangeState(TrafficState newState);

  bool IsInvalidState() const;
  bool IsEnabled() const;

  void UniteActiveMwms(std::set<MwmSet::MwmId> & activeMwms) const;

  void Pause();
  void Resume();

  template <class F>
  void ForEachActiveMwm(F && f) const
  {
    std::set<MwmSet::MwmId> activeMwms;
    UniteActiveMwms(activeMwms);
    std::for_each(activeMwms.begin(), activeMwms.end(), forward<F>(f));
  }

  GetMwmsByRectFn m_getMwmsByRectFn;
  traffic::TrafficObserver & m_observer;

  ref_ptr<df::DrapeEngine> m_drapeEngine;
  std::atomic<int64_t> m_currentDataVersion;

  // These fields have a flag of their initialization.
  pair<MyPosition, bool> m_currentPosition = {MyPosition(), false};
  pair<ScreenBase, bool> m_currentModelView = {ScreenBase(), false};

  std::atomic<TrafficState> m_state;
  TrafficStateChangedFn m_onStateChangedFn;

  size_t m_maxCacheSizeBytes;
  size_t m_currentCacheSizeBytes = 0;

  std::map<MwmSet::MwmId, CacheEntry> m_mwmCache;

  bool m_isRunning;
  condition_variable m_condition;

  std::vector<MwmSet::MwmId> m_lastDrapeMwmsByRect;
  std::set<MwmSet::MwmId> m_activeDrapeMwms;
  std::vector<MwmSet::MwmId> m_lastRoutingMwmsByRect;
  std::set<MwmSet::MwmId> m_activeRoutingMwms;

  // The ETag or entity tag is part of HTTP, the protocol for the World Wide Web.
  // It is one of several mechanisms that HTTP provides for web cache validation,
  // which allows a client to make conditional requests.
  std::map<MwmSet::MwmId, std::string> m_trafficETags;

  std::atomic<bool> m_isPaused;

  std::vector<MwmSet::MwmId> m_requestedMwms;
  std::mutex m_mutex;
  threads::SimpleThread m_thread;
};

extern std::string DebugPrint(TrafficManager::TrafficState state);
