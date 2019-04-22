#pragma once

#include "search/hotels_filter.hpp"
#include "search/mode.hpp"

#include "geometry/point2d.hpp"
#include "geometry/rect2d.hpp"

#include <cstddef>
#include <functional>
#include <memory>
#include <string>

#include <boost/optional.hpp>

namespace search
{
class Results;
class Tracer;

struct SearchParamsBase
{
  static size_t const kDefaultNumBookmarksResults = 1000;
  static size_t const kDefaultNumResultsEverywhere = 30;
  static size_t const kDefaultNumResultsInViewport = 200;

  bool IsEqualCommon(SearchParamsBase const & rhs) const;

  void Clear() { m_query.clear(); }

  std::string m_query;
  std::string m_inputLocale;

  boost::optional<m2::PointD> m_position;
  m2::RectD m_viewport;

  size_t m_maxNumResults = kDefaultNumResultsEverywhere;

  // Minimal distance between search results in mercators, needed for
  // pre-ranking of viewport search results.
  double m_minDistanceOnMapBetweenResults = 0.0;

  Mode m_mode = Mode::Everywhere;

  // Needed to generate search suggests.
  bool m_suggestsEnabled = false;

  // Needed to generate address for results.
  bool m_needAddress = false;

  // Needed to highlight matching parts of search result names.
  bool m_needHighlighting = false;
};

// Note: inheritance for aggregation data only, any of polymorphic usage is not supported.
struct SearchParams : SearchParamsBase
{
  using OnStarted = std::function<void()>;
  using OnResults = std::function<void(Results const &, SearchParamsBase const &)>;

  OnStarted m_onStarted;
  OnResults m_onResults;

  std::shared_ptr<hotels_filter::Rule> m_hotelsFilter;
  std::shared_ptr<Tracer> m_tracer;
};

std::string DebugPrint(SearchParams const & params);
}  // namespace search
