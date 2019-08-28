#pragma once

#include "generator/processor_interface.hpp"
#include "generator/translator.hpp"

#include <memory>

namespace cache
{
class IntermediateData;
}  // namespace cache

namespace generator
{
// The TranslatorGeoObjects class implements translator for building geo objects.
// Every GeoObject is either a building or a POI.
class TranslatorGeoObjects : public Translator
{
public:
  explicit TranslatorGeoObjects(std::shared_ptr<FeatureProcessorInterface> const & processor,
                                std::shared_ptr<cache::IntermediateData> const & cache);

  // TranslatorInterface overrides:
  std::shared_ptr<TranslatorInterface> Clone() const override;

  void Merge(TranslatorInterface const & other) override;
  void MergeInto(TranslatorGeoObjects & other) const override;

protected:
  using Translator::Translator;
};
}  // namespace generator
