#pragma once

#include "search/hotels_filter.hpp"
#include "search/result.hpp"
#include "search/search_params.hpp"

#include <functional>
#include <string>
#include <vector>

namespace search
{
struct EverywhereSearchParams
{
  using OnResults =
      std::function<void(Results const & results, std::vector<bool> const & isLocalAdsCustomer)>;

  std::string m_query;
  std::string m_inputLocale;
  shared_ptr<hotels_filter::Rule> m_hotelsFilter;

  OnResults m_onResults;
};
}  // namespace search
