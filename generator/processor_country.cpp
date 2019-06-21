#include "generator/processor_country.hpp"

#include "generator/feature_builder.hpp"
#include "generator/feature_processing_layers.hpp"
#include "generator/generate_info.hpp"

#include "base/logging.hpp"

#include <fstream>

#include "defines.hpp"

namespace generator
{
ProcessorCountry::ProcessorCountry(std::shared_ptr<FeatureProcessorQueue> const & queue,
                                   std::string const & bordersPath, std::string const & layerLogFilename)
  : m_layerLogFilename(layerLogFilename)
{
  m_processingChain = std::make_shared<RepresentationLayer>();
  m_processingChain->Add(std::make_shared<PrepareFeatureLayer>());
  m_processingChain->Add(std::make_shared<CountryLayer>());
  auto affilation = std::make_shared<feature::CountriesFilesAffiliation>(bordersPath);
  m_processingChain->Add(std::make_shared<AffilationsFeatureLayer>(queue, affilation));
}

ProcessorCountry::ProcessorCountry(std::shared_ptr<FeatureProcessorQueue> const & queue,
                                   std::shared_ptr<LayerBase> const & processingChain)
  : m_queue(queue), m_processingChain(processingChain) {}

std::shared_ptr<FeatureProcessorInterface> ProcessorCountry::Clone() const
{
  return std::make_shared<ProcessorCountry>(m_queue, m_processingChain->CloneRecursive());
}

void ProcessorCountry::Process(feature::FeatureBuilder & feature)
{
  m_processingChain->Handle(feature);
}

bool ProcessorCountry::Finish()
{
//  WriteDump();
  return true;
}

void ProcessorCountry::WriteDump()
{
  std::ofstream file;
  file.exceptions(std::ios::failbit | std::ios::badbit);
  file.open(m_layerLogFilename);
  file << m_processingChain->GetAsStringRecursive();
  LOG(LINFO, ("Skipped elements were saved to", m_layerLogFilename));
}

void ProcessorCountry::Merge(FeatureProcessorInterface const * other)
{
  CHECK(other, ());

  other->MergeInto(this);
}

void ProcessorCountry::MergeInto(ProcessorCountry * other) const
{
  CHECK(other, ());

  other->m_processingChain->Merge(m_processingChain);
}
}  // namespace generator
