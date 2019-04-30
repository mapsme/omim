#include "coding/not_intersecting_intervals.hpp"

namespace coding
{

bool NotIntersectingIntervals::AddInterval(size_t left, size_t right)
{
  Interval interval(left, right);
  auto it = m_leftEnds.lower_bound(interval);
  if (it != m_leftEnds.end() && interval.IsIntersect(*it))
    return false;

  if (it != m_leftEnds.begin() && interval.IsIntersect(*std::prev(it)))
    return false;

  m_leftEnds.emplace(left, right);
  return true;
}

bool NotIntersectingIntervals::Interval::ByLeftEnd::operator()(Interval const & lhs,
                                                               Interval const & rhs) const
{
  return lhs.m_left < rhs.m_left;
};

bool NotIntersectingIntervals::Interval::IsIntersect(Interval const & rhs) const
{
  if (rhs.m_left <= m_left && m_left <= rhs.m_right)
    return true;

  if (rhs.m_left <= m_right && m_right <= rhs.m_right)
    return true;

  if (m_left <= rhs.m_left && rhs.m_left <= m_right)
    return true;

  if (m_left <= rhs.m_right && rhs.m_right <= m_right)
    return true;

  return false;
}
}  // namespace coding
