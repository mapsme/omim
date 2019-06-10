#include "generator/translator_world.hpp"

#include "generator/collector_interface.hpp"
#include "generator/feature_maker.hpp"
#include "generator/filter_collection.hpp"
#include "generator/filter_planet.hpp"
#include "generator/filter_elements.hpp"
#include "generator/filter_world.hpp"
#include "generator/generate_info.hpp"
#include "generator/intermediate_data.hpp"
#include "generator/node_mixer.hpp"


#include "platform/platform.hpp"

#include "base/file_name_utils.hpp"

#include <algorithm>
#include <string>

#include "defines.hpp"

namespace generator
{
TranslatorWorld::TranslatorWorld(std::shared_ptr<FeatureProcessorInterface> const & processor,
                                 std::shared_ptr<cache::IntermediateData> const & cache,
                                 feature::GenerateInfo const & info)
  : Translator(processor, cache, std::make_shared<FeatureMaker>(cache))
  , m_tagAdmixer(info.GetIntermediateFileName("ways", ".csv"), info.GetIntermediateFileName("towns", ".csv"))
  , m_tagReplacer(GetPlatform().ResourcesDir() + REPLACED_TAGS_FILE)
{
  auto filters = std::make_shared<FilterCollection>();
  filters->Append(std::make_shared<FilterPlanet>());
  filters->Append(std::make_shared<FilterElements>(base::JoinPath(GetPlatform().ResourcesDir(), SKIPPED_ELEMENTS_FILE)));
  filters->Append(std::make_shared<FilterWorld>(info.m_popularPlacesFilename));
  SetFilter(filters);
}

std::shared_ptr<TranslatorInterface>
TranslatorWorld::Clone(std::shared_ptr<cache::IntermediateData> const & cache) const
{
  auto t = std::make_shared<TranslatorWorld>(m_processor->Clone(), cache, m_featureMaker->Clone(),
                                             m_filter->Clone(), m_collector->Clone(cache->GetCache()));
  t->m_tagAdmixer = m_tagAdmixer;
  t->m_tagReplacer = m_tagReplacer;
  return t;
}

void TranslatorWorld::Preprocess(OsmElement & element)
{
  // Here we can add new tags to the elements!
  m_tagReplacer(element);
  m_tagAdmixer(element);
}

void TranslatorWorld::Merge(TranslatorInterface const * other)
{
  CHECK(other, ());

  other->MergeInto(this);
}

void TranslatorWorld::MergeInto(TranslatorWorld * other) const
{
  CHECK(other, ());

  other->m_collector->Merge(m_collector.get());
  other->m_processor->Merge(m_processor.get());
}

TranslatorWorldWithAds::TranslatorWorldWithAds(std::shared_ptr<FeatureProcessorInterface> const & processor,
                                               std::shared_ptr<cache::IntermediateData> const & cache,
                                               feature::GenerateInfo const & info)
  : TranslatorWorld(processor, cache, info)
  , m_osmTagMixer(base::JoinPath(GetPlatform().ResourcesDir(), MIXED_TAGS_FILE)) {}

std::shared_ptr<TranslatorInterface>
TranslatorWorldWithAds::Clone(std::shared_ptr<cache::IntermediateData> const & cache) const
{
  auto t = std::make_shared<TranslatorWorldWithAds>(m_processor->Clone(), cache, m_featureMaker->Clone(),
                                                    m_filter->Clone(), m_collector->Clone(cache->GetCache()));
  t->m_tagAdmixer = m_tagAdmixer;
  t->m_tagReplacer = m_tagReplacer;
  t->m_osmTagMixer = m_osmTagMixer;
  return t;
}

void TranslatorWorldWithAds::Preprocess(OsmElement & element)
{
  // Here we can add new tags to the elements!
  m_osmTagMixer(element);
  TranslatorWorld::Preprocess(element);
}

bool TranslatorWorldWithAds::Finish()
{
  MixFakeNodes(GetPlatform().ResourcesDir() + MIXED_NODES_FILE,
               std::bind(&TranslatorWorldWithAds::Emit, this, std::placeholders::_1));
  return TranslatorWorld::Finish();
}

void TranslatorWorldWithAds::Merge(TranslatorInterface const * other)
{
  CHECK(other, ());

  TranslatorWorld::Merge(other);
}

void TranslatorWorldWithAds::MergeInto(TranslatorWorldWithAds * other) const
{
  CHECK(other, ());

  TranslatorWorld::MergeInto(other);
}
}  // namespace generator
