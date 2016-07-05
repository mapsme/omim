#pragma once

#include "std/chrono.hpp"
#include "std/string.hpp"
#include "std/vector.hpp"

class BookingCollector
{
public:
  struct Data
  {
    system_clock::time_point m_timestampBegin;
    system_clock::time_point m_timestampEnd;
    vector<string> m_hotelIds;
  };

  BookingCollector() = default;

  void Serialize();
  void Deserialize();

  void TryToBookHotel(string const & hotelId);

  Data CollectData();
  void RestoreData(Data const & data);

private:
  Data m_data;
};
