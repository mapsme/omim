#pragma once

#include "drape/pointers.hpp"

#include "geometry/point2d.hpp"

namespace df
{
class DrapeEngine;
}

class RoutingManager;

class SubwayManager
{
public:
  void SetEnabled(bool isEnabled);
  bool IsEnabled() const { return m_isEnabled; }

  void SetStartPoint(m2::PointD const & pt);
  void SetFinishPoint(m2::PointD const & pt);
  void RemoveStartPoint();
  void RemoveFinishPoint();
  void ClearRoute();

  void SetDrapeEngine(ref_ptr<df::DrapeEngine> engine);
  void SetRoutingManager(ref_ptr<RoutingManager> mng);

private:
  void RemovePoints();
  void CheckAndBuild();

  bool m_isEnabled = true; // TODO: set false by default
  ref_ptr<df::DrapeEngine> m_drapeEngine;
  ref_ptr<RoutingManager> m_routingManager;
};
