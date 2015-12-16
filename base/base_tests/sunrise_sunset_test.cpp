#include "testing/testing.hpp"

#include "base/sunrise_sunset.hpp"

#include "base/logging.hpp"
#include "base/timegm.hpp"

// Test site for sunrise and sunset is
// http://voshod-solnca.ru/

namespace
{

time_t constexpr kOneDaySeconds = 24 * 60 * 60;

time_t constexpr kTimeEqualityRangeSec = 10 * 60; // 10 minutes

bool TimesEqual(time_t examinedTime, time_t sampleTime, time_t range = kTimeEqualityRangeSec)
{
  time_t const halfRange = range / 2;
  bool const res = examinedTime >= (sampleTime - halfRange) &&
                   examinedTime <= (sampleTime + halfRange);
  if (!res)
  {
    string const examined = asctime(gmtime(&examinedTime));
    string const sample = asctime(gmtime(&sampleTime));
    LOG(LINFO, ("Times are not equal: examined time", examined, "sample time", sample));
  }
  return res;
}

} // namespace

UNIT_TEST(SunriseSunsetAlgorithm_Moscow_April)
{
  // Moscow (utc +3), date 2015/4/12:
  // Sunrise utc time: 2015/4/12,2:34
  // Sunset utc time: 2015/4/12,16:29
  double const lat = 55.7522222;
  double const lon = 37.6155556;

  pair<time_t, time_t> sunriseSunset = CalculateSunriseSunsetTime(2015, 4, 12, lat, lon);
  TEST(TimesEqual(sunriseSunset.first, base::TimeGM(2015, 4, 12, 2, 34, 0)), ());
  TEST(TimesEqual(sunriseSunset.second, base::TimeGM(2015, 4, 12, 16, 29, 0)), ());
}

UNIT_TEST(SunriseSunsetAlgorithm_Moscow_July)
{
  // Moscow (utc +3), date 2015/7/13:
  // Sunrise utc time: 2015/7/13,1:04
  // Sunset utc time: 2015/7/13,18:09
  double const lat = 55.7522222;
  double const lon = 37.6155556;

  pair<time_t, time_t> sunriseSunset = CalculateSunriseSunsetTime(2015, 7, 13, lat, lon);
  TEST(TimesEqual(sunriseSunset.first, base::TimeGM(2015, 7, 13, 1, 4, 0)), ());
  TEST(TimesEqual(sunriseSunset.second, base::TimeGM(2015, 7, 13, 18, 9, 0)), ());
}

UNIT_TEST(SunriseSunsetAlgorithm_Moscow_September)
{
  // Moscow (utc +3), date 2015/9/17:
  // Sunrise utc time: 2015/9/17,3:05
  // Sunset utc time: 2015/9/17,15:46
  double const lat = 55.7522222;
  double const lon = 37.6155556;

  pair<time_t, time_t> sunriseSunset = CalculateSunriseSunsetTime(2015, 9, 17, lat, lon);
  TEST(TimesEqual(sunriseSunset.first, base::TimeGM(2015, 9, 17, 3, 5, 0)), ());
  TEST(TimesEqual(sunriseSunset.second, base::TimeGM(2015, 9, 17, 15, 46, 0)), ());
}

UNIT_TEST(SunriseSunsetAlgorithm_Moscow_December)
{
  // Moscow (utc +3), date 2015/12/25:
  // Sunrise utc time: 2015/12/25,06:00
  // Sunset utc time: 2015/12/25,13:01
  double const lat = 55.7522222;
  double const lon = 37.6155556;

  pair<time_t, time_t> sunriseSunset = CalculateSunriseSunsetTime(2015, 12, 25, lat, lon);
  TEST(TimesEqual(sunriseSunset.first, base::TimeGM(2015, 12, 25, 6, 0, 0)), ());
  TEST(TimesEqual(sunriseSunset.second, base::TimeGM(2015, 12, 25, 13, 1, 0)), ());
}

UNIT_TEST(SunriseSunsetAlgorithm_Moscow_NewYear_1)
{
  // Moscow (utc +3), date 2016/1/1:
  // Sunrise utc time: 2016/1/1,6:1
  // Sunset utc time: 2016/1/1,13:7
  double const lat = 55.7522222;
  double const lon = 37.6155556;

  pair<time_t, time_t> sunriseSunset = CalculateSunriseSunsetTime(2016, 1, 1, lat, lon);
  TEST(TimesEqual(sunriseSunset.first, base::TimeGM(2016, 1, 1, 6, 1, 0)), ());
  TEST(TimesEqual(sunriseSunset.second, base::TimeGM(2016, 1, 1, 13, 7, 0)), ());
}

UNIT_TEST(SunriseSunsetAlgorithm_Moscow_NewYear_2)
{
  // Moscow (utc +3), local time 2016/1/1,2:0, utc time 2015/12/31,23:0
  // Sunrise utc time: 2016/1/1,6:1
  // Sunset utc time: 2016/1/1,13:7
  double const lat = 55.7522222;
  double const lon = 37.6155556;

  pair<time_t, time_t> sunriseSunset = CalculateSunriseSunsetTime(base::TimeGM(2015, 12, 31, 23, 0, 0), lat, lon);
  TEST(TimesEqual(sunriseSunset.first, base::TimeGM(2016, 1, 1, 6, 1, 0)), ());
  TEST(TimesEqual(sunriseSunset.second, base::TimeGM(2016, 1, 1, 13, 7, 0)), ());
}

