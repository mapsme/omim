#include "generator/processor_simple.hpp"

#include "generator/affiliation.hpp"
#include "generator/feature_builder.hpp"
#include "generator/feature_processing_layers.hpp"

#include "base/macros.hpp"

namespace generator
{
ProcessorSimple::ProcessorSimple(std::shared_ptr<FeatureProcessorQueue> const & queue,
                                 std::string const & filename)
{
  auto affilation = std::make_shared<feature::OneFileAffiliation>(filename);
  m_processingChain->Add(std::make_shared<PreserializeLayer>());
  m_processingChain->Add(std::make_shared<AffilationsFeatureLayer>(queue, affilation));
}

ProcessorSimple::ProcessorSimple(std::shared_ptr<FeatureProcessorQueue> const & queue,
                                 std::shared_ptr<LayerBase> const & processingChain)
  : m_queue(queue), m_processingChain(processingChain) {}

void ProcessorSimple::Process(feature::FeatureBuilder & fb)
{
  m_processingChain->Handle(fb);
}

void ProcessorSimple::Merge(FeatureProcessorInterface const * other)
{
  CHECK(other, ());

  other->MergeInto(this);
}

void ProcessorSimple::MergeInto(ProcessorSimple * other) const
{
  CHECK(other, ());

  other->m_processingChain->Merge(m_processingChain);
}
}  // namespace generator
