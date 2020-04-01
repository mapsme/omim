#include "routing/routing_session.hpp"

#include "routing/routing_helpers.hpp"
#include "routing/speed_camera.hpp"

#include "geometry/mercator.hpp"

#include "platform/location.hpp"
#include "platform/measurement_utils.hpp"
#include "platform/platform.hpp"

#include "coding/internal/file_data.hpp"

#include <utility>

#include "3party/Alohalytics/src/alohalytics.h"

using namespace location;
using namespace std;
using namespace traffic;

namespace
{

int constexpr kOnRouteMissedCount = 10;

// @TODO(vbykoianko) The distance should depend on the current speed.
double constexpr kShowLanesDistInMeters = 500.;

// @todo(kshalnev) The distance may depend on the current speed.
double constexpr kShowPedestrianTurnInMeters = 5.;

double constexpr kRunawayDistanceSensitivityMeters = 0.01;

double constexpr kCompletionPercentAccuracy = 5;

double constexpr kMinimumETASec = 60.0;
}  // namespace

namespace routing
{
void FormatDistance(double dist, string & value, string & suffix)
{
  /// @todo Make better formatting of distance and units.
  UNUSED_VALUE(measurement_utils::FormatDistance(dist, value));

  size_t const delim = value.find(' ');
  ASSERT(delim != string::npos, ());
  suffix = value.substr(delim + 1);
  value.erase(delim);
};

RoutingSession::RoutingSession()
  : m_router(nullptr)
  , m_route(make_shared<Route>(string() /* router */, 0 /* route id */))
  , m_state(SessionState::RoutingNotActive)
  , m_isFollowing(false)
  , m_speedCameraManager(m_turnNotificationsMgr)
  , m_routingSettings(GetRoutingSettings(VehicleType::Car))
  , m_passedDistanceOnRouteMeters(0.0)
  , m_lastCompletionPercent(0.0)
{
  // To call |m_changeSessionStateCallback| on |m_state| initialization.
  SetState(SessionState::RoutingNotActive);
  m_speedCameraManager.SetRoute(m_route);
}

void RoutingSession::Init(RoutingStatisticsCallback const & routingStatisticsFn,
                          PointCheckCallback const & pointCheckCallback)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  CHECK(!m_router, ());
  m_router = make_unique<AsyncRouter>(routingStatisticsFn, pointCheckCallback);

  alohalytics::TStringMap params = {
    {"speed_cameras", SpeedCameraManagerModeForStat(m_speedCameraManager.GetMode())},
    {"voice_enabled", m_turnNotificationsMgr.IsEnabled() ? "1" : "0"}
  };
  alohalytics::LogEvent("OnRoutingInit", params);
}

void RoutingSession::BuildRoute(Checkpoints const & checkpoints,
                                uint32_t timeoutSec)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  CHECK(m_router, ());
  m_checkpoints = checkpoints;
  m_router->ClearState();
  m_isFollowing = false;
  m_routingRebuildCount = -1; // -1 for the first rebuild.

  RebuildRoute(checkpoints.GetStart(), m_buildReadyCallback, m_needMoreMapsCallback,
               m_removeRouteCallback, timeoutSec, SessionState::RouteBuilding, false /* adjust */);
}

void RoutingSession::RebuildRoute(m2::PointD const & startPoint,
                                  ReadyCallback const & readyCallback,
                                  NeedMoreMapsCallback const & needMoreMapsCallback,
                                  RemoveRouteCallback const & removeRouteCallback,
                                  uint32_t timeoutSec, SessionState routeRebuildingState,
                                  bool adjustToPrevRoute)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  CHECK(m_router, ());
  RemoveRoute();
  SetState(routeRebuildingState);

  ++m_routingRebuildCount;
  m_lastCompletionPercent = 0;
  m_checkpoints.SetPointFrom(startPoint);

  auto const & direction = 
      m_routingSettings.m_useDirectionForRouteBuilding ? m_positionAccumulator.GetDirection()
                                                       : m2::PointD::Zero();

  // Use old-style callback construction, because lambda constructs buggy function on Android
  // (callback param isn't captured by value).
  m_router->CalculateRoute(m_checkpoints, direction, adjustToPrevRoute,
                           DoReadyCallback(*this, readyCallback),
                           needMoreMapsCallback, removeRouteCallback, m_progressCallback, timeoutSec);
}

