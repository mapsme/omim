#include "search/search_tests_support/test_search_engine.hpp"

#include "indexer/categories_holder.hpp"

#include "platform/platform.hpp"

namespace search
{
namespace tests_support
{
TestSearchEngine::TestSearchEngine(std::unique_ptr<storage::CountryInfoGetter> infoGetter,
                                   std::unique_ptr<::search::ProcessorFactory> factory,
                                   Engine::Params const & params)
  : m_platform(GetPlatform())
  , m_infoGetter(move(infoGetter))
  , m_engine(*this, GetDefaultCategories(), *m_infoGetter, move(factory), params)
{
}

TestSearchEngine::TestSearchEngine(std::unique_ptr<::search::ProcessorFactory> factory,
                                   Engine::Params const & params)
  : m_platform(GetPlatform())
  , m_infoGetter(storage::CountryInfoReader::CreateCountryInfoReader(m_platform))
  , m_engine(*this, GetDefaultCategories(), *m_infoGetter, move(factory), params)
{
}

TestSearchEngine::~TestSearchEngine() {}

weak_ptr<::search::ProcessorHandle> TestSearchEngine::Search(::search::SearchParams const & params,
                                                             m2::RectD const & viewport)
{
  return m_engine.Search(params, viewport);
}
}  // namespace tests_support
}  // namespace search
