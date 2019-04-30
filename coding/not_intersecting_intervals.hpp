#pragma once

#include <set>

namespace coding
{
class NotIntersectingIntervals
{
public:
  bool AddInterval(size_t left, size_t right)
  {
    Interval interval(left, right);
    auto it = m_leftEnds.lower_bound(interval);
    if (it != m_leftEnds.end() && interval.IsIntersect(*it))
      return false;

    it = m_rightEnds.lower_bound(interval);
    if (it != m_rightEnds.end() && interval.IsIntersect(*it))
      return false;

    m_leftEnds.emplace(left, right);
    m_rightEnds.emplace(left, right);
    return true;
  }

private:
  struct Interval
  {
    Interval(size_t left, size_t right) : m_left(left), m_right(right) {}

    struct ByLeftEnd
    {
      bool operator()(Interval const & lhs, Interval const & rhs) const
      {
        return lhs.m_left < rhs.m_left;
      }
    };

    struct ByRightEnd
    {
      bool operator()(Interval const & lhs, Interval const & rhs) const
      {
        return lhs.m_right < rhs.m_right;
      }
    };

    bool IsIntersect(Interval const & rhs) const
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

    size_t m_left = 0;
    size_t m_right = 0;
  };

  std::set<Interval, Interval::ByLeftEnd> m_leftEnds;
  std::set<Interval, Interval::ByRightEnd> m_rightEnds;
};
}  // namespace coding