m2::PointD RoutingSession::GetStartPoint() const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  return m_checkpoints.GetStart();
}

m2::PointD RoutingSession::GetEndPoint() const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  return m_checkpoints.GetFinish();
}

void RoutingSession::DoReadyCallback::operator()(shared_ptr<Route> route, RouterResultCode e)
{
  ASSERT(m_rs.m_route, ());
  m_rs.AssignRoute(route, e);
  m_callback(*m_rs.m_route, e);
}

void RoutingSession::RemoveRoute()
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  SetState(SessionState::RoutingNotActive);
  m_lastDistance = 0.0;
  m_moveAwayCounter = 0;
  m_turnNotificationsMgr.Reset();

  m_route = make_shared<Route>(string() /* router */, 0 /* route id */);
  m_speedCameraManager.Reset();
  m_speedCameraManager.SetRoute(m_route);
}

void RoutingSession::RebuildRouteOnTrafficUpdate()
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  m2::PointD startPoint;

  {
    startPoint = m_lastGoodPosition;

    switch (m_state)
    {
    case SessionState::RoutingNotActive:
    case SessionState::RouteNotReady:
    case SessionState::RouteFinished: return;

    case SessionState::RouteBuilding:
    case SessionState::RouteNotStarted:
    case SessionState::RouteNoFollowing:
    case SessionState::RouteRebuilding:
      startPoint = m_checkpoints.GetPointFrom();
      break;

    case SessionState::OnRoute:
    case SessionState::RouteNeedRebuild: break;
    }

    // Cancel current route building.
    m_router->ClearState();
  }

  RebuildRoute(startPoint, m_rebuildReadyCallback, nullptr /* needMoreMapsCallback */,
               nullptr /* removeRouteCallback */, RouterDelegate::kNoTimeout,
               SessionState::RouteRebuilding, false /* adjustToPrevRoute */);
}

bool RoutingSession::IsActive() const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  return (m_state != SessionState::RoutingNotActive);
}

bool RoutingSession::IsNavigable() const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  return (m_state == SessionState::RouteNotStarted || m_state == SessionState::OnRoute ||
          m_state == SessionState::RouteFinished);
}

bool RoutingSession::IsBuilt() const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  return (IsNavigable() || m_state == SessionState::RouteNeedRebuild);
}

bool RoutingSession::IsBuilding() const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  return (m_state == SessionState::RouteBuilding || m_state == SessionState::RouteRebuilding);
}

bool RoutingSession::IsBuildingOnly() const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  return m_state == SessionState::RouteBuilding;
}

bool RoutingSession::IsRebuildingOnly() const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  return m_state == SessionState::RouteRebuilding;
}

bool RoutingSession::IsNotReady() const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  return m_state == SessionState::RouteNotReady;
}

bool RoutingSession::IsFinished() const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  return m_state == SessionState::RouteFinished;
}

bool RoutingSession::IsNoFollowing() const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  return m_state == SessionState::RouteNoFollowing;
}

bool RoutingSession::IsOnRoute() const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  return (m_state == SessionState::OnRoute);
}

bool RoutingSession::IsFollowing() const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  return m_isFollowing;
}

void RoutingSession::Reset()
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  ASSERT(m_router != nullptr, ());

  RemoveRoute();
  m_router->ClearState();

  m_passedDistanceOnRouteMeters = 0.0;
  m_isFollowing = false;
  m_lastCompletionPercent = 0;
}

void RoutingSession::SetState(SessionState state)
{
  if (m_changeSessionStateCallback && m_state != state)
    m_changeSessionStateCallback(m_state, state);

  m_state = state;
}

