#include "generator/processor_coastline.hpp"

#include "generator/coastlines_generator.hpp"
#include "generator/feature_builder.hpp"
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
  auto affiliation =
      feature::GetOrCreateAffiliation(feature::AffiliationType::Single, WORLD_COASTS_FILE_NAME);
  m_affiliationsLayer =
      std::make_shared<AffiliationsFeatureLayer<>>(kAffiliationsBufferSize, affiliation, m_queue);
  m_processingChain->Add(m_affiliationsLayer);
}

std::shared_ptr<FeatureProcessorInterface> ProcessorCoastline::Clone() const
{
  return std::make_shared<ProcessorCoastline>(m_queue);
}

void ProcessorCoastline::Process(feature::FeatureBuilder & feature)
{
  m_processingChain->Handle(feature);
}

void ProcessorCoastline::Finish() { m_affiliationsLayer->AddBufferToQueue(); }
}  // namespace generator
