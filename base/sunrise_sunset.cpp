#include "base/sunrise_sunset.hpp"

#include "base/assert.hpp"
#include "base/exception.hpp"
#include "base/math.hpp"
#include "base/timer.hpp"
#include "base/timegm.hpp"
#include "base/logging.hpp"

namespace
{

// Sun's zenith for sunrise/sunset
//   offical      = 90 degrees 50'
//   civil        = 96 degrees
//   nautical     = 102 degrees
//   astronomical = 108 degrees
double constexpr kZenith = 90 + 50. / 60.; // 90 degrees 50'

time_t constexpr kOneDaySeconds = 24 * 60 * 60;

inline double NormalizeAngle(double a)
{
  double res = fmod(a, 360.);
  if (res < 0)
    res += 360.;
  return res;
}

inline int DaysOfMonth(int year, int month)
{
  ASSERT_GREATER_OR_EQUAL(month, 1, ());
  ASSERT_LESS_OR_EQUAL(month, 12, ());

  int const february = base::IsLeapYear(year) ? 29 : 28;
  int const daysPerMonth[12] = { 31, february, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
  return daysPerMonth[month - 1];
}

enum class DayEventType
{
  Sunrise,
  Sunset,
  PolarDay,
  PolarNight
};

// Main work-horse function which calculates sunrise/sunset in a specified date in a specified location.
// This function was taken from source http://williams.best.vwh.net/sunrise_sunset_algorithm.htm.
// Notation is kept to have source close to source.
// Original article is // http://babel.hathitrust.org/cgi/pt?id=uiug.30112059294311;view=1up;seq=25
pair<DayEventType, time_t> CalculateDayEventTime(int year, int month, int day,
                                                 double latitude, double longitude,
                                                 bool sunrise)
{
  // 1. first calculate the day of the year

  double const N1 = floor(275. * month / 9.);
  double const N2 = floor((month + 9.) / 12.);
  double const N3 = (1. + floor((year - 4. * floor(year / 4.) + 2.) / 3.));
  double const N = N1 - (N2 * N3) + day - 30.;

  // 2. convert the longitude to hour value and calculate an approximate time

  double const lngHour = longitude / 15;

  double t = 0;
  if (sunrise)
    t = N + ((6 - lngHour) / 24);
  else
    t = N + ((18 - lngHour) / 24);

  // 3. calculate the Sun's mean anomaly

  double const M = (0.9856 * t) - 3.289;

  // 4. calculate the Sun's true longitude

  double L = M + (1.916 * sin(my::DegToRad(M))) + (0.020 * sin(2 * my::DegToRad(M))) + 282.634;
  // NOTE: L potentially needs to be adjusted into the range [0,360) by adding/subtracting 360
  L = NormalizeAngle(L);

  // 5a. calculate the Sun's right ascension

  double RA = my::RadToDeg( atan(0.91764 * tan(my::DegToRad(L))) );
  // NOTE: RA potentially needs to be adjusted into the range [0,360) by adding/subtracting 360
  RA = NormalizeAngle(RA);

  // 5b. right ascension value needs to be in the same quadrant as L

  double const Lquadrant = (floor( L / 90)) * 90;
  double const RAquadrant = (floor(RA / 90)) * 90;
  RA = RA + (Lquadrant - RAquadrant);

  // 5c. right ascension value needs to be converted into hours

  RA = RA / 15;

  // 6. calculate the Sun's declination

  double sinDec = 0.39782 * sin(my::DegToRad(L));
  double cosDec = cos(asin(sinDec));

  // 7a. calculate the Sun's local hour angle

  double cosH = (cos(my::DegToRad(kZenith)) - (sinDec * sin(my::DegToRad(latitude)))) / (cosDec * cos(my::DegToRad(latitude)));

  // if cosH > 1 then sun is never rises on this location on specified date (polar night)
  // if cosH < -1 then sun is never sets on this location on specified date (polar day)
  if (cosH < -1 || cosH > 1)
  {
    int const h = sunrise ? 0 : 23;
    int const m = sunrise ? 0 : 59;
    int const s = sunrise ? 0 : 59;

    return make_pair((cosH < -1) ? DayEventType::PolarDay : DayEventType::PolarNight,
                     base::TimeGM(year, month, day, h, m, s));
  }

  // 7b. finish calculating H and convert into hours

  double H = 0;
  if (sunrise)
    H = 360 - my::RadToDeg(acos(cosH));
  else
    H = my::RadToDeg(acos(cosH));

  H = H / 15;

  // 8. calculate local mean time of rising/setting

  double T = H + RA - (0.06571 * t) - 6.622;

  if (T > 24.)
    T = fmod(T, 24.);
  else if (T < 0)
    T += 24.;

  // 9. adjust back to UTC

  double UT = T - lngHour;

  if (UT > 24.)
  {
    NextDay(year, month, day);
    UT = fmod(UT, 24.0);
  }
  else if (UT < 0)
  {
    PrevDay(year, month, day);
    UT += 24.;
  }

  // UT - is a hour with fractional part of date year/month/day, in range of [0;24)

  int const h = floor(UT); // [0;24)
  int const m = floor((UT - h) * 60); // [0;60)
  int const s = fmod(floor(UT * 60 * 60) /* number of seconds from 0:0 to UT */, 60); // [0;60)

  return make_pair(sunrise ? DayEventType::Sunrise : DayEventType::Sunset,
                   base::TimeGM(year, month, day, h, m, s));
}

} // namespace

void NextDay(int & year, int & month, int & day)
{
  ASSERT_GREATER_OR_EQUAL(month, 1, ());
  ASSERT_LESS_OR_EQUAL(month, 12, ());
  ASSERT_GREATER_OR_EQUAL(day, 1, ());
  ASSERT_LESS_OR_EQUAL(day, DaysOfMonth(year, month), ());

  if (day < DaysOfMonth(year, month))
  {
    ++day;
    return;
  }
  if (month < 12)
  {
    day = 1;
    ++month;
    return;
  }
  day = 1;
  month = 1;
  ++year;
}

void PrevDay(int & year, int & month, int & day)
{
  ASSERT_GREATER_OR_EQUAL(month, 1, ());
  ASSERT_LESS_OR_EQUAL(month, 12, ());
  ASSERT_GREATER_OR_EQUAL(day, 1, ());
  ASSERT_LESS_OR_EQUAL(day, DaysOfMonth(year, month), ());

  if (day > 1)
  {
    --day;
    return;
  }
  if (month > 1)
  {
    --month;
    day = DaysOfMonth(year, month);
    return;
  }
  --year;
  month = 12;
  day = 31;
}

pair<time_t, time_t> CalculateSunriseSunsetTime(int year, int month, int day,
                                                double latitude, double longitude)
{
  ASSERT_GREATER_OR_EQUAL(month, 1, ());
  ASSERT_LESS_OR_EQUAL(month, 12, ());
  ASSERT_GREATER_OR_EQUAL(day, 1, ());
  ASSERT_LESS_OR_EQUAL(day, DaysOfMonth(year, month), ());

  auto sunrise = CalculateDayEventTime(year, month, day, latitude, longitude, true /* sunrise */);
  auto sunset = CalculateDayEventTime(year, month, day, latitude, longitude, false /* sunrise */);

  // Edge cases: polar day and polar night
  if (sunrise.first == DayEventType::PolarDay || sunset.first == DayEventType::PolarDay)
  {
    // Polar day: 24 hours of sun
    time_t sunriseUtc = base::TimeGM(year, month, day, 0, 0, 0);
    time_t sunsetUtc = sunriseUtc + kOneDaySeconds;
    return make_pair(sunriseUtc, sunsetUtc);
  }
  else if (sunrise.first == DayEventType::PolarNight || sunset.first == DayEventType::PolarNight)
  {
    // Polar night: 0 seconds of sun
    time_t sunriseUtc = base::TimeGM(year, month, day, 0, 0, 0);
    time_t sunsetUtc = base::TimeGM(year, month, day, 0, 0, 0);
    return make_pair(sunriseUtc, sunsetUtc);
  }

  ASSERT_LESS(sunrise.second, sunset.second, ());
  return make_pair(sunrise.second, sunset.second);
}

DayTimeType GetDayTime(time_t timeUtc, double latitude, double longitude)
{
  tm const * const t = gmtime(&timeUtc);
  if (nullptr == t)
  {
    LOG(LDEBUG, ("gmtime failed for time", timeUtc));
    MYTHROW(RootException, ("gmtime failed for time", timeUtc));
  }

  int year = t->tm_year + 1900;
  int month = t->tm_mon + 1;
  int day = t->tm_mday;

  auto sunrise = CalculateDayEventTime(year, month, day, latitude, longitude, true /* sunrise */);
  auto sunset = CalculateDayEventTime(year, month, day, latitude, longitude, false /* sunrise */);

  // Edge cases: polar day and polar night
  if (sunrise.first == DayEventType::PolarDay || sunset.first == DayEventType::PolarDay)
    return DayTimeType::PolarDayTime;
  else if (sunrise.first == DayEventType::PolarNight || sunset.first == DayEventType::PolarNight)
    return DayTimeType::PolarNightTime;

  if (timeUtc < sunrise.second)
  {
    PrevDay(year, month, day);
    auto prevSunrise = CalculateDayEventTime(year, month, day, latitude, longitude, true /* sunrise */);
    auto prevSunset = CalculateDayEventTime(year, month, day, latitude, longitude, false /* sunrise */);

    if (timeUtc >= prevSunset.second)
      return DayTimeType::NightTime;
    if (timeUtc < prevSunrise.second)
      return DayTimeType::NightTime;
    return DayTimeType::DayTime;
  }
  else if (timeUtc > sunset.second)
  {
    NextDay(year, month, day);
    auto nextSunrise = CalculateDayEventTime(year, month, day, latitude, longitude, true /* sunrise */);
    auto nextSunset = CalculateDayEventTime(year, month, day, latitude, longitude, false /* sunrise */);

    if (timeUtc < nextSunrise.second)
      return DayTimeType::NightTime;
    if (timeUtc > nextSunset.second)
      return DayTimeType::NightTime;
    return DayTimeType::DayTime;
  }

  return DayTimeType::DayTime;
}

string DebugPrint(DayTimeType type)
{
  switch (type)
  {
  case DayTimeType::DayTime: return "DayTime";
  case DayTimeType::NightTime: return "NightTime";
  case DayTimeType::PolarDayTime: return "PolarDayTime";
  case DayTimeType::PolarNightTime: return "PolarNightTime";
  }
  return string();
}
