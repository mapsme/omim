#include "generator/raw_generator.hpp"

#include "generator/complex_loader.hpp"
#include "generator/features_processing_helpers.hpp"
#include "generator/final_processor_coastline.hpp"
#include "generator/final_processor_country.hpp"
#include "generator/final_processor_world.hpp"
#include "generator/osm_source.hpp"
#include "generator/processor_factory.hpp"
#include "generator/raw_generator_writer.hpp"
#include "generator/translator_factory.hpp"
#include "generator/translators_pool.hpp"

#include "base/thread_pool_computational.hpp"

#include "defines.hpp"

namespace generator
{
RawGenerator::RawGenerator(feature::GenerateInfo & genInfo, size_t threadsCount, size_t chunkSize)
  : m_genInfo(genInfo)
  , m_threadsCount(threadsCount)
  , m_chunkSize(chunkSize)
  , m_cache(std::make_shared<generator::cache::IntermediateData>(m_intermediateDataObjectsCache, genInfo))
  , m_queue(std::make_shared<FeatureProcessorQueue>())
  , m_translators(std::make_shared<TranslatorCollection>())
{
}

void RawGenerator::ForceReloadCache()
{
  m_intermediateDataObjectsCache.Clear();
  m_cache = std::make_shared<cache::IntermediateData>(m_intermediateDataObjectsCache, m_genInfo);
}

std::shared_ptr<FeatureProcessorQueue> RawGenerator::GetQueue() { return m_queue; }

void RawGenerator::GenerateCountries(bool addAds)
{
  if (!m_genInfo.m_complexHierarchyFilename.empty())
    m_hierarchyNodesSet = GetOrCreateComplexLoader(m_genInfo.m_complexHierarchyFilename).GetIdsSet();

  auto const complexFeaturesMixer = std::make_shared<ComplexFeaturesMixer>(m_hierarchyNodesSet);

  auto processor = CreateProcessor(ProcessorType::Country, m_queue, m_genInfo.m_targetDir,
                                   m_genInfo.m_haveBordersForWholeWorld, complexFeaturesMixer);
  m_translators->Append(
      CreateTranslator(TranslatorType::Country, processor, m_cache, m_genInfo, addAds));
  m_finalProcessors.emplace(CreateCountryFinalProcessor(addAds));
}

void RawGenerator::GenerateWorld(bool addAds)
{
  auto processor =
      CreateProcessor(ProcessorType::World, m_queue, m_genInfo.m_popularPlacesFilename);
  m_translators->Append(
      CreateTranslator(TranslatorType::World, processor, m_cache, m_genInfo, addAds));
  m_finalProcessors.emplace(CreateWorldFinalProcessor());
}

void RawGenerator::GenerateCoasts()
{
  auto processor = CreateProcessor(ProcessorType::Coastline, m_queue);
  m_translators->Append(CreateTranslator(TranslatorType::Coastline, processor, m_cache));
  m_finalProcessors.emplace(CreateCoslineFinalProcessor());
}

void RawGenerator::GenerateCustom(std::shared_ptr<TranslatorInterface> const & translator)
{
  m_translators->Append(translator);
}

void RawGenerator::GenerateCustom(
    std::shared_ptr<TranslatorInterface> const & translator,
    std::shared_ptr<FinalProcessorIntermediateMwmInterface> const & finalProcessor)
{
  m_translators->Append(translator);
  m_finalProcessors.emplace(finalProcessor);
}

bool RawGenerator::Execute()
{
  if (!GenerateFilteredFeatures())
    return false;

  m_translators.reset();
  m_cache.reset();
  m_queue.reset();
  m_intermediateDataObjectsCache.Clear();

  while (!m_finalProcessors.empty())
  {
    base::thread_pool::computational::ThreadPool threadPool(m_threadsCount);
    while (true)
    {
      auto const finalProcessor = m_finalProcessors.top();
      m_finalProcessors.pop();
      threadPool.SubmitWork([finalProcessor{finalProcessor}]() { finalProcessor->Process(); });
      if (m_finalProcessors.empty() || *finalProcessor != *m_finalProcessors.top())
        break;
    }
  }

  LOG(LINFO, ("Final processing is finished."));
  return true;
}

std::vector<std::string> const & RawGenerator::GetNames() const { return m_names; }

RawGenerator::FinalProcessorPtr RawGenerator::CreateCoslineFinalProcessor()
{
  auto finalProcessor = make_shared<CoastlineFinalProcessor>(
      m_genInfo.GetTmpFileName(WORLD_COASTS_FILE_NAME, DATA_FILE_EXTENSION_TMP));
  finalProcessor->SetCoastlinesFilenames(
      m_genInfo.GetIntermediateFileName(WORLD_COASTS_FILE_NAME, ".geom"),
      m_genInfo.GetIntermediateFileName(WORLD_COASTS_FILE_NAME, RAW_GEOM_FILE_EXTENSION));
  return finalProcessor;
}

RawGenerator::FinalProcessorPtr RawGenerator::CreateCountryFinalProcessor(bool addAds)
{
  auto finalProcessor =
      make_shared<CountryFinalProcessor>(m_genInfo.m_targetDir, m_genInfo.m_tmpDir,
                                         m_genInfo.m_haveBordersForWholeWorld, m_threadsCount);
  finalProcessor->SetIsolinesDir(m_genInfo.m_isolinesDir);
  finalProcessor->SetBooking(m_genInfo.m_bookingDataFilename);
  finalProcessor->SetCitiesAreas(m_genInfo.GetIntermediateFileName(CITIES_AREAS_TMP_FILENAME));
  finalProcessor->SetPromoCatalog(m_genInfo.m_promoCatalogCitiesFilename);
  finalProcessor->SetMiniRoundabouts(m_genInfo.GetIntermediateFileName(MINI_ROUNDABOUTS_FILENAME));
  if (addAds)
    finalProcessor->SetFakeNodes(base::JoinPath(GetPlatform().ResourcesDir(), MIXED_NODES_FILE));

  if (m_genInfo.m_emitCoasts)
  {
    finalProcessor->SetCoastlines(
        m_genInfo.GetIntermediateFileName(WORLD_COASTS_FILE_NAME, ".geom"),
        m_genInfo.GetTmpFileName(WORLD_COASTS_FILE_NAME));
  }

  finalProcessor->DumpCitiesBoundaries(m_genInfo.m_citiesBoundariesFilename);
  finalProcessor->DumpRoutingCitiesBoundaries(
      m_genInfo.GetIntermediateFileName(ROUTING_CITY_BOUNDARIES_TMP_FILENAME),
      m_genInfo.GetIntermediateFileName(ROUTING_CITY_BOUNDARIES_DUMP_FILENAME));
  return finalProcessor;
}

RawGenerator::FinalProcessorPtr RawGenerator::CreateWorldFinalProcessor()
{
  auto finalProcessor = make_shared<WorldFinalProcessor>(
      m_genInfo.m_tmpDir,
      m_genInfo.GetIntermediateFileName(WORLD_COASTS_FILE_NAME, RAW_GEOM_FILE_EXTENSION));
  finalProcessor->SetPopularPlaces(m_genInfo.m_popularPlacesFilename);
  finalProcessor->SetCitiesAreas(m_genInfo.GetIntermediateFileName(CITIES_AREAS_TMP_FILENAME));
  finalProcessor->SetPromoCatalog(m_genInfo.m_promoCatalogCitiesFilename);
  return finalProcessor;
}

bool RawGenerator::GenerateFilteredFeatures()
{
  SourceReader reader =
      m_genInfo.m_osmFileName.empty() ? SourceReader() : SourceReader(m_genInfo.m_osmFileName);

  std::unique_ptr<ProcessorOsmElementsInterface> sourceProcessor;
  switch (m_genInfo.m_osmFileType)
  {
  case feature::GenerateInfo::OsmSourceType::O5M:
    sourceProcessor = std::make_unique<ProcessorOsmElementsFromO5M>(reader);
    break;
  case feature::GenerateInfo::OsmSourceType::XML:
    sourceProcessor = std::make_unique<ProcessorOsmElementsFromXml>(reader);
    break;
  }
  CHECK(sourceProcessor, ());

  TranslatorsPool translators(m_translators, m_threadsCount);
  RawGeneratorWriter rawGeneratorWriter(m_queue, m_genInfo.m_tmpDir);
  rawGeneratorWriter.Run();

  size_t element_pos = 0;
  std::vector<OsmElement> elements(m_chunkSize);
  while (sourceProcessor->TryRead(elements[element_pos]))
  {
    if (++element_pos != m_chunkSize)
      continue;

    translators.Emit(std::move(elements));
    elements = std::vector<OsmElement>(m_chunkSize);
    element_pos = 0;
  }
  elements.resize(element_pos);
  translators.Emit(std::move(elements));

  LOG(LINFO, ("Input was processed."));
  if (!translators.Finish())
    return false;

  rawGeneratorWriter.ShutdownAndJoin();
  m_names = rawGeneratorWriter.GetNames();
  LOG(LINFO, ("Names:", m_names));
  return true;
}
}  // namespace generator
