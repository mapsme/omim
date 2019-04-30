#pragma once

#include <set>

namespace coding
{
class NotIntersectingIntervals
{
public:
  bool AddInterval(size_t left, size_t right);

private:
  struct Interval
  {
    Interval(size_t left, size_t right) : m_left(left), m_right(right) {}

    struct ByLeftEnd
    {
      bool operator()(Interval const & lhs, Interval const & rhs) const;
    };

    bool IsIntersect(Interval const & rhs) const;

    size_t m_left = 0;
    size_t m_right = 0;
  };

  std::set<Interval, Interval::ByLeftEnd> m_leftEnds;
};
}  // namespace coding