SessionState RoutingSession::OnLocationPositionChanged(GpsInfo const & info)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  ASSERT_NOT_EQUAL(m_state, SessionState::RoutingNotActive, ());
  ASSERT(m_router, ());

  if (m_state == SessionState::RouteNeedRebuild || m_state == SessionState::RouteFinished
      || m_state == SessionState::RouteBuilding || m_state == SessionState::RouteRebuilding
      || m_state == SessionState::RouteNotReady || m_state == SessionState::RouteNoFollowing)
    return m_state;

  ASSERT(m_route, ());
  ASSERT(m_route->IsValid(), ());

  m_turnNotificationsMgr.SetSpeedMetersPerSecond(info.m_speedMpS);

  auto const iteratorAction = m_route->MoveIteratorToReal(info);

  if (iteratorAction.m_movedIterator)
  {
    m_moveAwayCounter = 0;
    m_lastDistance = 0.0;

    PassCheckpoints();

    if (m_checkpoints.IsFinished())
    {
      m_passedDistanceOnRouteMeters += m_route->GetTotalDistanceMeters();
      SetState(SessionState::RouteFinished);

      alohalytics::TStringMap params = {{"router", m_route->GetRouterId()},
                                        {"passedDistance", strings::to_string(m_passedDistanceOnRouteMeters)},
                                        {"rebuildCount", strings::to_string(m_routingRebuildCount)}};
      alohalytics::LogEvent("RouteTracking_ReachedDestination", params);
    }
    else
    {
      SetState(SessionState::OnRoute);

      m_speedCameraManager.OnLocationPositionChanged(info);
    }

    if (m_userCurrentPositionValid)
      m_lastGoodPosition = m_userCurrentPosition;

    return m_state;
  }

  if (!iteratorAction.m_closerToFake)
  {
    // Distance from the last known projection on route
    // (check if we are moving far from the last known projection).
    auto const & lastGoodPoint = m_route->GetFollowedPolyline().GetCurrentIter().m_pt;
    double const dist = mercator::DistanceOnEarth(lastGoodPoint,
                                                  mercator::FromLatLon(info.m_latitude, info.m_longitude));
    if (base::AlmostEqualAbs(dist, m_lastDistance, kRunawayDistanceSensitivityMeters))
      return m_state;

    if (!info.HasSpeed() || info.m_speedMpS < m_routingSettings.m_minSpeedForRouteRebuildMpS)
      m_moveAwayCounter += 1;
    else
      m_moveAwayCounter += 2;

    m_lastDistance = dist;

    if (m_moveAwayCounter > kOnRouteMissedCount)
    {
      m_passedDistanceOnRouteMeters += m_route->GetCurrentDistanceFromBeginMeters();
      SetState(SessionState::RouteNeedRebuild);
      alohalytics::TStringMap params = {
          {"router", m_route->GetRouterId()},
          {"percent", strings::to_string(GetCompletionPercent())},
          {"passedDistance", strings::to_string(m_passedDistanceOnRouteMeters)},
          {"rebuildCount", strings::to_string(m_routingRebuildCount)}};
      alohalytics::LogEvent(
          "RouteTracking_RouteNeedRebuild", params,
          alohalytics::Location::FromLatLon(mercator::YToLat(lastGoodPoint.y),
                                            mercator::XToLon(lastGoodPoint.x)));
    }
  }

  return m_state;
}

