#include <functional>
#include <iterator>
#include <set>

#include "base/assert.hpp"

namespace coding
{
template <typename T>
class ScanLine
{
public:
  struct Event
  {
    Event(T const & value, bool isOpenEvent, size_t id)
      : m_coord(value), m_open(isOpenEvent), m_id(id) {}

    bool operator<(Event const & event) const
    {
      if (m_coord < event.m_coord)
        return true;

      if (event.m_coord < m_coord)
        return false;

      if (!m_open && event.m_open)
        return true;

      if (m_open && !event.m_open)
        return false;

      return m_id < event.m_id;
    }

    T m_coord;
    bool m_open;
    size_t m_id;
  };

  struct Interval
  {
    T m_left;
    T m_right;
    T m_len;
    size_t m_id;

    bool operator<(Interval const & interval) const
    {
      if (m_len < interval.m_len)
        return true;

      if (m_len > interval.m_len)
        return false;

      return m_id < interval.m_id;
    }

    bool operator>(Interval const & interval) const
    {
      return interval < *this;
    }
  };

  void Add(T const & left, T const & right)
  {
    size_t id = m_events.size() / 2;
    m_events.emplace(left, true /* isOpenEvent */, id);
    m_events.emplace(right, false /* isOpenEvent */, id);
  }

  std::vector<std::pair<T, T>> GetLongestIntervals()
  {
    std::vector<std::pair<T, T>> result;

    while (!m_intervals.empty())
    {
      auto it = m_intervals.begin();

      auto const & left = it->m_left;
      auto const & right = it->m_right;
      auto id = it->m_id;

      if (!IsEventExists(left, right, id))
      {
        m_intervals.erase(it);
        continue;
      }

      result.empace_back(left, right);

      auto leftBound = m_events.find(Event(left, true, id));
      auto rightBound = m_events.find(Event(right, false, id));

      CHECK(leftBound != m_events.end(), ());
      CHECK(rightBound != m_events.end(), ());

      auto current = std::next(leftBound);
      for (; current != rightBound;)
        m_events.erase(current);

      m_events.erase(leftBound);
      m_events.erase(rightBound);
    }

    std::sort(result.begin(), result.end(), [&](auto const & a, auto const & b)
    {
      CHECK(a.second <= b.first || b.second <= a.first, ());

      return a.second <= b.first;
    });

    return result;
  }

private:
  std::set<Interval, std::greater<Interval>()> m_intervals;

  bool IsEventExists(T const & left, T const & right, size_t id) const
  {
    return m_events.count(Event(left, true /* isOpenEvent */, id)) +
           m_events.count(Event(right, false /* isOpenEvent */, id)) == 2;
  }

  std::set<Event> m_events;
};
}  // namespace coding
