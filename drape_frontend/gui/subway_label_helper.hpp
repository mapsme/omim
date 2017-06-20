#pragma once

#include "geometry/point2d.hpp"

#include <string>

class ScreenBase;

namespace gui
{
class SubwayLabelHelper
{
public:
  SubwayLabelHelper() = default;

  void UpdateTime(double timeInSeconds);
  void UpdatePosition(m2::PointD const & position);
  void Clear();
  bool IsVisible() const;
  bool IsTextDirty() const { return m_isTextDirty; }
  std::string const & GetText() const { return m_timeText; }
  m2::PointD const & GetPosition() const { return m_position; }

private:
  double m_timeInSeconds = 0.0;
  std::string m_timeText;
  m2::PointD m_position = m2::PointD::Zero();
  bool m_hasPosition = false;
  bool m_isTextDirty = false;
};
}  // namespace gui