void RoutingSession::GetRouteFollowingInfo(FollowingInfo & info) const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());

  ASSERT(m_route, ());

  if (!m_route->IsValid())
  {
    // nothing should be displayed on the screen about turns if these lines are executed
    info = FollowingInfo();
    return;
  }

  if (!IsNavigable())
  {
    info = FollowingInfo();
    FormatDistance(m_route->GetTotalDistanceMeters(), info.m_distToTarget, info.m_targetUnitsSuffix);
    info.m_time = static_cast<int>(max(kMinimumETASec, m_route->GetCurrentTimeToEndSec()));
    return;
  }

  FormatDistance(m_route->GetCurrentDistanceToEndMeters(), info.m_distToTarget, info.m_targetUnitsSuffix);

  double distanceToTurnMeters = 0.;
  turns::TurnItem turn;
  m_route->GetCurrentTurn(distanceToTurnMeters, turn);
  FormatDistance(distanceToTurnMeters, info.m_distToTurn, info.m_turnUnitsSuffix);
  info.m_turn = turn.m_turn;

  // The turn after the next one.
  if (m_routingSettings.m_showTurnAfterNext)
    info.m_nextTurn = m_turnNotificationsMgr.GetSecondTurnNotification();
  else
    info.m_nextTurn = routing::turns::CarDirection::None;

  info.m_exitNum = turn.m_exitNum;
  info.m_time = static_cast<int>(max(kMinimumETASec, m_route->GetCurrentTimeToEndSec()));
  m_route->GetCurrentStreetName(info.m_sourceName);
  m_route->GetStreetNameAfterIdx(turn.m_index, info.m_targetName);
  info.m_completionPercent = GetCompletionPercent();

  // Lane information and next street name.
  if (distanceToTurnMeters < kShowLanesDistInMeters)
  {
    info.m_displayedStreetName = info.m_targetName;
    // There are two nested loops below. Outer one is for lanes and inner one (ctor of
    // SingleLaneInfo) is
    // for each lane's directions. The size of turn.m_lanes is relatively small. Less than 10 in
    // most cases.
    info.m_lanes.clear();
    info.m_lanes.reserve(turn.m_lanes.size());
    for (size_t j = 0; j < turn.m_lanes.size(); ++j)
      info.m_lanes.emplace_back(turn.m_lanes[j]);
  }
  else
  {
    info.m_displayedStreetName = "";
    info.m_lanes.clear();
  }

  // Pedestrian info
  m2::PointD pos;
  m_route->GetCurrentDirectionPoint(pos);
  info.m_pedestrianDirectionPos = mercator::ToLatLon(pos);
  info.m_pedestrianTurn =
      (distanceToTurnMeters < kShowPedestrianTurnInMeters) ? turn.m_pedestrianTurn : turns::PedestrianDirection::None;
}

double RoutingSession::GetCompletionPercent() const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  ASSERT(m_route, ());

  double const denominator = m_passedDistanceOnRouteMeters + m_route->GetTotalDistanceMeters();
  if (!m_route->IsValid() || denominator == 0.0)
    return 0;

  double const percent = 100.0 *
    (m_passedDistanceOnRouteMeters + m_route->GetCurrentDistanceFromBeginMeters()) /
    denominator;
  if (percent - m_lastCompletionPercent > kCompletionPercentAccuracy)
  {
    auto const lastGoodPoint =
        mercator::ToLatLon(m_route->GetFollowedPolyline().GetCurrentIter().m_pt);
    alohalytics::Stats::Instance().LogEvent(
        "RouteTracking_PercentUpdate", {{"percent", strings::to_string(percent)}},
        alohalytics::Location::FromLatLon(lastGoodPoint.m_lat, lastGoodPoint.m_lon));
    m_lastCompletionPercent = percent;
  }
  return percent;
}

void RoutingSession::PassCheckpoints()
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  while (!m_checkpoints.IsFinished() && m_route->IsSubroutePassed(m_checkpoints.GetPassedIdx()))
  {
    m_route->PassNextSubroute();
    m_checkpoints.PassNextPoint();
    LOG(LINFO, ("Pass checkpoint, ", m_checkpoints));
    m_checkpointCallback(m_checkpoints.GetPassedIdx());
  }
}

void RoutingSession::GenerateNotifications(vector<string> & notifications)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  notifications.clear();

  ASSERT(m_route, ());

  // Voice turn notifications.
  if (!m_routingSettings.m_soundDirection)
    return;

  if (!m_route->IsValid() || !IsNavigable())
    return;

  // Generate turns notifications.
  vector<turns::TurnItemDist> turns;
  if (m_route->GetNextTurns(turns))
    m_turnNotificationsMgr.GenerateTurnNotifications(turns, notifications);

  m_speedCameraManager.GenerateNotifications(notifications);
}

void RoutingSession::AssignRoute(shared_ptr<Route> route, RouterResultCode e)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  if (e != RouterResultCode::Cancelled)
  {
    if (route->IsValid())
      SetState(SessionState::RouteNotStarted);
    else
      SetState(SessionState::RoutingNotActive);

    if (e != RouterResultCode::NoError)
      SetState(SessionState::RouteNotReady);
  }
  else
  {
    SetState(SessionState::RoutingNotActive);
  }

  ASSERT(m_route, ());

  route->SetRoutingSettings(m_routingSettings);
  m_route = route;
  m_speedCameraManager.Reset();
  m_speedCameraManager.SetRoute(m_route);
}

