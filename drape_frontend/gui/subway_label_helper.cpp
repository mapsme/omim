#include "drape_frontend/gui/subway_label_helper.hpp"

#include "base/string_utils.hpp"

namespace gui
{
namespace
{
std::string TimeToString(double timeInSeconds)
{
  uint32_t const seconds = static_cast<uint32_t>(timeInSeconds);
  uint32_t const minutes = seconds / 60;
  return strings::to_string(minutes) + " min";
}
}  // namespace

void SubwayLabelHelper::UpdateTime(double timeInSeconds)
{
  std::string const s = TimeToString(timeInSeconds);
  if (m_timeText != s)
  {
    m_timeText = s;
    m_isTextDirty = true;
  }
}

void SubwayLabelHelper::UpdatePosition(m2::PointD const & position)
{
  m_hasPosition = true;
  m_position = position;
}

void SubwayLabelHelper::Clear()
{
  m_timeText.clear();
  m_timeInSeconds = 0.0;
  m_position = m2::PointD::Zero();
  m_hasPosition = false;
}

bool SubwayLabelHelper::IsVisible() const
{
  return !m_timeText.empty() && m_hasPosition;
}
}  // namespace gui
