#include "generator/translator_coastline.hpp"

#include "generator/feature_maker.hpp"
#include "generator/filter_elements.hpp"
#include "generator/filter_planet.hpp"
#include "generator/generate_info.hpp"
#include "generator/intermediate_data.hpp"

#include "indexer/ftypes_matcher.hpp"

#include "platform/platform.hpp"

#include "base/file_name_utils.hpp"

#include <memory>

#include "defines.hpp"

using namespace feature;

namespace generator
{
namespace
{
class CoastlineFilter : public FilterInterface
{
public:
  // FilterInterface overrides:
  std::shared_ptr<FilterInterface> Clone() const override
  {
    return std::make_shared<CoastlineFilter>();
  }

  bool IsAccepted(FeatureBuilder1 const & feature) override
  {
    auto const & checker = ftypes::IsCoastlineChecker::Instance();
    return checker(feature.GetTypes());
  }
};
}  // namespace

TranslatorCoastline::TranslatorCoastline(std::shared_ptr<EmitterInterface> const & emitter,
                                         std::shared_ptr<cache::IntermediateDataReader> const & cache)
  : Translator(emitter, cache, std::make_shared<FeatureMaker>(cache))
{
  AddFilter(std::make_shared<FilterPlanet>());
  AddFilter(std::make_shared<CoastlineFilter>());
  AddFilter(std::make_shared<FilterElements>(base::JoinPath(GetPlatform().ResourcesDir(), SKIPPED_ELEMENTS_FILE)));
}
}  // namespace generator
