#include "editor/ui2oh.hpp"

#include "std/array.hpp"
#include "std/string.hpp"

#include "3party/opening_hours/opening_hours.hpp"

// namespace
// {
// osmoh::TWeekdayRanges DaysSetToRange( const & days)
// {

//   TWeekdayRanges ragnes;
//   Weekday previous = Weekday::None;
//   for (auto const & wd : days)
//   {
//     auto & range = ranges.back();
//     if (previous == Weekday::None)
//     {
//       ranges.push_back(WeekdayRange());
//       range.SetStart(wd);
//     }
//     else if (static_cast<int>(wd) - static_cast<int>(previous) > 1)
//     {
//       ranges.push_back(WeekdayRange());
//       range.SetStart(wd);
//     }
//   }
//   return {};
// }
// } // namespace

namespace editor
{
osmoh::OpeningHours ConvertOpeningHours(ui::TimeTableSet const & tt)
{
  return string(); // TODO(mgsergio): // Just a dummy.
}

ui::TimeTableSet ConvertOpeningHours(osmoh::OpeningHours const & oh)
{
  return {};
}
} // namespace editor
