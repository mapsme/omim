#include "generator/translator_streets.hpp"

#include "generator/collector_interface.hpp"
#include "generator/feature_maker.hpp"
#include "generator/streets/streets_filter.hpp"
#include "generator/intermediate_data.hpp"

namespace generator
{
TranslatorStreets::TranslatorStreets(std::shared_ptr<FeatureProcessorInterface> const & processor,
                                     std::shared_ptr<cache::IntermediateData> const & cache)
  : Translator(processor, cache, std::make_shared<FeatureMakerSimple>(cache))

{
  SetFilter(std::make_shared<streets::StreetsFilter>());
}

std::shared_ptr<TranslatorInterface>
TranslatorStreets::Clone(std::shared_ptr<cache::IntermediateData> const & cache) const
{
  return std::make_shared<TranslatorStreets>(m_processor->Clone(), cache, m_featureMaker->Clone(),
                                             m_filter->Clone(), m_collector->Clone(cache->GetCache()));
}

void TranslatorStreets::Merge(TranslatorInterface const * other)
{
  CHECK(other, ());

  other->MergeInto(this);
}

void TranslatorStreets::MergeInto(TranslatorStreets * other) const
{
  CHECK(other, ());

  other->m_collector->Merge(m_collector.get());
  other->m_processor->Merge(m_processor.get());
}
}  // namespace generator
