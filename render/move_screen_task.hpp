#pragma once

#include "anim/segment_interpolation.hpp"
#include "render/navigator.hpp"

namespace rg
{

class MoveScreenTask : public anim::SegmentInterpolation
{
private:

  Navigator & m_navigator;
  m2::PointD m_outPt;

public:

  MoveScreenTask(Navigator & navigator,
                 m2::PointD const & startPt,
                 m2::PointD const & endPt,
                 double interval);

  void OnStep(double ts);
  void OnEnd(double ts);

  bool IsVisual() const;
};

}