UNIT_TEST(SunriseSunsetAlgorithm_GetDayTime_Moscow_December)
{
  // Moscow (utc +3), 2015/12/24
  // prev sunset utc 2015/12/23,13:00
  // sunrise utc 2015/12/24,6:00
  // sunset utc 2015/12/24,13:00
  // next sunrise utc 2015/12/24,6:00
  double const lat = 55.7522222;
  double const lon = 37.6155556;

  // before sunrise but after prev sunset
  pair<DayTimeType, time_t> dayTime = GetDayTime(base::TimeGM(2015, 12, 23, 17, 0, 0), lat, lon);
  TEST_EQUAL(dayTime.first, DayTimeType::NightTime, ());
  TEST(TimesEqual(dayTime.second, base::TimeGM(2015, 12, 24, 6, 0, 0)), ());

  // between sunrise and sunset
  dayTime = GetDayTime(base::TimeGM(2015, 12, 24, 10, 53, 0), lat, lon);
  TEST_EQUAL(dayTime.first, DayTimeType::DayTime, ());
  TEST(TimesEqual(dayTime.second, base::TimeGM(2015, 12, 24, 13, 0, 0)), ());

  // after sunset and before next sunrise
  dayTime = GetDayTime(base::TimeGM(2015, 12, 24, 16, 30, 0), lat, lon);
  TEST_EQUAL(dayTime.first, DayTimeType::NightTime, ());
  TEST(TimesEqual(dayTime.second, base::TimeGM(2015, 12, 25, 6, 0, 0)), ());
}

UNIT_TEST(SunriseSunsetAlgorithm_Paris_NewYear)
{
  // Paris (utc +1)
  // Sunrise utc time: 2016/1/1,7:45
  // Sunset utc time: 2016/1/1,16:04
  double const lat = 48.875649;
  double const lon = 2.344428;

  pair<time_t, time_t> sunriseSunset = CalculateSunriseSunsetTime(base::TimeGM(2015, 12, 31, 23, 30, 0), lat, lon);
  TEST(TimesEqual(sunriseSunset.first, base::TimeGM(2016, 1, 1, 7, 45, 0)), ());
  TEST(TimesEqual(sunriseSunset.second, base::TimeGM(2016, 1, 1, 16, 4, 0)), ());
}

UNIT_TEST(SunriseSunsetAlgorithm_Honolulu_February)
{
  // Honolulu (utc -10), date 2015/2/12:
  // Sunrise utc time: 2015/2/12,17:05
  // Sunset utc time: 2015/2/13,4:29
  double const lat = 21.307431;
  double const lon = -157.848568;

  pair<time_t, time_t> sunriseSunset = CalculateSunriseSunsetTime(2015, 2, 12, lat, lon);
  TEST(TimesEqual(sunriseSunset.first, base::TimeGM(2015, 2, 12, 17, 5, 0)), ());
  TEST(TimesEqual(sunriseSunset.second, base::TimeGM(2015, 2, 13, 4, 29, 0)), ());

  auto dayTime = GetDayTime(base::TimeGM(2015, 2, 12, 15, 15, 0), lat, lon);
  TEST_EQUAL(dayTime.first, DayTimeType::NightTime, ());
  TEST_EQUAL(dayTime.second, sunriseSunset.first, ());

  dayTime = GetDayTime(base::TimeGM(2015, 2, 12, 17, 10, 0), lat, lon);
  TEST_EQUAL(dayTime.first, DayTimeType::DayTime, ());
  TEST_EQUAL(dayTime.second, sunriseSunset.second, ());

  dayTime = GetDayTime(base::TimeGM(2015, 2, 13, 2, 30, 0), lat, lon);
  TEST_EQUAL(dayTime.first, DayTimeType::DayTime, ());
  TEST_EQUAL(dayTime.second, sunriseSunset.second, ());

  dayTime = GetDayTime(base::TimeGM(2015, 2, 13, 5, 0, 0), lat, lon);
  TEST_EQUAL(dayTime.first, DayTimeType::NightTime, ());
}

UNIT_TEST(SunriseSunsetAlgorithm_Honolulu_July)
{
  // Honolulu (utc -10). For date 2015/7/13:
  // Sunrise utc time: 2015/7/13,15:58
  // Sunset utc time: 2015/7/14,5:18
  double const lat = 21.307431;
  double const lon = -157.848568;

  pair<time_t, time_t> sunriseSunset = CalculateSunriseSunsetTime(2015, 7, 13, lat, lon);
  TEST(TimesEqual(sunriseSunset.first, base::TimeGM(2015, 7, 13, 15, 58, 0)), ());
  TEST(TimesEqual(sunriseSunset.second, base::TimeGM(2015, 7, 14, 5, 18, 0)), ());
}

