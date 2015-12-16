#include "base/sunrise_sunset.hpp"

#include "base/assert.hpp"
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

inline double NormalizeHour(double h)
{
  double res = fmod(h, 24.);
  if (res < 0)
    res += 24.;
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
DayEventType CalculateDayEventTime(int year, int month, int day,
                                   double latitude, double longitude,
                                   bool sunrise,
                                   int & hour, int & minute, int & second)
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
    hour = sunrise ? 0 : 23;
    minute = sunrise ? 0 : 59;
    second = sunrise ? 0 : 59;
    return (cosH < -1) ? DayEventType::PolarDay : DayEventType::PolarNight;
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

  // 9. adjust back to UTC

  double UT = T - lngHour;
  // NOTE: UT potentially needs to be adjusted into the range [0,24) by adding/subtracting 24
  UT = NormalizeHour(UT);

  // UT - is a hour with fractional part of date year/month/day, in range of [0;24)

  hour = floor(UT); // [0;24)
  minute = floor((UT - hour) * 60); // [0;60)
  second = fmod(floor(UT * 60 * 60) /* number of seconds from 0:0 to UT */, 60); // [0;60)

  return sunrise ? DayEventType::Sunrise : DayEventType::Sunset;
}

DayEventType CalculateDayEventTime(int year, int month, int day,
                                   double latitude, double longitude,
                                   bool sunrise,
                                   time_t & timestampUtc)
{
  int h, m, s;
  DayEventType const res = CalculateDayEventTime(year, month, day, latitude, longitude, sunrise, h, m, s);
  timestampUtc = base::TimeGM(year, month, day, h, m, s);
  return res;
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

  time_t sunriseUtc, sunsetUtc;
  DayEventType const sunriseRes = CalculateDayEventTime(year, month, day, latitude, longitude, true /* sunrise */, sunriseUtc);
  DayEventType const sunsetRes = CalculateDayEventTime(year, month, day, latitude, longitude, false /* sunrise */, sunsetUtc);

  // Edge cases: polar day and polar night
  if (sunriseRes == DayEventType::PolarDay || sunsetRes == DayEventType::PolarDay)
  {
    // Polar day: 24 hours of sun
    sunriseUtc = base::TimeGM(year, month, day, 0, 0, 0);
    sunsetUtc = sunriseUtc + kOneDaySeconds;
    return make_pair(sunriseUtc, sunsetUtc);
  }
  else if (sunriseRes == DayEventType::PolarNight || sunsetRes == DayEventType::PolarNight)
  {
    // Polar night: 0 seconds of sun
    sunriseUtc = sunsetUtc = base::TimeGM(year, month, day, 0, 0, 0);
    return make_pair(sunriseUtc, sunsetUtc);
  }

  if (sunsetUtc < sunriseUtc)
  {
    // Process line of date changing.
    // Local date fits to two utc dates (current utc date and previous utc date).

    if (longitude > 0)
    {
      PrevDay(year, month, day);
      CalculateDayEventTime(year, month, day, latitude, longitude, true /* sunrise */, sunriseUtc);
    }
    else if (longitude < 0)
    {
      NextDay(year, month, day);
      CalculateDayEventTime(year, month, day, latitude, longitude, false /* sunrise */, sunsetUtc);
    }
  }

  ASSERT_LESS(sunriseUtc, sunsetUtc, ());
  return make_pair(sunriseUtc, sunsetUtc);
}

pair<time_t, time_t> CalculateSunriseSunsetTime(time_t timeUtc, double latitude, double longitude)
{
  tm const * const t = gmtime(&timeUtc);
  if (nullptr == t)
  {
    LOG(LDEBUG, ("gmtime failed for time", timeUtc));
    return make_pair(my::INVALID_TIME_STAMP, my::INVALID_TIME_STAMP);
  }

  int year = t->tm_year + 1900;
  int mon = t->tm_mon + 1;
  int day = t->tm_mday;

  auto sunriseSunsetUtc = CalculateSunriseSunsetTime(year, mon, day, latitude, longitude);

  if (sunriseSunsetUtc.second < timeUtc)
  {
    // (sunrise) (sunset) <time> [sunrise] [sunset] ---> time axis
    // result here is [sunrise][sunset]
    NextDay(year, mon, day);
    sunriseSunsetUtc = CalculateSunriseSunsetTime(year, mon, day, latitude, longitude);
  }
  else if (timeUtc < sunriseSunsetUtc.first)
  {
    // <time> (sunrise) (sunset) ---> time axis
    // but on the change date line it can be:
    //  [sunrise] <time> [sunset] (sunrise) (sunset) ---> time axis
    // then result is [sunrise][sunset]
    // or in regular case:
    //  [sunrise] [sunset] <time> (sunrise) (sunset) ---> time axis
    // then result is (sunrise)(sunset)
    PrevDay(year, mon, day);
    auto prevSunriseSunsetUtc = CalculateSunriseSunsetTime(year, mon, day, latitude, longitude);
    if (timeUtc < prevSunriseSunsetUtc.second)
      sunriseSunsetUtc = prevSunriseSunsetUtc;
  }

  return sunriseSunsetUtc;
}

pair<DayTimeType, time_t>  GetDayTime(time_t timeUtc, double latitude, double longitude)
{
  auto const sunriseSunsetUtc = CalculateSunriseSunsetTime(timeUtc, latitude, longitude);

  if (sunriseSunsetUtc.first == my::INVALID_TIME_STAMP ||
      sunriseSunsetUtc.second == my::INVALID_TIME_STAMP)
  {
    return make_pair(DayTimeType::DayTime, my::INVALID_TIME_STAMP);
  }

  // CalculateSunriseSunsetTime returns only 2 possible cases:
  // <time> (sunrise) (sunset) ---> time axis
  // (sunrise) <time> (sunset) ---> time axis
  // it never returns sunrise and sunset before time

  // Edge cases: polar day and polar night
  if (sunriseSunsetUtc.first == sunriseSunsetUtc.second)
  {
    // Polar night: 0 seconds of sun
    return make_pair(DayTimeType::PolarNight, timeUtc + kOneDaySeconds);
  }
  else if (sunriseSunsetUtc.second == (sunriseSunsetUtc.first + kOneDaySeconds))
  {
    // Polar day: 24 hours of sun
    return make_pair(DayTimeType::PolarDay, timeUtc + kOneDaySeconds);
  }

  if (timeUtc < sunriseSunsetUtc.first)
  {
    // (time) (sunrise) (sunset) ---> time axis
    return make_pair(DayTimeType::NightTime, sunriseSunsetUtc.first);
  }
  else
  {
    // (sunrise) (time) (sunset) ---> time axis
    ASSERT(timeUtc <= sunriseSunsetUtc.second, ());
    return make_pair(DayTimeType::DayTime, sunriseSunsetUtc.second);
  }
}

string DebugPrint(DayTimeType type)
{
  switch (type)
  {
  case DayTimeType::DayTime: return "DayTime";
  case DayTimeType::NightTime: return "NightTime";
  case DayTimeType::PolarDay: return "PolarDay";
  case DayTimeType::PolarNight: return "PolarNight";
  }
  return string();
}
