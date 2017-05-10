#pragma once

#include "search/categories_cache.hpp"
#include "search/locality_finder.hpp"

#include "geometry/point2d.hpp"

#include "base/cancellable.hpp"

#include <cstdint>

class CityFinder
{
public:
  explicit CityFinder(Index const & index)
    : m_unusedCache(m_cancellable), m_finder(index, m_unusedCache)
  {
  }

  std::string GetCityName(m2::PointD const & p, int8_t lang)
  {
    std::string city;
    m_finder.SetLanguage(lang);
    m_finder.GetLocality(p, city);
    return city;
  }

private:
  my::Cancellable m_cancellable;
  search::VillagesCache m_unusedCache;
  search::LocalityFinder m_finder;
};