UNIT_TEST(SunriseSunsetAlgorithm_Honolulu_June)
{
  // Honolulu (utc -10). For date 2015/6/22:
  // Sunrise utc time: 2015/6/22,15:51
  // Sunset utc time: 2015/6/23,5:17
  double const lat = 21.307431;
  double const lon = -157.848568;

  pair<time_t, time_t> sunriseSunset = CalculateSunriseSunsetTime(2015, 6, 22, lat, lon);
  TEST(TimesEqual(sunriseSunset.first, base::TimeGM(2015, 6, 22, 15, 51, 0)), ());
  TEST(TimesEqual(sunriseSunset.second, base::TimeGM(2015, 6, 23, 5, 17, 0)), ());

  sunriseSunset = CalculateSunriseSunsetTime(base::TimeGM(2015, 6, 22, 15, 0, 0), lat, lon);
  TEST(TimesEqual(sunriseSunset.first, base::TimeGM(2015, 6, 22, 15, 51, 0)), ());
  TEST(TimesEqual(sunriseSunset.second, base::TimeGM(2015, 6, 23, 5, 17, 0)), ());

  sunriseSunset = CalculateSunriseSunsetTime(base::TimeGM(2015, 6, 22, 19, 0, 0), lat, lon);
  TEST(TimesEqual(sunriseSunset.first, base::TimeGM(2015, 6, 22, 15, 51, 0)), ());
  TEST(TimesEqual(sunriseSunset.second, base::TimeGM(2015, 6, 23, 5, 17, 0)), ());

  sunriseSunset = CalculateSunriseSunsetTime(base::TimeGM(2015, 6, 23, 1, 0, 0), lat, lon);
  TEST(TimesEqual(sunriseSunset.first, base::TimeGM(2015, 6, 22, 15, 51, 0)), ());
  TEST(TimesEqual(sunriseSunset.second, base::TimeGM(2015, 6, 23, 5, 17, 0)), ());

  sunriseSunset = CalculateSunriseSunsetTime(base::TimeGM(2015, 6, 23, 5, 10, 0), lat, lon);
  TEST(TimesEqual(sunriseSunset.first, base::TimeGM(2015, 6, 22, 15, 51, 0)), ());
  TEST(TimesEqual(sunriseSunset.second, base::TimeGM(2015, 6, 23, 5, 17, 0)), ());
}

UNIT_TEST(SunriseSunsetAlgorithm_Honolulu_December)
{
  // Honolulu (utc -10). For date 2015/12/23:
  // Sunrise utc time: 2015/12/23,17:06
  // Sunset utc time: 2015/12/24,3:56
  double const lat = 21.307431;
  double const lon = -157.848568;

  pair<time_t, time_t> sunriseSunset = CalculateSunriseSunsetTime(2015, 12, 23, lat, lon);
  TEST(TimesEqual(sunriseSunset.first, base::TimeGM(2015, 12, 23, 17, 6, 0)), ());
  TEST(TimesEqual(sunriseSunset.second, base::TimeGM(2015, 12, 24, 3, 56, 0)), ());
}

UNIT_TEST(SunriseSunsetAlgorithm_Melbourne_Ferbuary)
{
  // Melbourne (utc +11). For date 2015/2/12:
  // Sunrise utc time: 2015/2/11,19:46
  // Sunset utc time: 2015/2/12,9:24
  double const lat = -37.829188;
  double const lon = 144.957976;

  pair<time_t, time_t> sunriseSunset = CalculateSunriseSunsetTime(2015, 2, 12, lat, lon);
  TEST(TimesEqual(sunriseSunset.first, base::TimeGM(2015, 2, 11, 19, 46, 0)), ());
  TEST(TimesEqual(sunriseSunset.second, base::TimeGM(2015, 2, 12, 9, 24, 0)), ());
}

UNIT_TEST(SunriseSunsetAlgorithm_Melbourne_NewYear)
{
  // Melbourne (utc +11). For date 2016/1/1:
  // Sunrise utc time: 2015/12/31,19:02
  // Sunset utc time: 2016/1/1,9:46
  double const lat = -37.829188;
  double const lon = 144.957976;

  pair<time_t, time_t> sunriseSunset = CalculateSunriseSunsetTime(2016, 1, 1, lat, lon);
  TEST(TimesEqual(sunriseSunset.first, base::TimeGM(2015, 12, 31, 19, 2, 0)), ());
  TEST(TimesEqual(sunriseSunset.second, base::TimeGM(2016, 1, 1, 9, 46, 0)), ());
}

UNIT_TEST(SunriseSunsetAlgorithm_GetDayTime_Melbourne_August)
{
  // Melbourne (utc +11), 2015/8/12
  // prev sunset utc 2015/8/11,7:41
  // sunrise utc 2015/8/11,21:10
  // sunset utc 2015/8/12,7:42
  // next sunrise utc 2015/8/12,21:09
  double const lat = -37.829188;
  double const lon = 144.957976;

  // before sunrise but after prev sunset
  pair<DayTimeType, time_t> dayTime = GetDayTime(base::TimeGM(2015, 8, 11, 15, 0, 0), lat, lon);
  TEST_EQUAL(dayTime.first, DayTimeType::NightTime, ());
  TEST(TimesEqual(dayTime.second, base::TimeGM(2015, 8, 11, 21, 10, 0)), ());

  // between sunrise and sunset
  dayTime = GetDayTime(base::TimeGM(2015, 8, 11, 21, 20, 0), lat, lon);
  TEST_EQUAL(dayTime.first, DayTimeType::DayTime, ());
  TEST(TimesEqual(dayTime.second, base::TimeGM(2015, 8, 12, 7, 42, 0)), ());

  // after sunset but before next sunrise
  dayTime = GetDayTime(base::TimeGM(2015, 8, 12, 10, 0, 0), lat, lon);
  TEST_EQUAL(dayTime.first, DayTimeType::NightTime, ());
  TEST(TimesEqual(dayTime.second, base::TimeGM(2015, 8, 12, 21, 9, 0)), ());
}

