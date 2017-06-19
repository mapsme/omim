#include "map/subway_manager.hpp"

#include "drape_frontend/drape_engine.hpp"

void SubwayManager::SetDrapeEngine(ref_ptr<df::DrapeEngine> engine)
{
  m_drapeEngine = engine;
}

void SubwayManager::SetStartPoint(m2::PointD const & pt)
{
  m_startPoint = pt;
}

void SubwayManager::SetFinishPoint(m2::PointD const & pt)
{
  m_finishPoint = pt;
}

void SubwayManager::ClearRoute()
{
  //TODO: clear built route
}
