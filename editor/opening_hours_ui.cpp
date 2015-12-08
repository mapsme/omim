#include "opening_hours_ui.hpp"

#include "std/numeric.hpp"
#include "std/stack.hpp"
#include "std/tuple.hpp"
#include "std/type_traits.hpp"

#include "base/assert.hpp"

namespace
{
using namespace editor::ui;

size_t SpanLength(osmoh::Timespan const & span)
{
  using osmoh::operator""_h;
  auto const start = span.GetStart().GetHourMinutes().GetDurationCount();
  auto const end = span.GetEnd().GetHourMinutes().GetDurationCount();
  return end - start + (span.HasExtendedHours() ? osmoh::HourMinutes::TMinutes(24_h).count() : 0);
}

bool DoesIncludeAll(osmoh::Timespan const & openingTime, osmoh::TTimespans const & spans)
{
  if (spans.empty())
    return true;

  auto const openingTimeStart = openingTime.GetStart().GetHourMinutes().GetDuration();
  auto const openingTimeEnd = openingTime.GetEnd().GetHourMinutes().GetDuration();
  auto const excludeTimeStart = spans.front().GetStart().GetHourMinutes().GetDuration();
  auto const excludeTimeEnd = spans.back().GetEnd().GetHourMinutes().GetDuration();

  if (excludeTimeStart < openingTimeStart || openingTimeEnd < excludeTimeEnd)
    return false;

  return true;
}

bool FixTimeSpans(osmoh::Timespan openingTime, osmoh::TTimespans & spans)
{
  using osmoh::operator""_h;

  if (spans.empty())
    return true;

  for (auto & span : spans)
  {
    if (span.HasExtendedHours())
      span.GetEnd().GetHourMinutes().AddDuration(24_h);
  }

  sort(begin(spans), end(spans),
       [](osmoh::Timespan const & s1, osmoh::Timespan const s2)
         {
           auto const start1 = s1.GetStart().GetHourMinutes().GetDuration();
           auto const start2 = s2.GetStart().GetHourMinutes().GetDuration();

           // If two spans start at the same point the longest span should be leftmost.
           if (start1 == start2)
             return SpanLength(s1) > SpanLength(s2);

           return start1 < start2;
         });

  osmoh::TTimespans result{spans.front()};
  for (auto i = 1, j = 0; i < spans.size(); ++i)
  {
    auto const start2 = spans[i].GetStart().GetHourMinutes().GetDuration();
    auto const end1 = spans[j].GetEnd().GetHourMinutes().GetDuration();
    auto const end2 = spans[i].GetEnd().GetHourMinutes().GetDuration();

    // The first one includes the second.
    if (start2 < end1 && end2 <= end1)
    {
      continue;
    }
    // Two spans have non-empty intersection.
    else if (start2 <= end1)
    {
      result.back().SetEnd(spans[i].GetEnd());
    }
    // The scond span starts after the end of the first one.
    else
    {
      result.push_back(spans[i]);
      ++j;
    }
  }

  // Check that all exclude time spans are included in opening time.
  if (openingTime.HasExtendedHours())
    openingTime.GetEnd().GetHourMinutes().AddDuration(24_h);

  if (!DoesIncludeAll(openingTime, spans))
    return false;

  for (auto & span : result)
  {
    if (span.HasExtendedHours())
      span.GetEnd().GetHourMinutes().AddDuration(-24_h);
  }

  spans.swap(result);
  return true;
}

osmoh::Timespan GetLongetsOpenSpan(osmoh::Timespan const & openingTime,
                                   osmoh::TTimespans const & excludeTime)
{
  if (excludeTime.empty())
    return openingTime;

  osmoh::Timespan longestSpan{openingTime.GetStart(), excludeTime.front().GetStart()};
  for (auto i = 0; i < excludeTime.size() - 1; ++i)
  {
    osmoh::Timespan nextOpenSpan{excludeTime[i].GetEnd(), excludeTime[i + 1].GetStart()};
    longestSpan = SpanLength(longestSpan) > SpanLength(nextOpenSpan) ? longestSpan : nextOpenSpan;
  }

  osmoh::Timespan lastSpan{excludeTime.back().GetEnd(), openingTime.GetEnd()};
  return SpanLength(longestSpan) > SpanLength(lastSpan) ? longestSpan : lastSpan;
}
} // namespace

