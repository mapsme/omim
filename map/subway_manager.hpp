#pragma once

#include "drape/pointers.hpp"

#include "geometry/point2d.hpp"

namespace df
{
class DrapeEngine;
}

class SubwayManager
{
public:
  void SetEnabled(bool isEnabled) { m_isEnabled = isEnabled; }
  bool IsEnabled() const { return m_isEnabled; }

  void SetStartPoint(m2::PointD const & pt);
  void SetFinishPoint(m2::PointD const & pt);
  void ClearRoute();

  void SetDrapeEngine(ref_ptr<df::DrapeEngine> engine);

private:
  bool m_isEnabled = true; // TODO: set false by default
  ref_ptr<df::DrapeEngine> m_drapeEngine;
  m2::PointD m_startPoint = m2::PointD::Zero();
  m2::PointD m_finishPoint = m2::PointD::Zero();
};
