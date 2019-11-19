#pragma once

#include "generator/feature_builder.hpp"
#include "generator/feature_processing_layers.hpp"
#include "generator/processor_interface.hpp"

#include <memory>
#include <string>

namespace feature
{
struct GenerateInfo;
}  // namespace feature

namespace generator
{
// This class is the implementation of FeatureProcessorInterface for complexes.
class ProcessorComplex : public FeatureProcessorInterface
{
public:
  explicit ProcessorComplex(std::shared_ptr<FeatureProcessorQueue> const & queue,
                            std::string const & bordersPath, std::string const & layerLogFilename,
                            bool haveBordersForWholeWorld);

  // FeatureProcessorInterface overrides:
  std::shared_ptr<FeatureProcessorInterface> Clone() const override;

  void Process(feature::FeatureBuilder & feature) override;
  void Finish() override;

  void Merge(FeatureProcessorInterface const & other) override;
  void MergeInto(ProcessorComplex & other) const override;

private:
  void WriteDump();

  std::string m_bordersPath;
  std::string m_layerLogFilename;
  std::shared_ptr<AffiliationsFeatureLayer<>> m_affiliationsLayer;
  std::shared_ptr<FeatureProcessorQueue> m_queue;
  std::shared_ptr<LayerBase> m_processingChain;
  bool m_haveBordersForWholeWorld;
};
}  // namespace generator
