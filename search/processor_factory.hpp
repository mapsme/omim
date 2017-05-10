#pragma once

#include "search/processor.hpp"
#include "search/search_params.hpp"
#include "search/suggest.hpp"

#include "base/stl_add.hpp"

#include <memory>

namespace storage
{
class CountryInfoGetter;
}

namespace search
{
class ProcessorFactory
{
public:
  virtual ~ProcessorFactory() = default;

  virtual std::unique_ptr<Processor> Build(Index & index, CategoriesHolder const & categories,
                                      vector<Suggest> const & suggests,
                                      storage::CountryInfoGetter const & infoGetter)
  {
    return my::make_unique<Processor>(index, categories, suggests, infoGetter);
  }
};
}  // namespace search
