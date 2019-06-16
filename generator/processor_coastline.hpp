#pragma once

#include "generator/processor_interface.hpp"

#include <memory>
#include <string>
#include <vector>

class FeatureBuilder1;

namespace generator
{
class LayerBase;

// This class is implementation oft FeatureProcessorInterface for coastlines.
class ProcessorCoastline : public FeatureProcessorInterface
{
public:
  explicit ProcessorCoastline(std::shared_ptr<FeatureProcessorQueue> const & queue);
  explicit ProcessorCoastline(std::shared_ptr<FeatureProcessorQueue> const & queue,
                              std::shared_ptr<LayerBase> const & processingChain);

  // EmitterInterface overrides:
  std::shared_ptr<FeatureProcessorInterface> Clone() const override;

  void Process(FeatureBuilder1 & feature) override;
  bool Finish() override;

  void Merge(FeatureProcessorInterface const * other) override;
  void MergeInto(ProcessorCoastline * other) const override;

private:
  std::shared_ptr<FeatureProcessorQueue> m_queue;
  std::shared_ptr<LayerBase> m_processingChain;
};
}  // namespace generator
