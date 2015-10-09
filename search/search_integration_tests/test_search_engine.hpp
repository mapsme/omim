#pragma once

#include "indexer/index.hpp"

#include "geometry/rect2d.hpp"

#include "search/search_engine.hpp"

#include "std/string.hpp"
#include "std/weak_ptr.hpp"

class Platform;

namespace search
{
class SearchParams;
}

namespace storage
{
class CountryInfoGetter;
}

class TestSearchEngine : public Index
{
public:
  TestSearchEngine(string const & locale);
  TestSearchEngine(string const & locale, unique_ptr<storage::CountryInfoGetter> && infoGetter);
  ~TestSearchEngine();

  weak_ptr<search::QueryHandle> Search(search::SearchParams const & params,
                                       m2::RectD const & viewport);

private:
  Platform & m_platform;
  unique_ptr<storage::CountryInfoGetter> m_infoGetter;
  search::Engine m_engine;
};