void RoutingSession::SetRouter(unique_ptr<IRouter> && router,
                               unique_ptr<OnlineAbsentCountriesFetcher> && fetcher)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  ASSERT(m_router != nullptr, ());
  Reset();
  m_router->SetRouter(move(router), move(fetcher));
}

void RoutingSession::MatchLocationToRoute(location::GpsInfo & location,
                                          location::RouteMatchingInfo & routeMatchingInfo) const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  if (!IsOnRoute())
    return;

  ASSERT(m_route, ());

  m_route->MatchLocationToRoute(location, routeMatchingInfo);
}

traffic::SpeedGroup RoutingSession::MatchTraffic(
    location::RouteMatchingInfo const & routeMatchingInfo) const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  if (!routeMatchingInfo.IsMatched())
    return SpeedGroup::Unknown;

  size_t const index = routeMatchingInfo.GetIndexInRoute();

  return m_route->GetTraffic(index);
}

bool RoutingSession::DisableFollowMode()
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  LOG(LINFO, ("Routing disables a following mode. SessionState: ", m_state));
  if (m_state == SessionState::RouteNotStarted || m_state == SessionState::OnRoute)
  {
    SetState(SessionState::RouteNoFollowing);
    m_isFollowing = false;
    return true;
  }
  return false;
}

bool RoutingSession::EnableFollowMode()
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  LOG(LINFO, ("Routing enables a following mode. SessionState: ", m_state));
  if (m_state == SessionState::RouteNotStarted || m_state == SessionState::OnRoute)
  {
    SetState(SessionState::OnRoute);
    m_isFollowing = true;
  }
  return m_isFollowing;
}

void RoutingSession::SetRoutingSettings(RoutingSettings const & routingSettings)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  m_routingSettings = routingSettings;
}

void RoutingSession::SetRoutingCallbacks(ReadyCallback const & buildReadyCallback,
                                         ReadyCallback const & rebuildReadyCallback,
                                         NeedMoreMapsCallback const & needMoreMapsCallback,
                                         RemoveRouteCallback const & removeRouteCallback)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  m_buildReadyCallback = buildReadyCallback;
  m_rebuildReadyCallback = rebuildReadyCallback;
  m_needMoreMapsCallback = needMoreMapsCallback;
  m_removeRouteCallback = removeRouteCallback;
}

void RoutingSession::SetSpeedCamShowCallback(SpeedCameraShowCallback && callback)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  m_speedCameraManager.SetSpeedCamShowCallback(std::move(callback));
}

void RoutingSession::SetSpeedCamClearCallback(SpeedCameraClearCallback && callback)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  m_speedCameraManager.SetSpeedCamClearCallback(std::move(callback));
}

void RoutingSession::SetProgressCallback(ProgressCallback const & progressCallback)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  m_progressCallback = progressCallback;
}

void RoutingSession::SetCheckpointCallback(CheckpointCallback const & checkpointCallback)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  m_checkpointCallback = checkpointCallback;
}

void RoutingSession::SetChangeSessionStateCallback(
    ChangeSessionStateCallback const & changeSessionStateCallback)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  m_changeSessionStateCallback = changeSessionStateCallback;
}

void RoutingSession::SetUserCurrentPosition(m2::PointD const & position)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());

  m_userCurrentPosition = position;
  m_userCurrentPositionValid = true;
}

void RoutingSession::PushPositionAccumulator(m2::PointD const & position)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());

  m_positionAccumulator.PushNextPoint(position);
}
void RoutingSession::ClearPositionAccumulator()
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());

  m_positionAccumulator.Clear();
}

void RoutingSession::EnableTurnNotifications(bool enable)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  m_turnNotificationsMgr.Enable(enable);
}

bool RoutingSession::AreTurnNotificationsEnabled() const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  return m_turnNotificationsMgr.IsEnabled();
}

void RoutingSession::SetTurnNotificationsUnits(measurement_utils::Units const units)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  m_turnNotificationsMgr.SetLengthUnits(units);
}

void RoutingSession::SetTurnNotificationsLocale(string const & locale)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  LOG(LINFO, ("The language for turn notifications is", locale));
  m_turnNotificationsMgr.SetLocale(locale);
}

string RoutingSession::GetTurnNotificationsLocale() const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  return m_turnNotificationsMgr.GetLocale();
}

