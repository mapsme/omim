#pragma once

#include "std/ctime.hpp"

namespace base
{

bool IsLeapYear(int year);

// For some reasons android doesn't have timegm function. The following
// work around is provided.
time_t TimeGM(std::tm const & tm);

// year - since 0, for example 2015
// month - 1-jan...12-dec
// day - 1...31
// hour - 0...23
// min - 0...59
// sec - 0...59
time_t TimeGM(int year, int month, int day, int hour, int min, int sec);

} // base