UNIT_TEST(SunriseSunsetAlgorithm_Wellington_October)
{
  // Melbourne (utc +13). For date 2015/10/20:
  // Sunrise utc time: 2015/10/19,17:26
  // Sunset utc time: 2015/10/20,6:47
  double const lat = -41.287481;
  double const lon = 174.774189;

  pair<time_t, time_t> sunriseSunset = CalculateSunriseSunsetTime(2015, 10, 20, lat, lon);
  TEST(TimesEqual(sunriseSunset.first, base::TimeGM(2015, 10, 19, 17, 26, 0)), ());
  TEST(TimesEqual(sunriseSunset.second, base::TimeGM(2015, 10, 20, 6, 47, 0)), ());
}

UNIT_TEST(SunriseSunsetAlgorithm_BuenosAires_March)
{
  // Buenos Aires (utc -3). For date 2015/3/8:
  // Sunrise utc time: 2015/3/8,9:48
  // Sunset utc time: 2015/3/8,22:23
  double const lat = -34.607639;
  double const lon = -58.438095;

  pair<time_t, time_t> sunriseSunset = CalculateSunriseSunsetTime(2015, 3, 8, lat, lon);
  TEST(TimesEqual(sunriseSunset.first, base::TimeGM(2015, 3, 8, 9, 48, 0)), ());
  TEST(TimesEqual(sunriseSunset.second, base::TimeGM(2015, 3, 8, 22, 23, 0)), ());

  auto dayTime = GetDayTime(base::TimeGM(2015, 3, 8, 9, 0, 0), lat, lon);
  TEST_EQUAL(dayTime.first, DayTimeType::NightTime, ());
  TEST_EQUAL(dayTime.second, sunriseSunset.first, ());

  dayTime = GetDayTime(base::TimeGM(2015, 3, 8, 21, 0, 0), lat, lon);
  TEST_EQUAL(dayTime.first, DayTimeType::DayTime, ());
  TEST_EQUAL(dayTime.second, sunriseSunset.second, ());

  dayTime = GetDayTime(base::TimeGM(2015, 3, 8, 22, 55, 0), lat, lon);
  TEST_EQUAL(dayTime.first, DayTimeType::NightTime, ());
}

UNIT_TEST(SunriseSunsetAlgorithm_Seattle_May)
{
  // Seattle (utc -8). For date 2015/5/9:
  // Sunrise utc time: 2015/5/9,12:41
  // Sunset utc time: 2015/5/10,3:32
  double const lat = 47.597482;
  double const lon = -122.334590;

  pair<time_t, time_t> sunriseSunset = CalculateSunriseSunsetTime(2015, 5, 9, lat, lon);
  TEST(TimesEqual(sunriseSunset.first, base::TimeGM(2015, 5, 9, 12, 41, 0)), ());
  TEST(TimesEqual(sunriseSunset.second, base::TimeGM(2015, 5, 10, 3, 32, 0)), ());
}

UNIT_TEST(SunriseSunsetAlgorithm_Reykjavik_May)
{
  // Reykjavik (utc 0). For date 2015/5/9:
  // Sunrise utc time: 2015/5/9,4:34
  // Sunset utc time: 2015/5/9,22:15
  double const lat = 64.120467;
  double const lon = -21.809448;

  pair<time_t, time_t> sunriseSunset = CalculateSunriseSunsetTime(2015, 5, 9, lat, lon);
  TEST(TimesEqual(sunriseSunset.first, base::TimeGM(2015, 5, 9, 4, 34, 0)), ());
  TEST(TimesEqual(sunriseSunset.second, base::TimeGM(2015, 5, 9, 22, 15, 0)), ());
}

UNIT_TEST(SunriseSunsetAlgorithm_Reykjavik_June)
{
  // Reykjavik (utc 0). For date 2015/6/22:
  // Sunrise utc time: 2015/6/22,2:56
  // Sunset utc time: 2015/6/23,0:04
  double const lat = 64.120467;
  double const lon = -21.809448;

  pair<time_t, time_t> sunriseSunset = CalculateSunriseSunsetTime(2015, 6, 22, lat, lon);
  TEST(TimesEqual(sunriseSunset.first, base::TimeGM(2015, 6, 22, 2, 56, 0)), ());
  TEST(TimesEqual(sunriseSunset.second, base::TimeGM(2015, 6, 23, 0, 4, 0)), ());
}

UNIT_TEST(SunriseSunsetAlgorithm_CapeTown_November)
{
  // Cape Town (utc +2). For date 2015/11/11:
  // Sunrise utc time: 2015/11/11,3:38
  // Sunset utc time: 2015/11/11,17:24
  double const lat = -33.929573;
  double const lon = 18.428439;

  pair<time_t, time_t> sunriseSunset = CalculateSunriseSunsetTime(2015, 11, 11, lat, lon);
  TEST(TimesEqual(sunriseSunset.first, base::TimeGM(2015, 11, 11, 3, 38, 0)), ());
  TEST(TimesEqual(sunriseSunset.second, base::TimeGM(2015, 11, 11, 17, 24, 0)), ());
}

