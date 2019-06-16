#include "generator/processor_coastline.hpp"

#include "generator/coastlines_generator.hpp"
#include "generator/feature_builder.hpp"
#include "generator/feature_processing_layers.hpp"
#include "generator/feature_generator.hpp"
#include "generator/generate_info.hpp"
#include "generator/type_helper.hpp"

#include "base/logging.hpp"

#include <cstddef>

#include "defines.hpp"

namespace generator
{
ProcessorCoastline::ProcessorCoastline(std::shared_ptr<FeatureProcessorQueue> const & queue)
  : m_queue(queue)
{
  m_processingChain = std::make_shared<RepresentationCoastlineLayer>();
  m_processingChain->Add(std::make_shared<PrepareCoastlineFeatureLayer>());
  auto affilation = std::make_shared<feature::OneFileAffiliation>(WORLD_COASTS_FILE_NAME);
  m_processingChain->Add(std::make_shared<AffilationsFeatureLayer>(m_queue, affilation));
}

ProcessorCoastline::ProcessorCoastline(std::shared_ptr<FeatureProcessorQueue> const & queue,
                                       std::shared_ptr<LayerBase> const & processingChain)
  : m_queue(queue), m_processingChain(processingChain) {}

std::shared_ptr<FeatureProcessorInterface> ProcessorCoastline::Clone() const
{
  return std::make_shared<ProcessorCoastline>(m_queue, m_processingChain->CloneRecursive());
}

void ProcessorCoastline::Process(FeatureBuilder1 & feature)
{
  m_processingChain->Handle(feature);
}

bool ProcessorCoastline::Finish()
{
  return true;
}

void ProcessorCoastline::Merge(FeatureProcessorInterface const * other)
{
  CHECK(other, ());

  other->MergeInto(this);
}

void ProcessorCoastline::MergeInto(ProcessorCoastline * other) const
{
  CHECK(other, ());

  other->m_processingChain->Merge(m_processingChain);
}
}  // namespace generator
