#include "generator/translator_geo_objects.hpp"

#include "generator/collector_interface.hpp"
#include "generator/feature_maker.hpp"
#include "generator/filter_collection.hpp"
#include "generator/filter_interface.hpp"
#include "generator/geo_objects/geo_objects_filter.hpp"
#include "generator/intermediate_data.hpp"
#include "generator/osm_element.hpp"
#include "generator/osm_element_helpers.hpp"

namespace generator
{
TranslatorGeoObjects::TranslatorGeoObjects(std::shared_ptr<FeatureProcessorInterface> const & processor,
                                           std::shared_ptr<cache::IntermediateData> const & cache)
  : Translator(processor, cache, std::make_shared<FeatureMakerSimple>(cache))

{
  SetFilter(std::make_shared<geo_objects::GeoObjectsFilter>());
}

std::shared_ptr<TranslatorInterface>
TranslatorGeoObjects::Clone(std::shared_ptr<cache::IntermediateData> const & cache) const
{
  return std::make_shared<TranslatorGeoObjects>(m_processor->Clone(), cache, m_featureMaker->Clone(),
                                                m_filter->Clone(), m_collector->Clone(cache->GetCache()));
}

void TranslatorGeoObjects::Merge(TranslatorInterface const * other)
{
  CHECK(other, ());

  other->MergeInto(this);
}

void TranslatorGeoObjects::MergeInto(TranslatorGeoObjects * other) const
{
  CHECK(other, ());

  other->m_collector->Merge(m_collector.get());
  other->m_processor->Merge(m_processor.get());
}
}  // namespace generator
