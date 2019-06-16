#pragma once

#include "generator/processor_interface.hpp"

#include <memory>
#include <string>
#include <vector>

class FeatureBuilder1;

namespace generator
{
class LayerBase;

// ProcessorSimpleWriter class is a simple emitter. It does not filter objects.
class ProcessorSimple : public FeatureProcessorInterface
{
public:
  explicit ProcessorSimple(std::shared_ptr<FeatureProcessorQueue> const & queue,
                           std::string const & filename);
  explicit ProcessorSimple(std::shared_ptr<FeatureProcessorQueue> const & queue,
                           std::shared_ptr<LayerBase> const & processingChain);

  // FeatureProcessorInterface overrides:
  void Process(FeatureBuilder1 & fb) override;
  bool Finish() override { return true; }

  void Merge(FeatureProcessorInterface const * other) override;
  void MergeInto(ProcessorSimple * other) const override;

private:
  std::shared_ptr<FeatureProcessorQueue> m_queue;
  std::shared_ptr<LayerBase> m_processingChain;
};
}  // namespace generator
