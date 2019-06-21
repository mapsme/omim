#include "generator/osm_source.hpp"

#include "generator/intermediate_data.hpp"
#include "generator/intermediate_elements.hpp"
#include "generator/osm_element.hpp"
#include "generator/processor_factory.hpp"
#include "generator/towns_dumper.hpp"
#include "generator/translator_factory.hpp"

#include "platform/platform.hpp"

#include "geometry/mercator.hpp"
#include "geometry/tree4d.hpp"

#include "base/assert.hpp"
#include "base/stl_helpers.hpp"
#include "base/file_name_utils.hpp"

#include <fstream>
#include <memory>
#include <set>

#include "defines.hpp"

using namespace std;
using namespace base::thread_pool::delayed;

namespace generator
{
// SourceReader ------------------------------------------------------------------------------------
SourceReader::SourceReader() : m_file(unique_ptr<istream, Deleter>(&cin, Deleter(false)))
{
  LOG_SHORT(LINFO, ("Reading OSM data from stdin"));
}

SourceReader::SourceReader(string const & filename)
  : m_file(unique_ptr<istream, Deleter>(new ifstream(filename), Deleter()))
{
  CHECK(static_cast<ifstream *>(m_file.get())->is_open(), ("Can't open file:", filename));
  LOG_SHORT(LINFO, ("Reading OSM data from", filename));
}

SourceReader::SourceReader(istringstream & stream)
  : m_file(unique_ptr<istream, Deleter>(&stream, Deleter(false)))
{
  LOG_SHORT(LINFO, ("Reading OSM data from memory"));
}

uint64_t SourceReader::Read(char * buffer, uint64_t bufferSize)
{
  m_file->read(buffer, bufferSize);
  return m_file->gcount();
}

// Functions ---------------------------------------------------------------------------------------
void AddElementToCache(cache::IntermediateDataWriter & cache, OsmElement & element)
{
  switch (element.m_type)
  {
  case OsmElement::EntityType::Node:
  {
    auto const pt = MercatorBounds::FromLatLon(element.m_lat, element.m_lon);
    cache.AddNode(element.m_id, pt.y, pt.x);
    break;
  }
  case OsmElement::EntityType::Way:
  {
    // Store way.
    WayElement way(element.m_id);
    for (uint64_t nd : element.Nodes())
      way.nodes.push_back(nd);

    if (way.IsValid())
      cache.AddWay(element.m_id, way);
    break;
  }
  case OsmElement::EntityType::Relation:
  {
    // store relation
    RelationElement relation;
    for (auto const & member : element.Members())
    {
      switch (member.m_type) {
      case OsmElement::EntityType::Node:
        relation.nodes.emplace_back(member.m_ref, string(member.m_role));
        break;
      case OsmElement::EntityType::Way:
        relation.ways.emplace_back(member.m_ref, string(member.m_role));
        break;
      case OsmElement::EntityType::Relation:
        // we just ignore type == "relation"
        break;
      default:
        break;
      }
    }

    for (auto const & tag : element.Tags())
      relation.tags.emplace(tag.m_key, tag.m_value);

    if (relation.IsValid())
      cache.AddRelation(element.m_id, relation);

    break;
  }
  default:
    break;
  }
}

void BuildIntermediateDataFromXML(SourceReader & stream, cache::IntermediateDataWriter & cache,
                                  TownsDumper & towns)
{
  XMLSource parser([&](OsmElement * element)
  {
    towns.CheckElement(*element);
    AddElementToCache(cache, *element);
  });
  ParseXMLSequence(stream, parser);
}

void ProcessOsmElementsFromXML(SourceReader & stream, function<void(OsmElement *)> processor)
{
  ProcessorXmlElementsFromXml processorXmlElementsFromXml(stream);
  OsmElement element;
  while (processorXmlElementsFromXml.TryRead(element))
  {
    processor(&element);
    element = {};
  }
}

void BuildIntermediateDataFromO5M(SourceReader & stream, cache::IntermediateDataWriter & cache,
                                  TownsDumper & towns)
{
  auto processor = [&](OsmElement * element) {
    towns.CheckElement(*element);
    AddElementToCache(cache, *element);
  };

  // Use only this function here, look into ProcessOsmElementsFromO5M
  // for more details.
  ProcessOsmElementsFromO5M(stream, processor);
}

void ProcessOsmElementsFromO5M(SourceReader & stream, function<void(OsmElement *)> processor)
{
  ProcessorOsmElementsFromO5M processorOsmElementsFromO5M(stream);
  OsmElement element;
  while (processorOsmElementsFromO5M.TryRead(element))
  {
    processor(&element);
    element = {};
  }
}

ProcessorOsmElementsFromO5M::ProcessorOsmElementsFromO5M(SourceReader & stream)
  : m_stream(stream)
  , m_dataset([&, this](uint8_t * buffer, size_t size) { return m_stream.Read(reinterpret_cast<char *>(buffer), size); })
  , m_pos(m_dataset.begin())
{
}

bool ProcessorOsmElementsFromO5M::TryRead(OsmElement & element)
{
  if (m_pos == m_dataset.end())
    return false;

  using Type = osm::O5MSource::EntityType;

  auto const translate = [](Type t) -> OsmElement::EntityType {
    switch (t)
    {
    case Type::Node: return OsmElement::EntityType::Node;
    case Type::Way: return OsmElement::EntityType::Way;
    case Type::Relation: return OsmElement::EntityType::Relation;
    default: return OsmElement::EntityType::Unknown;
    }
  };

  auto entity = *m_pos;

  element.m_id = entity.id;
  switch (entity.type)
  {
  case Type::Node:
  {
    element.m_type = OsmElement::EntityType::Node;
    element.m_lat = entity.lat;
    element.m_lon = entity.lon;
    break;
  }
  case Type::Way:
  {
    element.m_type = OsmElement::EntityType::Way;
    for (uint64_t nd : entity.Nodes())
      element.AddNd(nd);
    break;
  }
  case Type::Relation:
  {
    element.m_type = OsmElement::EntityType::Relation;
    for (auto const & member : entity.Members())
      element.AddMember(member.ref, translate(member.type), member.role);
    break;
  }
  default: break;
  }

  for (auto const & tag : entity.Tags())
    element.AddTag(tag.key, tag.value);

  ++m_pos;
  return true;
}

ProcessorXmlElementsFromXml::ProcessorXmlElementsFromXml(SourceReader & stream)
  : m_stream(stream)
  , m_xmlSource([&, this](auto * element) { m_queue.emplace(*element); })
  , m_parser(m_stream, m_xmlSource)
{
}

bool ProcessorXmlElementsFromXml::TryReadFromQueue(OsmElement & element)
{
  if (m_queue.empty())
    return false;

  element = m_queue.front();
  m_queue.pop();
  return true;
}

bool ProcessorXmlElementsFromXml::TryRead(OsmElement & element)
{
  if (TryReadFromQueue(element))
    return true;

  while (m_parser.Read())
  {
    if (TryReadFromQueue(element))
      return true;
  }

  return TryReadFromQueue(element);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Generate functions implementations.
///////////////////////////////////////////////////////////////////////////////////////////////////
bool GenerateIntermediateData(feature::GenerateInfo & info)
{
  auto nodes = cache::CreatePointStorageWriter(info.m_nodeStorageType,
                                               info.GetIntermediateFileName(NODES_FILE));
  cache::IntermediateDataWriter cache(nodes, info);
  TownsDumper towns;
  SourceReader reader = info.m_osmFileName.empty() ? SourceReader() : SourceReader(info.m_osmFileName);

  LOG(LINFO, ("Data source:", info.m_osmFileName));

  switch (info.m_osmFileType)
  {
  case feature::GenerateInfo::OsmSourceType::XML:
    BuildIntermediateDataFromXML(reader, cache, towns);
    break;
  case feature::GenerateInfo::OsmSourceType::O5M:
    BuildIntermediateDataFromO5M(reader, cache, towns);
    break;
  }

  cache.SaveIndex();
  towns.Dump(info.GetIntermediateFileName(TOWNS_FILE));
  LOG(LINFO, ("Added points count =", nodes->GetNumProcessedPoints()));
  return true;
}

TranslatorsPool::TranslatorsPool(shared_ptr<TranslatorCollection> const & original,
                                 shared_ptr<generator::cache::IntermediateData> const & cache,
                                 size_t copyCount)
  : m_translators({original})
  , m_threadPool(copyCount + 1, ThreadPool::Exit::ExecPending)
{
  m_freeTranslators.Push(0);
  m_translators.reserve(copyCount + 1);
  for (size_t i = 0; i < copyCount; ++i)
  {
    auto cache_ = cache->Clone();
    auto translator = original->Clone(cache_);
    m_translators.emplace_back(translator);
    m_freeTranslators.Push(i + 1);
  }
}

void TranslatorsPool::Emit(vector<OsmElement> & elements)
{
  base::threads::DataWrapper<size_t> d;
  m_freeTranslators.WaitAndPop(d);
  auto const idx = d.Get();
  m_threadPool.Push([&, idx, elements{move(elements)}]() mutable {
    for (auto & element : elements)
      m_translators[idx]->Emit(element);

    m_freeTranslators.Push(idx);
  });
}

bool TranslatorsPool::Finish()
{
  m_threadPool.ShutdownAndJoin();
  auto const & translator = m_translators.front();
  for (size_t i = 1; i < m_translators.size(); ++i)
    translator->Merge(m_translators[i].get());

  return translator->Finish();
}

RawGeneratorWriter::RawGeneratorWriter(shared_ptr<FeatureProcessorQueue> const & queue,
                                       string const & path)
  : m_queue(queue), m_path(path) {}


RawGeneratorWriter::~RawGeneratorWriter()
{
  ShutdownAndJoin();
}

void RawGeneratorWriter::Run()
{
  m_thread = thread([&]() {
    while (true)
    {
      FeatureProcessorChank chank;
      m_queue->WaitAndPop(chank);
      if (chank.IsEmpty())
        return;

      Write(chank.Get());
    }
  });
}

vector<string> RawGeneratorWriter::GetNames()
{
  ShutdownAndJoin();
  vector<string> names;
  names.reserve(m_collectors.size());
  for (const auto  & p : m_collectors)
    names.emplace_back(p.first);

  return names;
}

void RawGeneratorWriter::Write(ProcessedData const & chank)
{
  for (auto const & affiliation : chank.m_affiliations)
  {
    if (affiliation.empty())
      continue;

    if (m_collectors.count(affiliation) == 0)
    {
      auto path = base::JoinPath(m_path, affiliation + DATA_FILE_EXTENSION_TMP);
      m_collectors.emplace(affiliation, make_unique<FeatureBuilderWriter>(move(path)));
    }

    m_collectors[affiliation]->Write(chank.m_fb);
  }
}

void RawGeneratorWriter::ShutdownAndJoin()
{
  m_queue->Push({});
  if (m_thread.joinable())
    m_thread.join();
}

RawGenerator::RawGenerator(feature::GenerateInfo & genInfo, size_t threadsCount, size_t chankSize)
  : m_genInfo(genInfo)
  , m_threadsCount(threadsCount)
  , m_chankSize(chankSize)
  , m_cache(make_shared<generator::cache::IntermediateData>(genInfo))
  , m_queue(make_shared<FeatureProcessorQueue>())
  , m_translators(make_shared<TranslatorCollection>())
{
}

void RawGenerator::ForceReloadCache()
{
  m_cache = make_shared<generator::cache::IntermediateData>(m_genInfo, true /* forceReload */);
}

std::shared_ptr<FeatureProcessorQueue> RawGenerator::GetQueue()
{
  return m_queue;
}

void RawGenerator::GenerateCountries(bool disableAds)
{
  auto processor = CreateProcessor(ProcessorType::Country, m_queue, m_genInfo.m_targetDir, "");
  auto const translatorType = disableAds ? TranslatorType::Country : TranslatorType::CountryWithAds;
  m_translators->Append(CreateTranslator(translatorType, processor, m_cache, m_genInfo));
  m_finalProcessors.emplace(CreateCountryFinalProcessor());
}

void RawGenerator::GenerateWorld(bool disableAds)
{
  auto processor = CreateProcessor(ProcessorType::World, m_queue, m_genInfo.m_popularPlacesFilename);
  auto const translatorType = disableAds ? TranslatorType::World : TranslatorType::WorldWithAds;
  m_translators->Append(CreateTranslator(translatorType, processor, m_cache, m_genInfo));
  m_finalProcessors.emplace(CreateWorldFinalProcessor());
}

void RawGenerator::GenerateCoasts()
{
  auto processor = CreateProcessor(ProcessorType::Coastline, m_queue);
  m_translators->Append(CreateTranslator(TranslatorType::Coastline, processor, m_cache));
  m_finalProcessors.emplace(CreateCoslineFinalProcessor());
}

void RawGenerator::GenerateRegionFeatures(string const & filename)
{
  auto processor = CreateProcessor(ProcessorType::Simple, m_queue, filename);
  m_translators->Append(CreateTranslator(TranslatorType::Regions, processor, m_cache, m_genInfo));
}

void RawGenerator::GenerateStreetsFeatures(string const & filename)
{
  auto processor = CreateProcessor(ProcessorType::Simple, m_queue, filename);
  m_translators->Append(CreateTranslator(TranslatorType::Streets, processor, m_cache));
}

void RawGenerator::GenerateGeoObjectsFeatures(string const & filename)
{
  auto processor = CreateProcessor(ProcessorType::Simple, m_queue, filename);
  m_translators->Append(CreateTranslator(TranslatorType::GeoObjects, processor, m_cache));
}

void RawGenerator::GenerateCustom(std::shared_ptr<TranslatorInterface> const & translator)
{
  m_translators->Append(translator);
}

void RawGenerator::GenerateCustom(std::shared_ptr<TranslatorInterface> const & translator,
                                  std::shared_ptr<FinalProcessorIntermediateMwmInteface> const & finalProcessor)
{
  m_translators->Append(translator);
  m_finalProcessors.emplace(finalProcessor);
}

bool RawGenerator::Execute()
{  
  if (!GenerateFilteredFeatures())
    return false;

  while (!m_finalProcessors.empty())
  {
    ThreadPool threadPool(m_threadsCount, ThreadPool::Exit::ExecPending);
    do
    {
      auto const finalProcessor = m_finalProcessors.top();
      threadPool.Push([finalProcessor]() {
        finalProcessor->Process();
      });
      m_finalProcessors.pop();
      if (m_finalProcessors.empty() || *finalProcessor != *m_finalProcessors.top())
        break;
    }
    while (true);
  }

  LOG(LINFO, ("Final processing is finished."));
  return true;
}

RawGenerator::FinalProcessorPtr RawGenerator::CreateCoslineFinalProcessor()
{
  auto finalProcessor = make_shared<CoastlineFinalProcessor>(
                          m_genInfo.GetTmpFileName(WORLD_COASTS_FILE_NAME, DATA_FILE_EXTENSION_TMP));
  finalProcessor->SetCoastlineGeomFilename(m_genInfo.GetIntermediateFileName(WORLD_COASTS_FILE_NAME, ".geom"));
  finalProcessor->SetCoastlineRawGeomFilename(
        m_genInfo.GetIntermediateFileName(WORLD_COASTS_FILE_NAME, RAW_GEOM_FILE_EXTENSION));
  return finalProcessor;
}

RawGenerator::FinalProcessorPtr RawGenerator::CreateCountryFinalProcessor()
{
  auto finalProcessor = make_shared<CountryFinalProcessor>(m_genInfo.m_targetDir, m_genInfo.m_tmpDir,
                                                           m_threadsCount);
  finalProcessor->NeedBookig(m_genInfo.m_bookingDataFilename);
  finalProcessor->UseCityBoundaries(m_genInfo.GetIntermediateFileName(CITY_BOUNDARIES_TMP_FILENAME));
  finalProcessor->SetPromoCatalog(m_genInfo.m_promoCatalogCitiesFilename);
  finalProcessor->DumpCityBoundaries(m_genInfo.m_citiesBoundariesFilename);
  if (m_genInfo.m_emitCoasts)
  {
    finalProcessor->AddCoastlines(m_genInfo.GetIntermediateFileName(WORLD_COASTS_FILE_NAME, ".geom"),
                                  m_genInfo.GetTmpFileName(WORLD_COASTS_FILE_NAME));
  }
  return finalProcessor;
}

RawGenerator::FinalProcessorPtr RawGenerator::CreateWorldFinalProcessor()
{
  auto finalProcessor = make_shared<WorldFinalProcessor>(
                          m_genInfo.m_tmpDir,
                          m_genInfo.GetIntermediateFileName(WORLD_COASTS_FILE_NAME, RAW_GEOM_FILE_EXTENSION),
                          m_genInfo.m_popularPlacesFilename);
  finalProcessor->UseCityBoundaries(m_genInfo.GetIntermediateFileName(CITY_BOUNDARIES_TMP_FILENAME));
  finalProcessor->SetPromoCatalog(m_genInfo.m_promoCatalogCitiesFilename);
  return finalProcessor;
}

bool RawGenerator::GenerateFilteredFeatures()
{
  SourceReader reader = m_genInfo.m_osmFileName.empty() ? SourceReader()
                                                        : SourceReader(m_genInfo.m_osmFileName);

  std::unique_ptr<ProcessorOsmElementsInterface> sourseProcessor;
  switch (m_genInfo.m_osmFileType) {
  case feature::GenerateInfo::OsmSourceType::O5M:
    sourseProcessor = std::make_unique<ProcessorOsmElementsFromO5M>(reader);
    break;
  case feature::GenerateInfo::OsmSourceType::XML:
    sourseProcessor = std::make_unique<ProcessorXmlElementsFromXml>(reader);
    break;
  }

  TranslatorsPool translators(m_translators, m_cache, m_threadsCount - 1 /* copyCount */);
  RawGeneratorWriter rawGeneratorWriter(m_queue, m_genInfo.m_tmpDir);
  rawGeneratorWriter.Run();

  size_t element_pos = 0;
  vector<OsmElement> elements(m_chankSize);
  while(sourseProcessor->TryRead(elements[element_pos]))
  {
    if (++element_pos != m_chankSize)
      continue;

    translators.Emit(elements);
    elements = vector<OsmElement>(m_chankSize);
    element_pos = 0;
  }
  elements.resize(element_pos);
  translators.Emit(elements);

  LOG(LINFO, ("Input was processed."));
  if (!translators.Finish())
    return false;

  LOG(LINFO, ("Names:", rawGeneratorWriter.GetNames()));
  return true;
}
}  // namespace generator
