#pragma once

#include "generator/affiliation.hpp"
#include "generator/processor_interface.hpp"

#include <memory>
#include <string>
#include <vector>

class FeatureBuilder1;

namespace feature
{
struct GenerateInfo;
}  // namespace feature

namespace generator
{
class WorldMapper;
class LayerBase;

// This class is implementation of EmitterInterface for the world.
class ProcessorWorld : public FeatureProcessorInterface
{
public:
  explicit ProcessorWorld(std::shared_ptr<FeatureProcessorQueue> const & queue,
                          std::string const & popularityFilename);
  explicit ProcessorWorld(std::shared_ptr<FeatureProcessorQueue> const & queue,
                          std::shared_ptr<LayerBase> const & processingChain);

  // EmitterInterface overrides:
  std::shared_ptr<FeatureProcessorInterface> Clone() const override;

  void Process(FeatureBuilder1 & feature) override;
  bool Finish() override;

  void Merge(FeatureProcessorInterface const * other) override;
  void MergeInto(ProcessorWorld * other) const override;

private:
  std::shared_ptr<FeatureProcessorQueue> m_queue;
  std::shared_ptr<LayerBase> m_processingChain;
};
}  // namespace generator
