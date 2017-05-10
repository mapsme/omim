#include "search/search_integration_tests/helpers.hpp"

#include "search/editor_delegate.hpp"
#include "search/processor_factory.hpp"
#include "search/search_tests_support/test_search_request.hpp"

#include "indexer/classificator_loader.hpp"
#include "indexer/indexer_tests_support/helpers.hpp"
#include "indexer/map_style.hpp"
#include "indexer/map_style_reader.hpp"
#include "indexer/scales.hpp"

#include "platform/platform.hpp"

namespace search
{
// TestWithClassificator ---------------------------------------------------------------------------
TestWithClassificator::TestWithClassificator()
{
  GetStyleReader().SetCurrentStyle(MapStyleMerged);
  classificator::Load();
}

// SearchTest --------------------------------------------------------------------------------------
SearchTest::SearchTest()
  : m_platform(GetPlatform())
  , m_scopedLog(LDEBUG)
  , m_engine(my::make_unique<storage::CountryInfoGetterForTesting>(), my::make_unique<ProcessorFactory>(),
             Engine::Params())
{
  indexer::tests_support::SetUpEditorForTesting(my::make_unique<EditorDelegate>(m_engine));
}

SearchTest::~SearchTest()
{
  for (auto const & file : m_files)
    Cleanup(file);
}

void SearchTest::RegisterCountry(std::string const & name, m2::RectD const & rect)
{
  auto & infoGetter =
      static_cast<storage::CountryInfoGetterForTesting &>(m_engine.GetCountryInfoGetter());
  infoGetter.AddCountry(storage::CountryDef(name, rect));
}

bool SearchTest::ResultsMatch(std::string const & query,
                              std::vector<shared_ptr<tests_support::MatchingRule>> const & rules)
{
  return ResultsMatch(query, "en" /* locale */, rules);
}

bool SearchTest::ResultsMatch(std::string const & query,
                              std::string const & locale,
                              std::vector<shared_ptr<tests_support::MatchingRule>> const & rules)
{
  tests_support::TestSearchRequest request(m_engine, query, locale, Mode::Everywhere, m_viewport);
  request.Run();
  return MatchResults(m_engine, rules, request.Results());
}

bool SearchTest::ResultsMatch(std::string const & query, Mode mode,
                              std::vector<shared_ptr<tests_support::MatchingRule>> const & rules)
{
  tests_support::TestSearchRequest request(m_engine, query, "en", mode, m_viewport);
  request.Run();
  return MatchResults(m_engine, rules, request.Results());
}

bool SearchTest::ResultsMatch(std::vector<search::Result> const & results, TRules const & rules)
{
  return MatchResults(m_engine, rules, results);
}

bool SearchTest::ResultsMatch(SearchParams const & params, TRules const & rules)
{
  tests_support::TestSearchRequest request(m_engine, params, m_viewport);
  request.Run();
  return ResultsMatch(request.Results(), rules);
}

unique_ptr<tests_support::TestSearchRequest> SearchTest::MakeRequest(std::string const & query)
{
  SearchParams params;
  params.m_query = query;
  params.m_inputLocale = "en";
  params.m_mode = Mode::Everywhere;
  params.m_suggestsEnabled = false;

  auto request = my::make_unique<tests_support::TestSearchRequest>(m_engine, params, m_viewport);
  request->Run();
  return request;
}

size_t SearchTest::CountFeatures(m2::RectD const & rect)
{
  size_t count = 0;
  auto counter = [&count](const FeatureType & /* ft */) { ++count; };
  m_engine.ForEachInRect(counter, rect, scales::GetUpperScale());
  return count;
}

// static
void SearchTest::Cleanup(platform::LocalCountryFile const & map)
{
  platform::CountryIndexes::DeleteFromDisk(map);
  map.DeleteFromDisk(MapOptions::Map);
}
}  // namespace search
