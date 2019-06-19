#include "generator/translator.hpp"

#include "generator/collector_interface.hpp"
#include "generator/emitter_interface.hpp"
#include "generator/intermediate_data.hpp"
#include "generator/osm_element.hpp"

#include "base/assert.hpp"

using namespace feature;

namespace generator
{
Translator::Translator(std::shared_ptr<EmitterInterface> const & emitter,
                       std::shared_ptr<cache::IntermediateDataReader> const & cache,
                       std::shared_ptr<FeatureMakerBase> const & maker,
                       FilterCollection const & filters,
                       CollectorCollection const & collectors)
  : m_filters(filters)
  , m_collectors(collectors)
  , m_tagsEnricher(cache)
  , m_featureMaker(maker)
  , m_emitter(emitter)
  , m_cache(cache)
{
  CHECK(m_emitter, ());
}

Translator::Translator(std::shared_ptr<EmitterInterface> const & emitter,
                       std::shared_ptr<cache::IntermediateDataReader> const & cache,
                       std::shared_ptr<FeatureMakerBase> const & maker)
  : Translator(emitter, cache, maker, {} /* filters */, {} /* collectors */) {}

void Translator::Emit(OsmElement & element)
{
  if (!m_filters.IsAccepted(element))
    return;

  Preprocess(element);
  m_tagsEnricher(element);
  m_collectors.Collect(element);
  m_featureMaker->Add(element);
  FeatureBuilder feature;
  while (m_featureMaker->GetNextFeature(feature))
  {
    if (!m_filters.IsAccepted(feature))
      continue;

    m_collectors.CollectFeature(feature, element);
    m_emitter->Process(feature);
  }
}

bool Translator::Finish()
{
  m_collectors.Save();
  return m_emitter->Finish();
}

void Translator::GetNames(std::vector<std::string> & names) const
{
  m_emitter->GetNames(names);
}

void Translator::AddCollector(std::shared_ptr<CollectorInterface> const & collector)
{
  m_collectors.Append(collector);
}

void Translator::AddCollectorCollection(CollectorCollection const & collectors)
{
  m_collectors.AddCollection(collectors);
}

void Translator::AddFilter(std::shared_ptr<FilterInterface> const & filter)
{
  m_filters.Append(filter);
}

void Translator::AddFilterCollection(FilterCollection const & filters)
{
  m_filters.AddCollection(filters);
}
}  // namespace generator
