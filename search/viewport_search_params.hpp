#pragma once

#include "search/hotels_filter.hpp"

#include <functional>
#include <memory>
#include <string>

namespace search
{
class Results;

struct ViewportSearchParams
{
  using TOnStarted = std::function<void()>;
  using TOnCompleted = std::function<void(Results const & results)>;

  std::string m_query;
  std::string m_inputLocale;
  std::shared_ptr<hotels_filter::Rule> m_hotelsFilter;

  TOnStarted m_onStarted;
  TOnCompleted m_onCompleted;
};
}  // namespace search