void RoutingSession::RouteCall(RouteCallback const & callback) const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  CHECK(m_route, ());
  callback(*m_route);
}

void RoutingSession::EmitCloseRoutingEvent() const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  ASSERT(m_route, ());

  if (!m_route->IsValid())
  {
    ASSERT(false, ());
    return;
  }
  auto const lastGoodPoint =
      mercator::ToLatLon(m_route->GetFollowedPolyline().GetCurrentIter().m_pt);
  alohalytics::Stats::Instance().LogEvent(
      "RouteTracking_RouteClosing",
      {{"percent", strings::to_string(GetCompletionPercent())},
       {"distance", strings::to_string(m_passedDistanceOnRouteMeters +
                                       m_route->GetCurrentDistanceToEndMeters())},
       {"router", m_route->GetRouterId()},
       {"rebuildCount", strings::to_string(m_routingRebuildCount)}},
      alohalytics::Location::FromLatLon(lastGoodPoint.m_lat, lastGoodPoint.m_lon));
}

bool RoutingSession::HasRouteAltitude() const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  ASSERT(m_route, ());
  return m_route->HaveAltitudes();
}

bool RoutingSession::IsRouteId(uint64_t routeId) const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  return m_route->IsRouteId(routeId);
}

bool RoutingSession::IsRouteValid() const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  return m_route && m_route->IsValid();
}

bool RoutingSession::GetRouteAltitudesAndDistancesM(vector<double> & routeSegDistanceM,
                                                    geometry::Altitudes & routeAltitudesM) const
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  ASSERT(m_route, ());

  if (!m_route->IsValid() || !m_route->HaveAltitudes())
    return false;

  routeSegDistanceM = m_route->GetSegDistanceMeters();
  geometry::Altitudes altitudes;
  m_route->GetAltitudes(routeAltitudesM);
  return true;
}

void RoutingSession::OnTrafficInfoClear()
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  Clear();
  RebuildRouteOnTrafficUpdate();
}

void RoutingSession::OnTrafficInfoAdded(TrafficInfo && info)
{
  TrafficInfo::Coloring const & fullColoring = info.GetColoring();
  auto coloring = make_shared<TrafficInfo::Coloring>();
  for (auto const & kv : fullColoring)
  {
    ASSERT_NOT_EQUAL(kv.second, SpeedGroup::Unknown, ());
    coloring->insert(kv);
  }

  // Note. |coloring| should not be used after this call on gui thread.
  auto const mwmId = info.GetMwmId();
  GetPlatform().RunTask(Platform::Thread::Gui, [this, mwmId, coloring]() {
    Set(mwmId, coloring);
    RebuildRouteOnTrafficUpdate();
  });
}

void RoutingSession::OnTrafficInfoRemoved(MwmSet::MwmId const & mwmId)
{
  GetPlatform().RunTask(Platform::Thread::Gui, [this, mwmId]() {
    CHECK_THREAD_CHECKER(m_threadChecker, ());
    Remove(mwmId);
    RebuildRouteOnTrafficUpdate();
  });
}

void RoutingSession::CopyTraffic(traffic::AllMwmTrafficInfo & trafficColoring) const
{
  TrafficCache::CopyTraffic(trafficColoring);
}

void RoutingSession::SetLocaleWithJsonForTesting(std::string const & json, std::string const & locale)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  m_turnNotificationsMgr.SetLocaleWithJsonForTesting(json, locale);
}

string DebugPrint(SessionState state)
{
  switch (state)
  {
  case SessionState::RoutingNotActive: return "RoutingNotActive";
  case SessionState::RouteBuilding: return "RouteBuilding";
  case SessionState::RouteNotReady: return "RouteNotReady";
  case SessionState::RouteNotStarted: return "RouteNotStarted";
  case SessionState::OnRoute: return "OnRoute";
  case SessionState::RouteNeedRebuild: return "RouteNeedRebuild";
  case SessionState::RouteFinished: return "RouteFinished";
  case SessionState::RouteNoFollowing: return "RouteNoFollowing";
  case SessionState::RouteRebuilding: return "RouteRebuilding";
  }
  UNREACHABLE();
}
}  // namespace routing
