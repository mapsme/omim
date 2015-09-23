#pragma once

#include "indexer/index.hpp"

#include "geometry/rect2d.hpp"

#include "search/search_engine.hpp"

#include "storage/country_info_getter.hpp"

#include "std/string.hpp"

class Platform;

namespace search
{
class SearchParams;
}

class TestSearchEngine : public Index
{
public:
  TestSearchEngine(string const & locale);

  bool Search(search::SearchParams const & params, m2::RectD const & viewport);

private:
  Platform & m_platform;
  storage::CountryInfoGetter m_infoGetter;
  search::Engine m_engine;
};
