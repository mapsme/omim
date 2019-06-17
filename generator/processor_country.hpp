#pragma once

#include "generator/feature_builder.hpp"
#include "generator/processor_interface.hpp"

#include <memory>
#include <string>
#include <vector>

namespace feature
{
struct GenerateInfo;
}  // namespace feature

namespace generator
{
class CountryMapper;
class LayerBase;
// This class is the implementation of FeatureProcessorInterface for countries.
class ProcessorCountry : public FeatureProcessorInterface
{
public:
  explicit ProcessorCountry(std::shared_ptr<FeatureProcessorQueue> const & queue,
                            std::string const & bordersPath, std::string const & layerLogFilename);
  explicit ProcessorCountry(std::shared_ptr<FeatureProcessorQueue> const & queue,
                            std::shared_ptr<LayerBase> const & processingChain);

  // FeatureProcessorInterface overrides:
  std::shared_ptr<FeatureProcessorInterface> Clone() const override;

  void Process(feature::FeatureBuilder & feature) override;
  bool Finish() override;

  void Merge(FeatureProcessorInterface const * other) override;
  void MergeInto(ProcessorCountry * other) const override;

private:
  void WriteDump();

  std::string m_layerLogFilename;
  std::shared_ptr<FeatureProcessorQueue> m_queue;
  std::shared_ptr<LayerBase> m_processingChain;
};
}  // namespace generator