namespace editor
{
namespace ui
{

// TimeTable ---------------------------------------------------------------------------------------

TimeTable TimeTable::GetPredefinedTimeTable()
{
  TimeTable tt;
  tt.m_isTwentyFourHours = true;
  tt.m_weekdays = {
    osmoh::Weekday::Sunday,
    osmoh::Weekday::Monday,
    osmoh::Weekday::Tuesday,
    osmoh::Weekday::Wednesday,
    osmoh::Weekday::Thursday,
    osmoh::Weekday::Friday,
    osmoh::Weekday::Saturday
  };

  tt.m_openingTime = tt.GetPredefinedOpeningTime();

  return tt;
}

bool TimeTable::SetWorkingDays(TOpeningDays const & days)
{
  if (days.empty())
    return true;
  m_weekdays = days;
  return true;
}

void TimeTable::AddWorkingDay(osmoh::Weekday const wd)
{
  if (wd != osmoh::Weekday::None)
    m_weekdays.insert(wd);
}

bool TimeTable::RemoveWorkingDay(osmoh::Weekday const wd)
{
  if (m_weekdays.size() == 1)
    return false;
  m_weekdays.erase(wd);
  return true;
}

bool TimeTable::SetOpeningTime(osmoh::Timespan const & span)
{
  if (IsTwentyFourHours())
    return false;

  m_openingTime = span;
  osmoh::TTimespans excludeTime;
  for (auto const & excludeSpan : GetExcludeTime())
  {
    auto const openingTimeStart = GetOpeningTime().GetStart().GetHourMinutes().GetDuration();
    auto const openingTimeEnd = GetOpeningTime().GetEnd().GetHourMinutes().GetDuration();
    auto const excludeSpanStart = excludeSpan.GetStart().GetHourMinutes().GetDuration();
    auto const excludeSpanEnd = excludeSpan.GetEnd().GetHourMinutes().GetDuration();

    if (excludeSpanStart < openingTimeStart || openingTimeEnd < excludeSpanEnd)
      continue;

    excludeTime.push_back(excludeSpan);
  }

  m_excludeTime.swap(excludeTime);
  return true;
}

bool TimeTable::CanAddExcludeTime() const
{
  return !GetPredefinedExcludeTime().IsEmpty();
}

bool TimeTable::AddExcludeTime(osmoh::Timespan const & span)
{
  return ReplaceExcludeTime(span, GetExcludeTime().size());
}

bool TimeTable::ReplaceExcludeTime(osmoh::Timespan const & span, size_t const index)
{
  if (IsTwentyFourHours() || index > m_excludeTime.size())
    return false;

  auto copy = m_excludeTime;
  if (index == m_excludeTime.size())
    copy.push_back(span);
  else
    copy[index] = span;

  if (!FixTimeSpans(m_openingTime, copy))
    return false;

  m_excludeTime.swap(copy);
  return true;
}

bool TimeTable::RemoveExcludeTime(size_t const index)
{
  if (IsTwentyFourHours() || index >= m_excludeTime.size())
    return false;
  m_excludeTime.erase(begin(m_excludeTime) + index);
  return true;
}

bool TimeTable::IsValid() const
{
  if (m_weekdays.empty())
    return false;

  if (!IsTwentyFourHours())
  {
    if (m_openingTime.IsEmpty())
      return false;

    auto copy = GetExcludeTime();
    if (!FixTimeSpans(m_openingTime, copy))
      return false;
  }

  return true;
}

osmoh::Timespan TimeTable::GetPredefinedOpeningTime() const
{
  using osmoh::operator""_h;
  return {10_h, 22_h};
}

osmoh::Timespan TimeTable::GetPredefinedExcludeTime() const
{
  using osmoh::operator""_h;
  using osmoh::operator""_min;
  auto longestOpenSpan = GetLongetsOpenSpan(GetOpeningTime(), GetExcludeTime());

  auto const longestOpenSpanLength = SpanLength(longestOpenSpan);
  auto offset = longestOpenSpanLength / 4;

  auto const remainder = offset % 10;
  if (remainder && remainder != 5)
    offset -= remainder;

  longestOpenSpan.GetStart().GetHourMinutes().AddDuration(osmoh::HourMinutes::TMinutes(offset));
  longestOpenSpan.GetEnd().GetHourMinutes().AddDuration(-osmoh::HourMinutes::TMinutes(offset));

  if (SpanLength(longestOpenSpan) < (30_min).count())
    return osmoh::Timespan{};

  return longestOpenSpan;
}

// TimeTableSet ------------------------------------------------------------------------------------

TimeTableSet::TimeTableSet()
{
  push_back(TimeTable::GetPredefinedTimeTable());
}

TOpeningDays TimeTableSet::GetUnhandledDays() const
{
  TOpeningDays days = {
    osmoh::Weekday::Sunday,
    osmoh::Weekday::Monday,
    osmoh::Weekday::Tuesday,
    osmoh::Weekday::Wednesday,
    osmoh::Weekday::Thursday,
    osmoh::Weekday::Friday,
    osmoh::Weekday::Saturday
  };

  for (auto const tt : *this)
    for (auto const day : tt.GetWorkingDays())
      days.erase(day);

  return days;
}

TimeTable TimeTableSet::GetComplementTimeTable() const
{
  TimeTable tt;
  tt.SetTwentyFourHours(true);
  tt.SetWorkingDays(GetUnhandledDays());
  return tt;
}

bool TimeTableSet::Append(TimeTable const & tt)
{
  auto copy = *this;
  copy.push_back(tt);

  if (!TimeTableSet::UpdateByIndex(copy, copy.size() - 1))
    return false;

  swap(copy);
  return true;
}

bool TimeTableSet::Remove(size_t const index)
{
  if (index == 0 || index >= size())
    return false;

  erase(begin() + index);

  return true;
}

bool TimeTableSet::Replace(TimeTable const & tt, size_t const index)
{
  if (index >= size())
    return false;

  auto copy = *this;
  copy[index] = tt;

  if (!TimeTableSet::UpdateByIndex(copy, index))
    return false;

  swap(copy);
  return true;
}

bool TimeTableSet::UpdateByIndex(TimeTableSet & ttSet, size_t const index)
{
  if (index >= ttSet.size() || !ttSet[index].IsValid())
    return false;

  auto const & updated = ttSet[index];

  for (auto i = 0; i < ttSet.size(); ++i)
  {
    if (i == index)
      continue;

    // Remove all days of updated timetable from all other timetables.
    TOpeningDays days;
    set_difference(std::begin(ttSet[i].GetWorkingDays()), std::end(ttSet[i].GetWorkingDays()),
                   std::begin(updated.GetWorkingDays()), std::end(updated.GetWorkingDays()),
                   inserter(days, std::end(days)));

    if (!ttSet[i].SetWorkingDays(days))
      return false;
  }

  return true;
}
} // namspace ui
} // namespace editor
