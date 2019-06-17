#include "generator/translator_coastline.hpp"

#include "generator/collector_interface.hpp"
#include "generator/feature_maker.hpp"
#include "generator/filter_collection.hpp"
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

  bool IsAccepted(FeatureBuilder const & feature) override
  {
    auto const & checker = ftypes::IsCoastlineChecker::Instance();
    return checker(feature.GetTypes());
  }
};
}  // namespace

TranslatorCoastline::TranslatorCoastline(std::shared_ptr<FeatureProcessorInterface> const & processor,
                                         std::shared_ptr<cache::IntermediateData> const & cache)
  : Translator(processor, cache, std::make_shared<FeatureMaker>(cache))
{
  auto filters = std::make_shared<FilterCollection>();
  filters->Append(std::make_shared<FilterPlanet>());
  filters->Append(std::make_shared<CoastlineFilter>());
  filters->Append(std::make_shared<FilterElements>(base::JoinPath(GetPlatform().ResourcesDir(), SKIPPED_ELEMENTS_FILE)));
  SetFilter(filters);
}

std::shared_ptr<TranslatorInterface>
TranslatorCoastline::Clone(std::shared_ptr<cache::IntermediateData> const & cache) const
{
  return Translator::CloneBase<TranslatorCoastline>(cache);
}

void TranslatorCoastline::Merge(TranslatorInterface const * other)
{
  CHECK(other, ());

  other->MergeInto(this);
}

void TranslatorCoastline::MergeInto(TranslatorCoastline * other) const
{
  CHECK(other, ());

  other->m_collector->Merge(m_collector.get());
  other->m_processor->Merge(m_processor.get());
}
}  // namespace generator