UNIT_TEST(SunriseSunsetAlgorithm_CapeTown_March)
{
  // Cape Town (utc +2). For date 2015/3/1:
  // Sunrise utc time: 2015/3/1,4:34
  // Sunset utc time: 2015/3/1,17:24
  double const lat = -33.929573;
  double const lon = 18.428439;

  pair<time_t, time_t> sunriseSunset = CalculateSunriseSunsetTime(2015, 3, 1, lat, lon);
  TEST(TimesEqual(sunriseSunset.first, base::TimeGM(2015, 3, 1, 4, 34, 0)), ());
  TEST(TimesEqual(sunriseSunset.second, base::TimeGM(2015, 3, 1, 17, 24, 0)), ());
}

UNIT_TEST(SunriseSunsetAlgorithm_Tiksi_March)
{
  // Russia, Siberia, Tiksi. For date 2015/3/1:
  // Sunrise utc time: 2015/2/28,23:04
  // Sunset utc time: 2015/3/1,8:12
  double const lat = 71.635604;
  double const lon = 128.882922;

  pair<time_t, time_t> sunriseSunset = CalculateSunriseSunsetTime(2015, 3, 1, lat, lon);
  TEST(TimesEqual(sunriseSunset.first, base::TimeGM(2015, 2, 28, 23, 4, 0)), ());
  TEST(TimesEqual(sunriseSunset.second, base::TimeGM(2015, 3, 1, 8, 12, 0)), ());
}

UNIT_TEST(SunriseSunsetAlgorithm_Tiksi_July)
{
  // Russia, Siberia, Tiksi. For date 2015/7/1:
  // Polar day
  double const lat = 71.635604;
  double const lon = 128.882922;

  pair<time_t, time_t> sunriseSunset = CalculateSunriseSunsetTime(2015, 7, 1, lat, lon);
  TEST(TimesEqual(sunriseSunset.first, base::TimeGM(2015, 7, 1, 0, 0, 0)), ());
  TEST_EQUAL(sunriseSunset.second, (sunriseSunset.first + kOneDaySeconds), ());

  auto dayTime = GetDayTime(base::TimeGM(2015, 7, 1, 0, 0, 0), lat, lon);
  TEST_EQUAL(dayTime.first, DayTimeType::PolarDay, ());
}

UNIT_TEST(SunriseSunsetAlgorithm_Tiksi_December)
{
  // Russia, Siberia, Tiksi. For date 2015/12/1:
  // Polar night
  double const lat = 71.635604;
  double const lon = 128.882922;

  pair<time_t, time_t> sunriseSunset = CalculateSunriseSunsetTime(2015, 12, 1, lat, lon);
  TEST(TimesEqual(sunriseSunset.first, base::TimeGM(2015, 12, 1, 0, 0, 0)), ());
  TEST(TimesEqual(sunriseSunset.second, base::TimeGM(2015, 12, 1, 0, 0, 0)), ());
  TEST_EQUAL(sunriseSunset.first, sunriseSunset.second, ());

  pair<DayTimeType, time_t> dayTime = GetDayTime(base::TimeGM(2015, 12, 1, 0, 0, 0), lat, lon);
  TEST_EQUAL(DayTimeType::PolarNight, dayTime.first, ());
}

UNIT_TEST(SunriseSunsetAlgorithm_Norilsk_NewYear)
{
  // Norilsk. For date 2016/1/1:
  // Polar night
  double const lat = 69.350000;
  double const lon = 88.180000;

  pair<time_t, time_t> sunriseSunset = CalculateSunriseSunsetTime(2016, 1, 1, lat, lon);
  TEST(TimesEqual(sunriseSunset.first, base::TimeGM(2016, 1, 1, 0, 0, 0)), ());
  TEST_EQUAL(sunriseSunset.first, sunriseSunset.second, ());

  auto dayTime = GetDayTime(base::TimeGM(2016, 1, 1, 0, 0, 0), lat, lon);
  TEST_EQUAL(DayTimeType::PolarNight, dayTime.first, ());
}

UNIT_TEST(SunriseSunsetAlgorithm_Norilsk_August)
{
  // Norilsk. For date 2015/6/22:
  // Polar day
  double const lat = 69.350000;
  double const lon = 88.180000;

  pair<time_t, time_t> sunriseSunset = CalculateSunriseSunsetTime(2015, 6, 22, lat, lon);
  TEST(TimesEqual(sunriseSunset.first, base::TimeGM(2015, 6, 22, 0, 0, 0)), ());
  TEST_EQUAL(sunriseSunset.second, (sunriseSunset.first + kOneDaySeconds), ());

  pair<DayTimeType, time_t> dayTime = GetDayTime(base::TimeGM(2015, 6, 22, 0, 0, 0), lat, lon);
  TEST_EQUAL(DayTimeType::PolarDay, dayTime.first, ());
}

UNIT_TEST(SunriseSunsetAlgorithm_Tokio_September)
{
  // Tokio. For date 2015/9/12:
  // Sunrise utc time: 2015/9/11,20:22
  // Sunset utc time: 2015/9/12,8:56
  double const lat = 35.715791;
  double const lon = 139.743945;

  pair<time_t, time_t> sunriseSunset = CalculateSunriseSunsetTime(2015, 9, 12, lat, lon);
  TEST(TimesEqual(sunriseSunset.first, base::TimeGM(2015, 9, 11, 20, 22, 0)), ());
  TEST(TimesEqual(sunriseSunset.second, base::TimeGM(2015, 9, 12, 8, 56, 0)), ());
}

