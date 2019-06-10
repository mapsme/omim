#pragma once

#include "generator/feature_maker_base.hpp"
#include "generator/filter_interface.hpp"
#include "generator/relation_tags_enricher.hpp"
#include "generator/translator_interface.hpp"

#include <memory>
#include <string>
#include <vector>

struct OsmElement;

namespace generator
{
class FeatureProcessorInterface;
class CollectorInterface;

namespace cache
{
class IntermediateData;
}  // namespace cache

// Implementing this base class allows an object to create FeatureBuilder1 from OsmElement and then process it.
// You can add any collectors and filters.
class Translator : public TranslatorInterface
{
public:
  explicit Translator(std::shared_ptr<FeatureProcessorInterface> const & processor,
                      std::shared_ptr<cache::IntermediateData> const & cache,
                      std::shared_ptr<FeatureMakerBase> const & maker,
                      std::shared_ptr<FilterInterface> const & filter,
                      std::shared_ptr<CollectorInterface> const & collector);
  explicit Translator(std::shared_ptr<FeatureProcessorInterface> const & processor,
                      std::shared_ptr<cache::IntermediateData> const & cache,
                      std::shared_ptr<FeatureMakerBase> const & maker);

  // TranslatorInterface overrides:
  void Emit(OsmElement & element) override;
  bool Finish() override;

  void SetCollector(std::shared_ptr<CollectorInterface> const & collector);
  void SetFilter(std::shared_ptr<FilterInterface> const & filter);

protected:
  std::shared_ptr<FilterInterface> m_filter;
  std::shared_ptr<CollectorInterface> m_collector;
  RelationTagsEnricher m_tagsEnricher;
  std::shared_ptr<FeatureMakerBase> m_featureMaker;
  std::shared_ptr<FeatureProcessorInterface> m_processor;
  std::shared_ptr<cache::IntermediateData> m_cache;
};
}  // namespace generator
