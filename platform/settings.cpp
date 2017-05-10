#include "platform/settings.hpp"
#include "platform/location.hpp"
#include "platform/platform.hpp"

#include "defines.hpp"

#include "coding/reader_streambuf.hpp"
#include "coding/file_writer.hpp"
#include "coding/file_reader.hpp"

#include "geometry/rect2d.hpp"
#include "geometry/any_rect2d.hpp"

#include "base/logging.hpp"

#include <cmath>
#include <iostream>
#include <sstream>

namespace
{
constexpr char kDelimChar = '=';
}  // namespace

namespace settings
{
char const * kLocationStateMode = "LastLocationStateMode";
char const * kMeasurementUnits = "Units";

StringStorage::StringStorage()
{
  try
  {
    std::string settingsPath = GetPlatform().SettingsPathForFile(SETTINGS_FILE_NAME);
    LOG(LINFO, ("Settings path:", settingsPath));
    ReaderStreamBuf buffer(make_unique<FileReader>(settingsPath));
    std::istream stream(&buffer);

    std::string line;
    while (std::getline(stream, line))
    {
      if (line.empty())
        continue;

      size_t const delimPos = line.find(kDelimChar);
      if (delimPos == std::string::npos)
        continue;

      std::string const key = line.substr(0, delimPos);
      std::string const value = line.substr(delimPos + 1);
      if (!key.empty() && !value.empty())
        m_values[key] = value;
    }
  }
  catch (RootException const & ex)
  {
    LOG(LWARNING, ("Loading settings:", ex.Msg()));
  }
}

void StringStorage::Save() const
{
  try
  {
    FileWriter file(GetPlatform().SettingsPathForFile(SETTINGS_FILE_NAME));
    for (auto const & value : m_values)
    {
      std::string line(value.first);
      line += kDelimChar;
      line += value.second;
      line += '\n';
      file.Write(line.data(), line.size());
    }
  }
  catch (RootException const & ex)
  {
    // Ignore all settings saving exceptions.
    LOG(LWARNING, ("Saving settings:", ex.Msg()));
  }
}

StringStorage & StringStorage::Instance()
{
  static StringStorage inst;
  return inst;
}

void StringStorage::Clear()
{
  std::lock_guard<std::mutex> guard(m_mutex);
  m_values.clear();
  Save();
}

bool StringStorage::GetValue(std::string const & key, std::string & outValue) const
{
  std::lock_guard<std::mutex> guard(m_mutex);

  auto const found = m_values.find(key);
  if (found == m_values.end())
    return false;

  outValue = found->second;
  return true;
}

void StringStorage::SetValue(std::string const & key, std::string && value)
{
  std::lock_guard<std::mutex> guard(m_mutex);

  m_values[key] = move(value);
  Save();
}

void StringStorage::DeleteKeyAndValue(std::string const & key)
{
  std::lock_guard<std::mutex> guard(m_mutex);

  auto const found = m_values.find(key);
  if (found != m_values.end())
  {
    m_values.erase(found);
    Save();
  }
}

////////////////////////////////////////////////////////////////////////////////////////////

template <>
std::string ToString<std::string>(std::string const & str)
{
  return str;
}

template <>
bool FromString<std::string>(std::string const & strIn, std::string & strOut)
{
  strOut = strIn;
  return true;
}

namespace impl
{
template <class T, size_t N>
bool FromStringArray(std::string const & s, T(&arr)[N])
{
  std::istringstream in(s);
  size_t count = 0;
  while (count < N && in >> arr[count])
  {
    if (!std::isfinite(arr[count]))
      return false;
    ++count;
  }

  return (!in.fail() && count == N);
}
}  // namespace impl

template <>
std::string ToString<m2::AnyRectD>(m2::AnyRectD const & rect)
{
  std::ostringstream out;
  out.precision(12);
  m2::PointD glbZero(rect.GlobalZero());
  out << glbZero.x << " " << glbZero.y << " ";
  out << rect.Angle().val() << " ";
  m2::RectD const & r = rect.GetLocalRect();
  out << r.minX() << " " << r.minY() << " " << r.maxX() << " " << r.maxY();
  return out.str();
}

template <>
bool FromString<m2::AnyRectD>(std::string const & str, m2::AnyRectD & rect)
{
  double val[7];
  if (!impl::FromStringArray(str, val))
    return false;

  // Will get an assertion in DEBUG and false return in RELEASE.
  m2::RectD const r(val[3], val[4], val[5], val[6]);
  if (!r.IsValid())
    return false;

  rect = m2::AnyRectD(m2::PointD(val[0], val[1]), ang::AngleD(val[2]), r);
  return true;
}

template <>
std::string ToString<m2::RectD>(m2::RectD const & rect)
{
  std::ostringstream stream;
  stream.precision(12);
  stream << rect.minX() << " " << rect.minY() << " " << rect.maxX() << " " << rect.maxY();
  return stream.str();
}
template <>
bool FromString<m2::RectD>(std::string const & str, m2::RectD & rect)
{
  double val[4];
  if (!impl::FromStringArray(str, val))
    return false;

  // Will get an assertion in DEBUG and false return in RELEASE.
  rect = m2::RectD(val[0], val[1], val[2], val[3]);
  return rect.IsValid();
}

template <>
std::string ToString<bool>(bool const & v)
{
  return v ? "true" : "false";
}

template <>
bool FromString<bool>(std::string const & str, bool & v)
{
  if (str == "true")
    v = true;
  else if (str == "false")
    v = false;
  else
    return false;
  return true;
}

namespace impl
{
template <typename T>
std::string ToStringScalar(T const & v)
{
  std::ostringstream stream;
  stream.precision(12);
  stream << v;
  return stream.str();
}

template <typename T>
bool FromStringScalar(std::string const & str, T & v)
{
  std::istringstream stream(str);
  if (stream)
  {
    stream >> v;
    return !stream.fail();
  }
  else
    return false;
}
}  // namespace impl

template <>
std::string ToString<double>(double const & v)
{
  return impl::ToStringScalar<double>(v);
}

template <>
bool FromString<double>(std::string const & str, double & v)
{
  return impl::FromStringScalar<double>(str, v);
}

template <>
std::string ToString<int32_t>(int32_t const & v)
{
  return impl::ToStringScalar<int32_t>(v);
}

template <>
bool FromString<int32_t>(std::string const & str, int32_t & v)
{
  return impl::FromStringScalar<int32_t>(str, v);
}

template <>
std::string ToString<int64_t>(int64_t const & v)
{
  return impl::ToStringScalar<int64_t>(v);
}

template <>
bool FromString<int64_t>(std::string const & str, int64_t & v)
{
  return impl::FromStringScalar<int64_t>(str, v);
}

template <>
std::string ToString<uint32_t>(uint32_t const & v)
{
  return impl::ToStringScalar<uint32_t>(v);
}

template <>
std::string ToString<uint64_t>(uint64_t const & v)
{
  return impl::ToStringScalar<uint64_t>(v);
}

template <>
bool FromString<uint32_t>(std::string const & str, uint32_t & v)
{
  return impl::FromStringScalar<uint32_t>(str, v);
}

template <>
bool FromString<uint64_t>(std::string const & str, uint64_t & v)
{
  return impl::FromStringScalar<uint64_t>(str, v);
}

namespace impl
{
template <class TPair>
std::string ToStringPair(TPair const & value)
{
  std::ostringstream stream;
  stream.precision(12);
  stream << value.first << " " << value.second;
  return stream.str();
}

template <class TPair>
bool FromStringPair(std::string const & str, TPair & value)
{
  std::istringstream stream(str);
  if (stream)
  {
    stream >> value.first;
    if (stream)
    {
      stream >> value.second;
      return !stream.fail();
    }
  }
  return false;
}
}  // namespace impl

typedef pair<int, int> IPairT;
typedef pair<double, double> DPairT;

template <>
std::string ToString<IPairT>(IPairT const & v)
{
  return impl::ToStringPair(v);
}

template <>
bool FromString<IPairT>(std::string const & s, IPairT & v)
{
  return impl::FromStringPair(s, v);
}

template <>
std::string ToString<DPairT>(DPairT const & v)
{
  return impl::ToStringPair(v);
}

template <>
bool FromString<DPairT>(std::string const & s, DPairT & v)
{
  return impl::FromStringPair(s, v);
}

template <>
std::string ToString<measurement_utils::Units>(measurement_utils::Units const & v)
{
  switch (v)
  {
  // The value "Foot" is left here for compatibility with old settings.ini files.
  case measurement_utils::Units::Imperial: return "Foot";
  case measurement_utils::Units::Metric: return "Metric";
  }
}

template <>
bool FromString<measurement_utils::Units>(std::string const & s, measurement_utils::Units & v)
{
  if (s == "Metric")
    v = measurement_utils::Units::Metric;
  else if (s == "Foot")
    v = measurement_utils::Units::Imperial;
  else
    return false;

  return true;
}

template <>
std::string ToString<location::EMyPositionMode>(location::EMyPositionMode const & v)
{
  switch (v)
  {
  case location::PendingPosition: return "PendingPosition";
  case location::NotFollow: return "NotFollow";
  case location::NotFollowNoPosition: return "NotFollowNoPosition";
  case location::Follow: return "Follow";
  case location::FollowAndRotate: return "FollowAndRotate";
  default: return "Pending";
  }
}

template <>
bool FromString<location::EMyPositionMode>(std::string const & s, location::EMyPositionMode & v)
{
  if (s == "PendingPosition")
    v = location::PendingPosition;
  else if (s == "NotFollow")
    v = location::NotFollow;
  else if (s == "NotFollowNoPosition")
    v = location::NotFollowNoPosition;
  else if (s == "Follow")
    v = location::Follow;
  else if (s == "FollowAndRotate")
    v = location::FollowAndRotate;
  else
    return false;

  return true;
}

bool IsFirstLaunchForDate(int date)
{
  constexpr char const * kFirstLaunchKey = "FirstLaunchOnDate";
  int savedDate;
  if (!Get(kFirstLaunchKey, savedDate) || savedDate < date)
  {
    Set(kFirstLaunchKey, date);
    return true;
  }
  else
    return false;
}
}  // namespace settings