UNIT_TEST(SunriseSunsetAlgorithm_Kabul_March)
{
  // Kabul. For date 2015/3/20:
  // Sunrise utc time: 2015/3/20,01:29
  // Sunset utc time: 2015/3/20,13:35
  double const lat = 34.552312;
  double const lon = 69.170520;

  pair<time_t, time_t> sunriseSunset = CalculateSunriseSunsetTime(2015, 3, 20, lat, lon);
  TEST(TimesEqual(sunriseSunset.first, base::TimeGM(2015, 3, 20, 1, 29, 0)), ());
  TEST(TimesEqual(sunriseSunset.second, base::TimeGM(2015, 3, 20, 13, 35, 0)), ());

  auto dayTime = GetDayTime(base::TimeGM(2015, 3, 20, 1, 15, 0), lat, lon);
  TEST_EQUAL(DayTimeType::NightTime, dayTime.first, ());

  dayTime = GetDayTime(base::TimeGM(2015, 3, 20, 1, 45, 0), lat, lon);
  TEST_EQUAL(DayTimeType::DayTime, dayTime.first, ());

  dayTime = GetDayTime(base::TimeGM(2015, 3, 20, 13, 15, 0), lat, lon);
  TEST_EQUAL(DayTimeType::DayTime, dayTime.first, ());

  dayTime = GetDayTime(base::TimeGM(2015, 3, 20, 13, 45, 0), lat, lon);
  TEST_EQUAL(DayTimeType::NightTime, dayTime.first, ());
}

UNIT_TEST(SunriseSunsetAlgorithm_Areora_January)
{
  // Areora. For date 2016/1/1:
  // Sunrise utc time: 2016/1/1,15:57
  // Sunset utc time: 2016/1/2,5:16
  double const lat = -20.003751;
  double const lon = -158.114640;

  pair<time_t, time_t> sunriseSunset = CalculateSunriseSunsetTime(2016, 1, 1, lat, lon);
  TEST(TimesEqual(sunriseSunset.first, base::TimeGM(2016, 1, 1, 15, 57, 0)), ());
  TEST(TimesEqual(sunriseSunset.second, base::TimeGM(2016, 1, 2, 5, 16, 0)), ());

  auto dayTime = GetDayTime(base::TimeGM(2016, 1, 1, 15, 15, 0), lat, lon);
  TEST_EQUAL(dayTime.first, DayTimeType::NightTime, ());
  TEST_EQUAL(dayTime.second, sunriseSunset.first, ());

  dayTime = GetDayTime(base::TimeGM(2016, 1, 1, 16, 10, 0), lat, lon);
  TEST_EQUAL(dayTime.first, DayTimeType::DayTime, ());
  TEST_EQUAL(dayTime.second, sunriseSunset.second, ());

  dayTime = GetDayTime(base::TimeGM(2016, 1, 2, 3, 15, 0), lat, lon);
  TEST_EQUAL(dayTime.first, DayTimeType::DayTime, ());
  TEST_EQUAL(dayTime.second, sunriseSunset.second, ());

  dayTime = GetDayTime(base::TimeGM(2016, 1, 2, 5, 35, 0), lat, lon);
  TEST_EQUAL(dayTime.first, DayTimeType::NightTime, ());
}

UNIT_TEST(SunriseSunsetAlgorithm_Lorino_February)
{
  // Lorino. For date 2016/2/2:
  // Sunrise utc time: 2016/2/2,20:17
  // Sunset utc time: 2016/2/3,03:10
  double const lat = 65.499550;
  double const lon = -171.715726;

  pair<time_t, time_t> sunriseSunset = CalculateSunriseSunsetTime(2016, 2, 2, lat, lon);
  TEST(TimesEqual(sunriseSunset.first, base::TimeGM(2016, 2, 2, 20, 17, 0)), ());
  TEST(TimesEqual(sunriseSunset.second, base::TimeGM(2016, 2, 3, 3, 10, 0)), ());

  auto dayTime = GetDayTime(base::TimeGM(2016, 2, 2, 19, 30, 0), lat, lon);
  TEST_EQUAL(dayTime.first, DayTimeType::NightTime, ());
  TEST_EQUAL(dayTime.second, sunriseSunset.first, ());

  dayTime = GetDayTime(base::TimeGM(2016, 2, 2, 20, 30, 0), lat, lon);
  TEST_EQUAL(dayTime.first, DayTimeType::DayTime, ());
  TEST_EQUAL(dayTime.second, sunriseSunset.second, ());

  dayTime = GetDayTime(base::TimeGM(2016, 2, 3, 3, 0, 0), lat, lon);
  TEST_EQUAL(dayTime.first, DayTimeType::DayTime, ());
  TEST_EQUAL(dayTime.second, sunriseSunset.second, ());

  dayTime = GetDayTime(base::TimeGM(2016, 2, 3, 10, 0, 0), lat, lon);
  TEST_EQUAL(dayTime.first, DayTimeType::NightTime, ());
}

