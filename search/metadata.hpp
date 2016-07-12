#pragma once

#include "std/string.hpp"

namespace search
{
/// Metadata for search results. Considered valid if m_resultType == RESULT_FEATURE.
struct Metadata
{
  /// True if the struct is already assigned or need to be calculated otherwise.
  bool m_isInitialized = false;
  bool m_isSponsoredHotel = false; // Used for hotels.
  string m_stars;
  string m_cuisine;
  string m_operator;
  string m_schedule;
  string m_elevation;
  string m_hotelId;
  string m_hotelRating;
  string m_hotelPriceRate;
};
}  // namespace search
