#pragma once

#include "std/ctime.hpp"
#include "std/string.hpp"
#include "std/utility.hpp"

enum class DayTimeType
{
  DayTime,
  NightTime,
  PolarDayTime,
  PolarNightTime
};

string DebugPrint(DayTimeType type);

/// Calculates timestamps of the sunrise and sunset for a specified date
/// in a specified location.
/// @param year - year, since 0, like 2015
/// @param month - month, 1-jan...12-dec
/// @param day - day of month 1...31
/// @param latitude - latutude, -90...+90 degrees
/// @param longitude - longitude, -180...+180 degrees
/// @returns pair where first is UTC time of sunrise and second is UTC time of sunset. sunrise <= sunset
/// @note date year/month/day is specified for the interesting point latituda/longitude
/// @note for polar day sunrise is set to year/month/day,0:0:0 and sunset is set to sunrise + 24h - 24h of day
/// @note for polar night sunrise and sunset both are set to year/month/day,0:0:0 - 0 sec of day
pair<time_t, time_t> CalculateSunriseSunsetTime(int year, int month, int day, double latitude, double longitude);

/// Helpers which calculates 'is day time' without a time calculation error.
/// @param timeUtc - utc time
/// @param latitude - latutude, -90...+90 degrees
/// @param longitude - longitude, -180...+180 degrees
/// @returns day time for a specified date for a specified location
/// @note throws RootException if gmtime returns nullptr
DayTimeType GetDayTime(time_t timeUtc, double latitude, double longitude);

/// Helper function which calculates next day date
void NextDay(int & year, int & month, int & day);

/// Helper function which calculates prev day date
void PrevDay(int & year, int & month, int & day);