UNIT_TEST(SunriseSunsetAlgorithm_Anadyr_December)
{
  // Anadyr. For date 2015/12/25:
  // Sunrise utc time: 2015/12/24,22:17
  // Sunset utc time: 2015/12/25,02:03
  double const lat = 64.722245;
  double const lon = 177.499123;

  pair<time_t, time_t> sunriseSunset = CalculateSunriseSunsetTime(2015, 12, 25, lat, lon);
  TEST(TimesEqual(sunriseSunset.first, base::TimeGM(2015, 12, 24, 22, 17, 0)), ());
  TEST(TimesEqual(sunriseSunset.second, base::TimeGM(2015, 12, 25, 2, 3, 0)), ());

  auto dayTime = GetDayTime(base::TimeGM(2015, 12, 24, 19, 0, 0), lat, lon);
  TEST_EQUAL(dayTime.first, DayTimeType::NightTime, ());
  TEST_EQUAL(dayTime.second, sunriseSunset.first, ());

  dayTime = GetDayTime(base::TimeGM(2015, 12, 24, 22, 30, 0), lat, lon);
  TEST_EQUAL(dayTime.first, DayTimeType::DayTime, ());
  TEST_EQUAL(dayTime.second, sunriseSunset.second, ());

  dayTime = GetDayTime(base::TimeGM(2015, 12, 25, 1, 55, 0), lat, lon);
  TEST_EQUAL(dayTime.first, DayTimeType::DayTime, ());
  TEST_EQUAL(dayTime.second, sunriseSunset.second, ());

  dayTime = GetDayTime(base::TimeGM(2015, 12, 25, 2, 30, 0), lat, lon);
  TEST_EQUAL(dayTime.first, DayTimeType::NightTime, ());
}

UNIT_TEST(SunriseSunsetAlgorithm_Nikolski_December)
{
  // Nikolski. For date 2015/12/25:
  // Sunrise utc time: 2015/12/25,19:29
  // Sunset utc time: 2015/12/26,3:04
  double const lat = 52.933280;
  double const lon = -168.864102;

  pair<time_t, time_t> sunriseSunset = CalculateSunriseSunsetTime(2015, 12, 25, lat, lon);
  TEST(TimesEqual(sunriseSunset.first, base::TimeGM(2015, 12, 25, 19, 29, 0)), ());
  TEST(TimesEqual(sunriseSunset.second, base::TimeGM(2015, 12, 26, 3, 4, 0)), ());

  auto dayTime = GetDayTime(base::TimeGM(2015, 12, 25, 19, 0, 0), lat, lon);
  TEST_EQUAL(dayTime.first, DayTimeType::NightTime, ());
  TEST_EQUAL(dayTime.second, sunriseSunset.first, ());

  dayTime = GetDayTime(base::TimeGM(2015, 12, 25, 19, 35, 0), lat, lon);
  TEST_EQUAL(dayTime.first, DayTimeType::DayTime, ());
  TEST_EQUAL(dayTime.second, sunriseSunset.second, ());

  dayTime = GetDayTime(base::TimeGM(2015, 12, 26, 3, 0, 0), lat, lon);
  TEST_EQUAL(dayTime.first, DayTimeType::DayTime, ());
  TEST_EQUAL(dayTime.second, sunriseSunset.second, ());

  dayTime = GetDayTime(base::TimeGM(2015, 12, 26, 3, 10, 0), lat, lon);
  TEST_EQUAL(dayTime.first, DayTimeType::NightTime, ());
}

UNIT_TEST(SunriseSunsetAlgorithm_Kiribati_July)
{
  // Kiribati. For date 2015/7/1:
  // Sunrise utc time: 2015/7/1,16:28
  // Sunset utc time: 2015/7/2,4:41
  double const lat = 1.928797;
  double const lon = -157.494678;

  pair<time_t, time_t> sunriseSunset = CalculateSunriseSunsetTime(2015, 7, 1, lat, lon);
  TEST(TimesEqual(sunriseSunset.first, base::TimeGM(2015, 7, 1, 16, 28, 0)), ());
  TEST(TimesEqual(sunriseSunset.second, base::TimeGM(2015, 7, 2, 4, 41, 0)), ());

  auto dayTime = GetDayTime(base::TimeGM(2015, 7, 1, 16, 10, 0), lat, lon);
  TEST_EQUAL(dayTime.first, DayTimeType::NightTime, ());
  TEST_EQUAL(dayTime.second, sunriseSunset.first, ());

  dayTime = GetDayTime(base::TimeGM(2015, 7, 1, 16, 35, 0), lat, lon);
  TEST_EQUAL(dayTime.first, DayTimeType::DayTime, ());
  TEST_EQUAL(dayTime.second, sunriseSunset.second, ());

  dayTime = GetDayTime(base::TimeGM(2015, 7, 2, 4, 0, 0), lat, lon);
  TEST_EQUAL(dayTime.first, DayTimeType::DayTime, ());
  TEST_EQUAL(dayTime.second, sunriseSunset.second, ());

  dayTime = GetDayTime(base::TimeGM(2015, 7, 2, 4, 50, 0), lat, lon);
  TEST_EQUAL(dayTime.first, DayTimeType::NightTime, ());
}

