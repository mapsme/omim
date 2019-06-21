#include "generator/processor_world.hpp"

#include "generator/cities_boundaries_builder.hpp"
#include "generator/feature_builder.hpp"
#include "generator/feature_processing_layers.hpp"
#include "generator/generate_info.hpp"

#include "defines.hpp"

namespace generator
{
ProcessorWorld::ProcessorWorld(std::shared_ptr<FeatureProcessorQueue> const & queue,
                               std::string const & popularityFilename)
{
  m_processingChain = std::make_shared<RepresentationLayer>();
  m_processingChain->Add(std::make_shared<PrepareFeatureLayer>());
  m_processingChain->Add(std::make_shared<WorldLayer>(popularityFilename));
  auto affilation = std::make_shared<feature::OneFileAffiliation>(WORLD_FILE_NAME);
  m_processingChain->Add(std::make_shared<AffilationsFeatureLayer>(queue, affilation));
}

ProcessorWorld::ProcessorWorld(std::shared_ptr<FeatureProcessorQueue> const & queue,
                        std::shared_ptr<LayerBase> const & processingChain)
  : m_queue(queue), m_processingChain(processingChain) {}

std::shared_ptr<FeatureProcessorInterface> ProcessorWorld::Clone() const
{
   return std::make_shared<ProcessorWorld>(m_queue, m_processingChain->CloneRecursive());
}

void ProcessorWorld::Process(feature::FeatureBuilder & feature)
{
  m_processingChain->Handle(feature);
}

bool ProcessorWorld::Finish()
{
  return true;
}

void ProcessorWorld::Merge(FeatureProcessorInterface const * other)
{
  CHECK(other, ());

  other->MergeInto(this);
}

void ProcessorWorld::MergeInto(ProcessorWorld * other) const
{
  CHECK(other, ());

  other->m_processingChain->Merge(m_processingChain);
}
}  // namespace generator
