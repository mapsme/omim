/*
  The MIT License (MIT)

  Copyright (c) 2015 Mail.Ru Group

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

#include "opening_hours.hpp"
#include "rules_evaluation.hpp"
#include "parse_opening_hours.hpp"

#include <cstdlib>
#include <iomanip>
#include <ios>
#include <ostream>
#include <vector>

namespace
{
template <typename T, typename SeparatorExtractor>
void PrintVector(std::ostream & ost, std::vector<T> const & v,
                 SeparatorExtractor && sepFunc)
{
  auto it = begin(v);
  if (it == end(v))
    return;

  auto sep = sepFunc(*it);
  ost << *it++;
  while (it != end(v))
  {
    ost << sep << *it;
    sep = sepFunc(*it);
    ++it;
  }
}

template <typename T>
void PrintVector(std::ostream & ost, std::vector<T> const & v, char const * const sep = ", ")
{
  PrintVector(ost, v, [&sep](T const &) { return sep; });
}

void PrintOffset(std::ostream & ost, int32_t const offset, bool const space)
{
  if (offset == 0)
    return;

  if (space)
    ost << ' ';
  if (offset > 0)
    ost << '+';
  ost << offset;
  ost << ' ' << "day";
  if (std::abs(offset) > 1)
    ost << 's';
}

class StreamFlagsKeeper
{
 public:
  explicit StreamFlagsKeeper(std::ostream & ost):
      m_ost(ost),
      m_flags(m_ost.flags())
  {
  }

  ~StreamFlagsKeeper()
  {
    m_ost.flags(m_flags);
  }

 private:
  std::ostream & m_ost;
  std::ios_base::fmtflags m_flags;
};

void PrintPaddedNumber(std::ostream & ost, uint32_t const number, uint32_t const padding = 1)
{
  StreamFlagsKeeper keeper(ost);
  ost << std::setw(padding) << std::setfill('0') << number;
}

void PrintHoursMinutes(std::ostream & ost,
                       std::chrono::hours::rep hours,
                       std::chrono::minutes::rep minutes)
{
  PrintPaddedNumber(ost, hours, 2);
  ost << ':';
  PrintPaddedNumber(ost, minutes, 2);
}

} // namespace

namespace osmoh
{

// HourMinutes -------------------------------------------------------------------------------------

bool HourMinutes::IsExtended() const
{
  return GetDuration() > 24_h;
}

void HourMinutes::SetHours(THours const hours)
{
  m_empty = false;
  m_hours = hours;
}

void HourMinutes::SetMinutes(TMinutes const minutes)
{
  m_empty = false;
  m_minutes = minutes;
}

void HourMinutes::SetDuration(TMinutes const duration)
{
  SetHours(std::chrono::duration_cast<THours>(duration));
  SetMinutes(duration - GetHours());
}

HourMinutes operator-(HourMinutes const & hm)
{
  HourMinutes result;
  result.SetHours(-hm.GetHours());
  result.SetMinutes(-hm.GetMinutes());
  return result;
}

std::ostream & operator<<(std::ostream & ost, HourMinutes const & hm)
{
  if (hm.IsEmpty())
    ost << "hh:mm";
  else
    PrintHoursMinutes(ost, std::abs(hm.GetHoursCount()), std::abs(hm.GetMinutesCount()));
  return ost;
}

// TimeEvent ---------------------------------------------------------------------------------------
Time TimeEvent::GetEventTime() const
{
  return Time(HourMinutes(0_h + 0_min)); // TODO(mgsergio): get real time
}

std::ostream & operator<<(std::ostream & ost, TimeEvent::Event const event)
{
  switch (event)
  {
    case TimeEvent::Event::None:
      ost << "None";
    case TimeEvent::Event::Sunrise:
      ost << "sunrise";
      break;
    case TimeEvent::Event::Sunset:
      ost << "sunset";
      break;
  }
  return ost;
}

std::ostream & operator<<(std::ostream & ost, TimeEvent const te)
{
  if (te.HasOffset())
  {
    ost << '(' << te.GetEvent();

    auto const & offset = te.GetOffset();

    if (offset.GetHoursCount() < 0)
      ost << '-';
    else
      ost << '+';

    ost << offset << ')';
  }
  else
  {
    ost << te.GetEvent();
  }

  return ost;
}

// Time --------------------------------------------------------------------------------------------
Time::THours Time::GetHours() const
{
  if (IsEvent())
    return GetEvent().GetEventTime().GetHours();
  return GetHourMinutes().GetHours();
}

Time::TMinutes Time::GetMinutes() const
{
  if (IsEvent())
    return GetEvent().GetEventTime().GetMinutes();
  return GetHourMinutes().GetMinutes();
}

void Time::AddDuration(TMinutes const duration)
{
  if (IsEvent())
  {
    m_event.AddDurationToOffset(duration);
  }
  else if (IsHoursMinutes())
  {
    m_hourMinutes.AddDuration(duration);
  }
  else
  {
    // Undefined behaviour.
  }
}

void Time::SetEvent(TimeEvent const & event)
{
  m_type = Type::Event;
  m_event = event;
}

void Time::SetHourMinutes(HourMinutes const & hm)
{
  m_type = Type::HourMinutes;
  m_hourMinutes = hm;
}

std::ostream & operator<<(std::ostream & ost, Time const & time)
{
  if (time.IsEmpty())
  {
    ost << "hh:mm";
    return ost;
  }

  if (time.IsEvent())
    ost << time.GetEvent();
  else
    ost << time.GetHourMinutes();

  return ost;
}

// TimespanPeriod ----------------------------------------------------------------------------------
TimespanPeriod::TimespanPeriod(HourMinutes const & hm):
    m_hourMinutes(hm),
    m_type(Type::HourMinutes)
{
}

TimespanPeriod::TimespanPeriod(HourMinutes::TMinutes const minutes):
    m_minutes(minutes),
    m_type(Type::Minutes)
{
}

std::ostream & operator<<(std::ostream & ost, TimespanPeriod const p)
{
  if (p.IsEmpty())
    ost << "None";
  else if (p.IsHoursMinutes())
    ost << p.GetHourMinutes();
  else if (p.IsMinutes())
    PrintPaddedNumber(ost, p.GetMinutesCount(), 2);
  return ost;
}

// Timespan ----------------------------------------------------------------------------------------
bool Timespan::HasExtendedHours() const
{
  bool const canHaveExtendedHours = HasStart() && HasEnd() &&
                                    GetStart().IsHoursMinutes() &&
                                    GetEnd().IsHoursMinutes();
  if (!canHaveExtendedHours)
  {
    return false;
  }

  auto const & startHM = GetStart().GetHourMinutes();
  auto const & endHM = GetEnd().GetHourMinutes();

  if (endHM.IsExtended())
    return true;

  return endHM.GetDurationCount() != 0 && (endHM.GetDuration() < startHM.GetDuration());
}

std::ostream & operator<<(std::ostream & ost, Timespan const & span)
{
  ost << span.GetStart();
  if (!span.IsOpen())
  {
    ost << '-' << span.GetEnd();
    if (span.HasPeriod())
      ost << '/' << span.GetPeriod();
  }
  if (span.HasPlus())
    ost << '+';
  return ost;
}

std::ostream & operator<<(std::ostream & ost, osmoh::TTimespans const & timespans)
{
  PrintVector(ost, timespans);
  return ost;
}

// NthWeekdayOfTheMonthEntry -----------------------------------------------------------------------
std::ostream & operator<<(std::ostream & ost, NthWeekdayOfTheMonthEntry const entry)
{
  if (entry.HasStart())
    ost << static_cast<uint32_t>(entry.GetStart());
  if (entry.HasEnd())
    ost << '-' << static_cast<uint32_t>(entry.GetEnd());
  return ost;
}

// WeekdayRange ------------------------------------------------------------------------------------
bool WeekdayRange::HasWday(Weekday const & wday) const
{
  if (IsEmpty() || wday == Weekday::None)
    return false;

  if (!HasEnd())
    return GetStart() == wday;

  return GetStart() <= wday && wday <= GetEnd();
}


std::ostream & operator<<(std::ostream & ost, Weekday const wday)
{
  switch (wday)
  {
    case Weekday::Sunday:
      ost << "Su";
      break;
    case Weekday::Monday:
      ost << "Mo";
      break;
    case Weekday::Tuesday:
      ost << "Tu";
      break;
    case Weekday::Wednesday:
      ost << "We";
      break;
    case Weekday::Thursday:
      ost << "Th";
      break;
    case Weekday::Friday:
      ost << "Fr";
      break;
    case Weekday::Saturday:
      ost << "Sa";
      break;
    case Weekday::None:
      ost << "None";
  }
  return ost;
}

std::ostream & operator<<(std::ostream & ost, WeekdayRange const & range)
{
  ost << range.GetStart();
  if (range.HasEnd())
  {
    ost << '-' << range.GetEnd();
  }
  else
  {
    if (range.HasNth())
    {
      ost << '[';
      PrintVector(ost, range.GetNths(), ",");
      ost << ']';
    }
    PrintOffset(ost, range.GetOffset(), true);
  }
  return ost;
}

std::ostream & operator<<(std::ostream & ost, TWeekdayRanges const & ranges)
{
  PrintVector(ost, ranges);
  return ost;
}

// Holiday -----------------------------------------------------------------------------------------
std::ostream & operator<<(std::ostream & ost, Holiday const & holiday)
{
  if (holiday.IsPlural())
  {
    ost << "PH";
  }
  else
  {
    ost << "SH";
    PrintOffset(ost, holiday.GetOffset(), true);
  }
  return ost;
}

std::ostream & operator<<(std::ostream & ost, THolidays const & holidays)
{
  PrintVector(ost, holidays);
  return ost;
}

// Weekdays ----------------------------------------------------------------------------------------

std::ostream & operator<<(std::ostream & ost, Weekdays const & weekday)
{
  ost << weekday.GetHolidays();
  if (weekday.HasWeekday() && weekday.HasHolidays())
    ost << ", ";
  ost << weekday.GetWeekdayRanges();
  return ost;
}

// DateOffset --------------------------------------------------------------------------------------
std::ostream & operator<<(std::ostream & ost, DateOffset const & offset)
{
  if (offset.HasWDayOffset())
  {
    ost << (offset.IsWDayOffsetPositive() ? '+' : '-')
        << offset.GetWDayOffset();
  }
  PrintOffset(ost, offset.GetOffset(), offset.HasWDayOffset());
  return ost;
}

// MonthDay ----------------------------------------------------------------------------------------
std::ostream & operator<<(std::ostream & ost, MonthDay::Month const month)
{
  switch (month)
  {
    case MonthDay::Month::None:
      ost << "None";
      break;
    case MonthDay::Month::Jan:
      ost << "Jan";
      break;
    case MonthDay::Month::Feb:
      ost << "Feb";
      break;
    case MonthDay::Month::Mar:
      ost << "Mar";
      break;
    case MonthDay::Month::Apr:
      ost << "Apr";
      break;
    case MonthDay::Month::May:
      ost << "May";
      break;
    case MonthDay::Month::Jun:
      ost << "Jun";
      break;
    case MonthDay::Month::Jul:
      ost << "Jul";
      break;
    case MonthDay::Month::Aug:
      ost << "Aug";
      break;
    case MonthDay::Month::Sep:
      ost << "Sep";
      break;
    case MonthDay::Month::Oct:
      ost << "Oct";
      break;
    case MonthDay::Month::Nov:
      ost << "Nov";
      break;
    case MonthDay::Month::Dec:
      ost << "Dec";
      break;
  }
  return ost;
}

std::ostream & operator<<(std::ostream & ost, MonthDay::VariableDate const date)
{
  switch (date)
  {
    case MonthDay::VariableDate::None:
      ost << "none";
      break;
    case MonthDay::VariableDate::Easter:
      ost << "easter";
      break;
  }
  return ost;
}

std::ostream & operator<<(std::ostream & ost, MonthDay const md)
{
  bool space = false;
  auto const putSpace = [&space, &ost] {
    if (space)
      ost << ' ';
    space = true;
  };

  if (md.HasYear())
  {
    putSpace();
    ost << md.GetYear();
  }

  if (md.IsVariable())
  {
    putSpace();
    ost << md.GetVariableDate();
  }
  else
  {
    if (md.HasMonth())
    {
      putSpace();
      ost << md.GetMonth();
    }
    if (md.HasDayNum())
    {
      putSpace();
      PrintPaddedNumber(ost, md.GetDayNum(), 2);
    }
  }
  if (md.HasOffset())
  {
    ost << ' ' << md.GetOffset();
  }
  return ost;
}

// MonthdayRange -----------------------------------------------------------------------------------
std::ostream & operator<<(std::ostream & ost, MonthdayRange const & range)
{
  if (range.HasStart())
    ost << range.GetStart();
  if (range.HasEnd())
  {
    ost << '-' << range.GetEnd();
    if (range.HasPeriod())
      ost << '/' << range.GetPeriod();
  }
  else if (range.HasPlus())
    ost << '+';
  return ost;
}

std::ostream & operator<<(std::ostream & ost, TMonthdayRanges const & ranges)
{
  PrintVector(ost, ranges);
  return ost;
}

// YearRange ---------------------------------------------------------------------------------------
std::ostream & operator<<(std::ostream & ost, YearRange const range)
{
  if (range.IsEmpty())
    return ost;

  ost << range.GetStart();
  if (range.HasEnd())
  {
    ost << '-' << range.GetEnd();
    if (range.HasPeriod())
      ost << '/' << range.GetPeriod();
  }
  else if (range.HasPlus())
  {
    ost << '+';
  }

  return ost;
}

std::ostream & operator<<(std::ostream & ost, TYearRanges const ranges)
{
  PrintVector(ost, ranges);
  return ost;
}

// WeekRange ---------------------------------------------------------------------------------------
std::ostream & operator<<(std::ostream & ost, WeekRange const range)
{
  if (range.IsEmpty())
    return ost;

  PrintPaddedNumber(ost, range.GetStart(), 2);
  if (range.HasEnd())
  {
    ost << '-';
    PrintPaddedNumber(ost, range.GetEnd(), 2);
    if (range.HasPeriod())
      ost << '/' << range.GetPeriod();
  }
  return ost;
}

std::ostream & operator<<(std::ostream & ost, TWeekRanges const ranges)
{
  ost << "week ";
  PrintVector(ost, ranges);
  return ost;
}

// RuleSequence ------------------------------------------------------------------------------------
std::ostream & operator<<(std::ostream & ost, RuleSequence::Modifier const modifier)
{
  switch (modifier)
  {
    case RuleSequence::Modifier::DefaultOpen:
    case RuleSequence::Modifier::Comment:
      break;
    case RuleSequence::Modifier::Unknown:
      ost << "unknown";
      break;
    case RuleSequence::Modifier::Closed:
      ost << "closed";
      break;
    case RuleSequence::Modifier::Open:
      ost << "open";
      break;
  }
  return ost;
}

std::ostream & operator<<(std::ostream & ost, RuleSequence const & s)
{
  bool space = false;
  auto const putSpace = [&space, &ost] {
    if (space)
      ost << ' ';
    space = true;
  };

  if (s.IsTwentyFourHours())
  {
    putSpace();
    ost << "24/7";
  }
  else
  {
    if (s.HasComment())
      ost << s.GetComment() << ':';
    else
    {
      if (s.HasYears())
      {
        putSpace();
        ost << s.GetYears();
      }
      if (s.HasMonths())
      {
        putSpace();
        ost << s.GetMonths();
      }
      if (s.HasWeeks())
      {
        putSpace();
        ost << s.GetWeeks();
      }

      if (s.HasSeparatorForReadability())
        ost << ':';

      if (s.HasWeekdays())
      {
        putSpace();
        ost << s.GetWeekdays();
      }
      if (s.HasTimes())
      {
        putSpace();
        ost << s.GetTimes();
      }
    }
  }
  if (s.GetModifier() != RuleSequence::Modifier::DefaultOpen &&
      s.GetModifier() != RuleSequence::Modifier::Comment)
  {
    putSpace();
    ost << s.GetModifier();
  }
  if (s.HasModifierComment())
  {
    putSpace();
    ost << '"' << s.GetModifierComment() << '"';
  }

  return ost;
}

std::ostream & operator<<(std::ostream & ost, TRuleSequences const & s)
{
  PrintVector(ost, s, [](RuleSequence const & r) {
      auto const sep = r.GetAnySeparator();
      return (sep == "||" ? ' ' + sep + ' ' : sep + ' ');
    });
  return ost;
}

// OpeningHours ------------------------------------------------------------------------------------
OpeningHours::OpeningHours(std::string const & rule):
    m_valid(Parse(rule, m_rule))
{
}

OpeningHours::OpeningHours(TRuleSequences const & rule):
    m_rule(rule),
    m_valid(true)
{
}

bool OpeningHours::IsOpen(time_t const dateTime) const
{
  return osmoh::IsOpen(m_rule, dateTime);
}

bool OpeningHours::IsClosed(time_t const dateTime) const
{
  return osmoh::IsClosed(m_rule, dateTime);
}

bool OpeningHours::IsUnknown(time_t const dateTime) const
{
  return osmoh::IsUnknown(m_rule, dateTime);
}

bool OpeningHours::IsValid() const
{
  return m_valid;
}
} // namespace osmoh
