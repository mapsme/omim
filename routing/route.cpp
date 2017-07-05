#include "routing/route.hpp"

#include "routing/turns_generator.hpp"

#include "traffic/speed_groups.hpp"

#include "indexer/feature_altitude.hpp"

#include "geometry/mercator.hpp"

#include "platform/location.hpp"

#include "geometry/angles.hpp"
#include "geometry/point2d.hpp"
#include "geometry/simplification.hpp"

#include "base/logging.hpp"

#include "std/numeric.hpp"

#include <algorithm>
#include <utility>

using namespace traffic;
using namespace routing::turns;

namespace routing
{
namespace
{
double constexpr kLocationTimeThreshold = 60.0 * 1.0;
double constexpr kOnEndToleranceM = 10.0;
double constexpr kSteetNameLinkMeters = 400.;
}  //  namespace

Route::Route(string const & router, vector<m2::PointD> const & points, string const & name)
  : m_router(router), m_routingSettings(GetCarRoutingSettings()),
    m_name(name), m_poly(points.begin(), points.end())
{
  Update();
}

void Route::Swap(Route & rhs)
{
  m_router.swap(rhs.m_router);
  swap(m_routingSettings, rhs.m_routingSettings);
  m_poly.Swap(rhs.m_poly);
  m_simplifiedPoly.Swap(rhs.m_simplifiedPoly);
  m_name.swap(rhs.m_name);
  swap(m_currentTime, rhs.m_currentTime);
  swap(m_turns, rhs.m_turns);
  swap(m_times, rhs.m_times);
  swap(m_streets, rhs.m_streets);
  m_absentCountries.swap(rhs.m_absentCountries);
  m_altitudes.swap(rhs.m_altitudes);
  m_traffic.swap(rhs.m_traffic);
  m_routeSegments.swap(rhs.m_routeSegments);
  m_subrouteAttrs.swap(rhs.m_subrouteAttrs);
}

void Route::AddAbsentCountry(string const & name)
{
  if (!name.empty()) m_absentCountries.insert(name);
}

double Route::GetTotalDistanceMeters() const
{
  if (!m_poly.IsValid())
    return 0.0;
  return m_poly.GetTotalDistanceM();
}

double Route::GetCurrentDistanceFromBeginMeters() const
{
  if (!m_poly.IsValid())
    return 0.0;
  return m_poly.GetDistanceFromBeginM();
}

double Route::GetCurrentDistanceToEndMeters() const
{
  if (!m_poly.IsValid())
    return 0.0;
  return m_poly.GetDistanceToEndM();
}

double Route::GetMercatorDistanceFromBegin() const
{
  //TODO Maybe better to return FollowedRoute and user will call GetMercatorDistance etc. by itself
  return m_poly.GetMercatorDistanceFromBegin();
}

uint32_t Route::GetTotalTimeSec() const
{
  return m_times.empty() ? 0 : m_times.back().second;
}

uint32_t Route::GetCurrentTimeToEndSec() const
{
  size_t const polySz = m_poly.GetPolyline().GetSize();
  if (m_times.empty() || polySz == 0)
  {
    ASSERT(!m_times.empty(), ());
    ASSERT(polySz != 0, ());
    return 0;
  }

  TTimes::const_iterator it = upper_bound(m_times.begin(), m_times.end(), m_poly.GetCurrentIter().m_ind,
                                         [](size_t v, Route::TTimeItem const & item) { return v < item.first; });

  if (it == m_times.end())
    return 0;

  size_t idx = distance(m_times.begin(), it);
  double time = (*it).second;
  if (idx > 0)
    time -= m_times[idx - 1].second;

  auto distFn = [&](size_t start, size_t end)
  {
    return m_poly.GetDistanceM(m_poly.GetIterToIndex(start), m_poly.GetIterToIndex(end));
  };

  ASSERT_LESS(m_times[idx].first, polySz, ());
  double const dist = distFn(idx > 0 ? m_times[idx - 1].first : 0, m_times[idx].first);

  if (!my::AlmostEqualULPs(dist, 0.))
  {
    double const distRemain = distFn(m_poly.GetCurrentIter().m_ind, m_times[idx].first) -
                                     MercatorBounds::DistanceOnEarth(m_poly.GetCurrentIter().m_pt,
                                     m_poly.GetPolyline().GetPoint(m_poly.GetCurrentIter().m_ind));
    return (uint32_t)((GetTotalTimeSec() - (*it).second) + (double)time * (distRemain / dist));
  }
  else
    return (uint32_t)((GetTotalTimeSec() - (*it).second));
}

Route::TTurns::const_iterator Route::GetCurrentTurn() const
{
  ASSERT(!m_turns.empty(), ());

  TurnItem t;
  t.m_index = static_cast<uint32_t>(m_poly.GetCurrentIter().m_ind);
  return upper_bound(m_turns.cbegin(), m_turns.cend(), t,
         [](TurnItem const & lhs, TurnItem const & rhs)
         {
           return lhs.m_index < rhs.m_index;
         });
}

void Route::GetCurrentStreetName(string & name) const
{
  auto it = GetCurrentStreetNameIterAfter(m_poly.GetCurrentIter());
  if (it == m_streets.cend())
    name.clear();
  else
    name = it->second;
}

void Route::GetStreetNameAfterIdx(uint32_t idx, string & name) const
{
  name.clear();
  auto polyIter = m_poly.GetIterToIndex(idx);
  auto it = GetCurrentStreetNameIterAfter(polyIter);
  if (it == m_streets.cend())
    return;
  for (;it != m_streets.cend(); ++it)
    if (!it->second.empty())
    {
      if (m_poly.GetDistanceM(polyIter, m_poly.GetIterToIndex(max(it->first, static_cast<uint32_t>(polyIter.m_ind)))) < kSteetNameLinkMeters)
        name = it->second;
      return;
    }
}

Route::TStreets::const_iterator Route::GetCurrentStreetNameIterAfter(FollowedPolyline::Iter iter) const
{
  // m_streets empty for pedestrian router.
  if (m_streets.empty())
  {
    return m_streets.cend();
  }

  TStreets::const_iterator curIter = m_streets.cbegin();
  TStreets::const_iterator prevIter = curIter;
  curIter++;

  while (curIter->first < iter.m_ind)
  {
    ++prevIter;
    ++curIter;
    if (curIter == m_streets.cend())
      return curIter;
  }
  return curIter->first == iter.m_ind ? curIter : prevIter;
}

bool Route::GetCurrentTurn(double & distanceToTurnMeters, TurnItem & turn) const
{
  auto it = GetCurrentTurn();
  if (it == m_turns.end())
  {
    ASSERT(false, ());
    return false;
  }

  size_t const segIdx = (*it).m_index;
  turn = (*it);
  distanceToTurnMeters = m_poly.GetDistanceM(m_poly.GetCurrentIter(),
                                             m_poly.GetIterToIndex(segIdx));
  return true;
}

bool Route::GetNextTurn(double & distanceToTurnMeters, TurnItem & turn) const
{
  auto it = GetCurrentTurn();
  auto const turnsEnd = m_turns.end();
  ASSERT(it != turnsEnd, ());

  if (it == turnsEnd || (it + 1) == turnsEnd)
  {
    turn = TurnItem();
    distanceToTurnMeters = 0;
    return false;
  }

  it += 1;
  turn = *it;
  distanceToTurnMeters = m_poly.GetDistanceM(m_poly.GetCurrentIter(),
                                             m_poly.GetIterToIndex(it->m_index));
  return true;
}

bool Route::GetNextTurns(vector<TurnItemDist> & turns) const
{
  TurnItemDist currentTurn;
  if (!GetCurrentTurn(currentTurn.m_distMeters, currentTurn.m_turnItem))
    return false;

  turns.clear();
  turns.emplace_back(move(currentTurn));

  TurnItemDist nextTurn;
  if (GetNextTurn(nextTurn.m_distMeters, nextTurn.m_turnItem))
    turns.emplace_back(move(nextTurn));
  return true;
}

void Route::GetCurrentDirectionPoint(m2::PointD & pt) const
{
  if (m_routingSettings.m_keepPedestrianInfo && m_simplifiedPoly.IsValid())
    m_simplifiedPoly.GetCurrentDirectionPoint(pt, kOnEndToleranceM);
  else
    m_poly.GetCurrentDirectionPoint(pt, kOnEndToleranceM);
}

bool Route::MoveIterator(location::GpsInfo const & info) const
{
  double predictDistance = -1.0;
  if (m_currentTime > 0.0 && info.HasSpeed())
  {
    /// @todo Need to distinguish GPS and WiFi locations.
    /// They may have different time metrics in case of incorrect system time on a device.
    double const deltaT = info.m_timestamp - m_currentTime;
    if (deltaT > 0.0 && deltaT < kLocationTimeThreshold)
      predictDistance = info.m_speed * deltaT;
  }

  m2::RectD const rect = MercatorBounds::MetresToXY(
        info.m_longitude, info.m_latitude,
        max(m_routingSettings.m_matchingThresholdM, info.m_horizontalAccuracy));
  FollowedPolyline::Iter const res = m_poly.UpdateProjectionByPrediction(rect, predictDistance);
  if (m_simplifiedPoly.IsValid())
    m_simplifiedPoly.UpdateProjectionByPrediction(rect, predictDistance);
  return res.IsValid();
}

double Route::GetPolySegAngle(size_t ind) const
{
  size_t const polySz = m_poly.GetPolyline().GetSize();

  if (ind + 1 >= polySz)
  {
    ASSERT(false, ());
    return 0;
  }

  m2::PointD const p1 = m_poly.GetPolyline().GetPoint(ind);
  m2::PointD p2;
  size_t i = ind + 1;
  do
  {
    p2 = m_poly.GetPolyline().GetPoint(i);
  }
  while (m2::AlmostEqualULPs(p1, p2) && ++i < polySz);
  return (i == polySz) ? 0 : my::RadToDeg(ang::AngleTo(p1, p2));
}

void Route::MatchLocationToRoute(location::GpsInfo & location, location::RouteMatchingInfo & routeMatchingInfo) const
{
  if (m_poly.IsValid())
  {
    auto const & iter = m_poly.GetCurrentIter();
    m2::PointD const locationMerc = MercatorBounds::FromLatLon(location.m_latitude, location.m_longitude);
    double const distFromRouteM = MercatorBounds::DistanceOnEarth(iter.m_pt, locationMerc);
    if (distFromRouteM < m_routingSettings.m_matchingThresholdM)
    {
      location.m_latitude = MercatorBounds::YToLat(iter.m_pt.y);
      location.m_longitude = MercatorBounds::XToLon(iter.m_pt.x);
      if (m_routingSettings.m_matchRoute)
        location.m_bearing = location::AngleToBearing(GetPolySegAngle(iter.m_ind));

      routeMatchingInfo.Set(iter.m_pt, iter.m_ind, GetMercatorDistanceFromBegin());
    }
  }
}

void Route::Update()
{
  if (!m_poly.IsValid())
    return;
  if (m_routingSettings.m_keepPedestrianInfo)
  {
    vector<m2::PointD> points;
    auto distFn = m2::DistanceToLineSquare<m2::PointD>();
    // TODO (ldargunov) Rewrite dist f to distance in meters and avoid 0.00000 constants.
    SimplifyNearOptimal(20, m_poly.GetPolyline().Begin(), m_poly.GetPolyline().End(), 0.00000001, distFn,
                        MakeBackInsertFunctor(points));
    FollowedPolyline(points.begin(), points.end()).Swap(m_simplifiedPoly);
  }
  else
  {
    // Free memory if we don't need simplified geometry.
    FollowedPolyline().Swap(m_simplifiedPoly);
  }
  m_currentTime = 0.0;
}

size_t Route::GetSubrouteCount() const { return m_subrouteAttrs.size(); }

void Route::GetSubrouteInfo(size_t subrouteIdx, std::vector<RouteSegment> & segments) const
{
  segments.clear();
  SubrouteAttrs const & attrs = GetSubrouteAttrs(subrouteIdx);

  CHECK_LESS_OR_EQUAL(attrs.GetEndSegmentIdx(), m_routeSegments.size(), ());

  for (size_t i = attrs.GetBeginSegmentIdx(); i < attrs.GetEndSegmentIdx(); ++i)
    segments.push_back(m_routeSegments[i]);
}

Route::SubrouteAttrs const & Route::GetSubrouteAttrs(size_t subrouteIdx) const
{
  CHECK(IsValid(), ());
  CHECK_LESS(subrouteIdx, m_subrouteAttrs.size(), ());
  return m_subrouteAttrs[subrouteIdx];
}

Route::SubrouteSettings const Route::GetSubrouteSettings(size_t segmentIdx) const
{
  CHECK_LESS(segmentIdx, GetSubrouteCount(), ());
  return SubrouteSettings(m_routingSettings, m_router, m_subrouteUid);
}

bool Route::IsSubroutePassed(size_t subrouteIdx) const
{
  size_t const segmentIdx = GetSubrouteAttrs(subrouteIdx).GetEndSegmentIdx() - 1;
  CHECK_LESS(segmentIdx, m_routeSegments.size(), ());
  double const lengthMeters = m_routeSegments[segmentIdx].GetDistFromBeginningMeters();
  double const passedDistanceMeters = m_poly.GetDistanceFromBeginM();
  return lengthMeters - passedDistanceMeters < kOnEndToleranceM;
}

void Route::SetSubrouteUid(size_t segmentIdx, SubrouteUid subrouteUid)
{
  CHECK_LESS(segmentIdx, GetSubrouteCount(), ());
  m_subrouteUid = subrouteUid;
}

Junction Route::GetJunction(size_t pointIdx) const
{
  CHECK(IsValid(), ());
  CHECK_LESS(pointIdx,  m_poly.GetPolyline().GetSize(), ());
  if (!m_altitudes.empty())
    CHECK_EQUAL(m_altitudes.size(), m_poly.GetPolyline().GetSize(), ());

  auto const & points = m_poly.GetPolyline().GetPoints();
  return Junction(points[pointIdx],
                  m_altitudes.empty() ? feature::kInvalidAltitude : m_altitudes[pointIdx]);
}

string DebugPrint(Route const & r)
{
  return DebugPrint(r.m_poly.GetPolyline());
}
} // namespace routing
