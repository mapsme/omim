#include "map/booking_collector.hpp"

#include "platform/settings.hpp"

#include "base/gmtime.hpp"
#include "base/stl_helpers.hpp"
#include "base/string_utils.hpp"

#include "std/algorithm.hpp"
#include "std/sstream.hpp"

namespace
{
string const kTimestampFormat = "%Y-%m-%d %H:%M:%S";

string SerializeTimestamp(system_clock::time_point const & timestamp)
{
  int constexpr kBufferSize = 32;
  return strings::FromTimePoint<kBufferSize>(timestamp, kTimestampFormat);
}

system_clock::time_point DeserializeTimestamp(string const & str)
{
  return strings::ToTimePoint(str, kTimestampFormat);
}

string SerializeHotels(vector<string> const & hotels)
{
  return strings::JoinStrings(hotels, ",");
}

vector<string> DeserializeHotels(string const & str)
{
  vector<string> result;
  strings::Tokenize(str, ",", [&result](string const & token)
  {
    result.push_back(token);
  });
  return result;
}
} // namespace

void BookingCollector::Serialize()
{
  settings::Set("BookingTimestampBegin", SerializeTimestamp(m_data.m_timestampBegin));
  settings::Set("BookingTimestampEnd", SerializeTimestamp(m_data.m_timestampEnd));
  settings::Set("BookingHotels", SerializeHotels(m_data.m_hotelIds));
}

void BookingCollector::Deserialize()
{
  string outVal;
  if (settings::Get("BookingTimestampBegin", outVal))
    m_data.m_timestampBegin = DeserializeTimestamp(outVal);

  if (settings::Get("BookingTimestampEnd", outVal))
    m_data.m_timestampEnd = DeserializeTimestamp(outVal);

  if (settings::Get("BookingHotels", outVal))
    m_data.m_hotelIds = DeserializeHotels(outVal);
}

void BookingCollector::TryToBookHotel(string const & hotelId)
{
  if (m_data.m_hotelIds.empty())
  {
    m_data.m_timestampBegin = system_clock::from_time_t(time(nullptr));
    m_data.m_timestampEnd = m_data.m_timestampBegin + minutes(30);
  }
  else
  {
    system_clock::time_point t = system_clock::from_time_t(time(nullptr));
    if (t > m_data.m_timestampEnd)
      m_data.m_timestampEnd = t + minutes(30);
  }

  if (find(m_data.m_hotelIds.begin(), m_data.m_hotelIds.end(), hotelId) == m_data.m_hotelIds.end())
    m_data.m_hotelIds.push_back(hotelId);
}

void BookingCollector::RestoreData(Data const & data)
{
  if (m_data.m_hotelIds.empty())
  {
    m_data.m_timestampBegin = data.m_timestampBegin;
    m_data.m_timestampEnd = data.m_timestampEnd;
    m_data.m_hotelIds = data.m_hotelIds;
  }
  else
  {
    m_data.m_timestampBegin = min(m_data.m_timestampBegin, data.m_timestampBegin);
    m_data.m_timestampEnd = max(m_data.m_timestampEnd, data.m_timestampEnd);

    // Paste hotels and remove duplicates.
    auto & v = m_data.m_hotelIds;
    v.insert(v.end(), data.m_hotelIds.begin(), data.m_hotelIds.end());
    my::SortUnique(v);
  }
}

BookingCollector::Data BookingCollector::CollectData()
{
  BookingCollector::Data data;
  swap(m_data, data);
  return data;
}
