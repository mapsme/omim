#pragma once

#include "generator/feature_builder.hpp"
#include "generator/feature_processing_layers.hpp"
#include "generator/processor_interface.hpp"

#include <memory>
#include <string>
#include <vector>

namespace generator
{
// This class is implementation oft FeatureProcessorInterface for coastlines.
class ProcessorCoastline : public FeatureProcessorInterface
{
public:
  explicit ProcessorCoastline(std::shared_ptr<FeatureProcessorQueue> const & queue);

  // EmitterInterface overrides:
  std::shared_ptr<FeatureProcessorInterface> Clone() const override;

  void Process(feature::FeatureBuilder & feature) override;
  bool Finish() override;

  void Merge(FeatureProcessorInterface const * other) override;
  void MergeInto(ProcessorCoastline * other) const override;

private:
  std::shared_ptr<AffilationsFeatureLayer<>> m_affilationsLayer;
  std::shared_ptr<FeatureProcessorQueue> m_queue;
  std::shared_ptr<LayerBase> m_processingChain;
};
}  // namespace generator