UNIT_TEST(SunriseSunsetAlgorithm_Kiribati_July_2)
{
  // Kiribati. For date 2015/7/1:
  // Sunrise utc time: 2015/7/1,16:28
  // Sunset utc time: 2015/7/2,4:41
  // Next sunrise utc time: 2015/7/2,16:28
  // Next sunset utc time: 2015/7/3,4:42
  double const lat = 1.928797;
  double const lon = -157.494678;

  // before sunrise
  pair<time_t, time_t> sunriseSunset = CalculateSunriseSunsetTime(base::TimeGM(2015, 7, 1, 16, 10, 0), lat, lon);
  TEST(TimesEqual(sunriseSunset.first, base::TimeGM(2015, 7, 1, 16, 28, 0)), ());
  TEST(TimesEqual(sunriseSunset.second, base::TimeGM(2015, 7, 2, 4, 41, 0)), ());

  // between sunrise sunset
  sunriseSunset = CalculateSunriseSunsetTime(base::TimeGM(2015, 7, 2, 3, 30, 0), lat, lon);
  TEST(TimesEqual(sunriseSunset.first, base::TimeGM(2015, 7, 1, 16, 28, 0)), ());
  TEST(TimesEqual(sunriseSunset.second, base::TimeGM(2015, 7, 2, 4, 41, 0)), ());

  // afer sunset but before next sunrise
  sunriseSunset = CalculateSunriseSunsetTime(base::TimeGM(2015, 7, 2, 7, 0, 0), lat, lon);
  TEST(TimesEqual(sunriseSunset.first, base::TimeGM(2015, 7, 2, 16, 28, 0)), ());
  TEST(TimesEqual(sunriseSunset.second, base::TimeGM(2015, 7, 3, 4, 42, 0)), ());

  // afer sunset but before next sunrise
  sunriseSunset = CalculateSunriseSunsetTime(base::TimeGM(2015, 7, 2, 16, 10, 0), lat, lon);
  TEST(TimesEqual(sunriseSunset.first, base::TimeGM(2015, 7, 2, 16, 28, 0)), ());
  TEST(TimesEqual(sunriseSunset.second, base::TimeGM(2015, 7, 3, 4, 42, 0)), ());
}

UNIT_TEST(SunriseSunsetAlgorithm_London_July)
{
  // London. For date 2015/7/1:
  // Sunrise utc time: 2015/7/1,3:47
  // Sunset utc time: 2015/7/1,20:21
  double const lat = 51.500000;
  double const lon = 0.120000;

  pair<time_t, time_t> sunriseSunset = CalculateSunriseSunsetTime(2015, 7, 1, lat, lon);
  TEST(TimesEqual(sunriseSunset.first, base::TimeGM(2015, 7, 1, 3, 47, 0)), ());
  TEST(TimesEqual(sunriseSunset.second, base::TimeGM(2015, 7, 1, 20, 21, 0)), ());

  auto dayTime = GetDayTime(base::TimeGM(2015, 7, 1, 2, 50, 0), lat, lon);
  TEST_EQUAL(dayTime.first, DayTimeType::NightTime, ());
  TEST_EQUAL(dayTime.second, sunriseSunset.first, ());

  dayTime = GetDayTime(base::TimeGM(2015, 7, 1, 16, 20, 0), lat, lon);
  TEST_EQUAL(dayTime.first, DayTimeType::DayTime, ());
  TEST_EQUAL(dayTime.second, sunriseSunset.second, ());

  dayTime = GetDayTime(base::TimeGM(2015, 7, 1, 20, 10, 0), lat, lon);
  TEST_EQUAL(dayTime.first, DayTimeType::DayTime, ());
  TEST_EQUAL(dayTime.second, sunriseSunset.second, ());

  dayTime = GetDayTime(base::TimeGM(2015, 7, 1, 21, 15, 0), lat, lon);
  TEST_EQUAL(dayTime.first, DayTimeType::NightTime, ());
}

UNIT_TEST(NextDay_Test)
{
  int y = 2008, m = 2, d = 28;
  NextDay(y, m, d);
  TEST_EQUAL(2008, y, ());
  TEST_EQUAL(2, m, ());
  TEST_EQUAL(29, d, ());

  y = 2008, m = 2, d = 29;
  NextDay(y, m, d);
  TEST_EQUAL(2008, y, ());
  TEST_EQUAL(3, m, ());
  TEST_EQUAL(1, d, ());

  y = 2008, m = 12, d = 31;
  NextDay(y, m, d);
  TEST_EQUAL(2009, y, ());
  TEST_EQUAL(1, m, ());
  TEST_EQUAL(1, d, ());

  y = 2008, m = 3, d = 5;
  NextDay(y, m, d);
  TEST_EQUAL(2008, y, ());
  TEST_EQUAL(3, m, ());
  TEST_EQUAL(6, d, ());
}

UNIT_TEST(PrevDay_Test)
{
  int y = 2008, m = 3, d = 1;
  PrevDay(y, m, d);
  TEST_EQUAL(2008, y, ());
  TEST_EQUAL(2, m, ());
  TEST_EQUAL(29, d, ());

  y = 2008, m = 2, d = 29;
  PrevDay(y, m, d);
  TEST_EQUAL(2008, y, ());
  TEST_EQUAL(2, m, ());
  TEST_EQUAL(28, d, ());

  y = 2009, m = 1, d = 1;
  PrevDay(y, m, d);
  TEST_EQUAL(2008, y, ());
  TEST_EQUAL(12, m, ());
  TEST_EQUAL(31, d, ());

  y = 2008, m = 3, d = 5;
  PrevDay(y, m, d);
  TEST_EQUAL(2008, y, ());
  TEST_EQUAL(3, m, ());
  TEST_EQUAL(4, d, ());
}
