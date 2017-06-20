#include "map/subway_manager.hpp"
#include "map/routing_manager.hpp"

#include "drape_frontend/drape_engine.hpp"

#include <utility>

void SubwayManager::SetEnabled(bool isEnabled)
{
  if (isEnabled == m_isEnabled)
    return;

  m_isEnabled = isEnabled;
  {
    std::lock_guard<std::mutex> lock(m_drapeEngineMutex);
    if (m_drapeEngine != nullptr)
      m_drapeEngine->SetSubwayModeEnabled(isEnabled);
  }

  if (m_isEnabled)
  {
    if (m_routingManager != nullptr)
      m_routingManager->CloseRouting(true);
  }
  else
  {
    RemovePoints();
    ClearRoute();
  }
}

void SubwayManager::SetDrapeEngine(ref_ptr<df::DrapeEngine> engine)
{
  std::lock_guard<std::mutex> lock(m_drapeEngineMutex);
  m_drapeEngine = engine;
}

void SubwayManager::SetRoutingManager(ref_ptr<RoutingManager> mng)
{
  m_routingManager = mng;
}

void SubwayManager::SetStartPoint(m2::PointD const & pt)
{
  if (!m_isEnabled || m_routingManager == nullptr)
    return;

  RouteMarkData data;
  data.m_position = pt;
  data.m_pointType = RouteMarkType::Start;
  m_routingManager->AddRoutePoint(std::move(data));

  CheckAndBuild();
}

void SubwayManager::SetFinishPoint(m2::PointD const & pt)
{
  if (!m_isEnabled || m_routingManager == nullptr)
    return;

  RouteMarkData data;
  data.m_position = pt;
  data.m_pointType = RouteMarkType::Finish;
  m_routingManager->AddRoutePoint(std::move(data));

  CheckAndBuild();
}

void SubwayManager::RemoveStartPoint()
{
  if (!m_isEnabled || m_routingManager == nullptr)
    return;
  m_routingManager->RemoveRoutePoint(RouteMarkType::Start);
  ClearRoute();
}

void SubwayManager::RemoveFinishPoint()
{
  if (!m_isEnabled || m_routingManager == nullptr)
    return;
  m_routingManager->RemoveRoutePoint(RouteMarkType::Finish);
  ClearRoute();
}

void SubwayManager::RemovePoints()
{
  RemoveStartPoint();
  RemoveFinishPoint();
}

void SubwayManager::CheckAndBuild()
{
  ClearRoute();

  auto const points = m_routingManager->GetRoutePoints();
  if (points.size() < 2)
    return;
  m_routingManager->BuildSubwayRoute(points.front().m_position,
                                     points.back().m_position);
}

void SubwayManager::ClearRoute()
{
  m_routingManager->CloseRouting(false /* remove route points */);
}
