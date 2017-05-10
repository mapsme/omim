#pragma once

#include "indexer/index.hpp"

#include "geometry/rect2d.hpp"

#include "search/engine.hpp"

#include <string>
#include <memory>
#include <memory>

class Platform;

namespace storage
{
class CountryInfoGetter;
}

namespace search
{
class SearchParams;

namespace tests_support
{
class TestSearchEngine : public Index
{
public:
  TestSearchEngine(std::unique_ptr<storage::CountryInfoGetter> infoGetter,
                   std::unique_ptr<search::ProcessorFactory> factory, Engine::Params const & params);
  TestSearchEngine(std::unique_ptr<::search::ProcessorFactory> factory, Engine::Params const & params);
  ~TestSearchEngine() override;

  inline void SetLocale(std::string const & locale) { m_engine.SetLocale(locale); }

  std::weak_ptr<search::ProcessorHandle> Search(search::SearchParams const & params,
                                           m2::RectD const & viewport);

  storage::CountryInfoGetter & GetCountryInfoGetter() { return *m_infoGetter; }

private:
  Platform & m_platform;
  std::unique_ptr<storage::CountryInfoGetter> m_infoGetter;
  search::Engine m_engine;
};
}  // namespace tests_support
}  // namespace search
