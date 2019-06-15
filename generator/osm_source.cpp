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

#include "coding/parse_xml.hpp"

#include "base/assert.hpp"
#include "base/stl_helpers.hpp"
#include "base/file_name_utils.hpp"

#include <fstream>
#include <memory>
#include <set>

#include "defines.hpp"

using namespace std;

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
  XMLSource parser([&](OsmElement * element) { processor(element); });
  ParseXMLSequence(stream, parser);
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
  using Type = osm::O5MSource::EntityType;

  osm::O5MSource dataset([&stream](uint8_t * buffer, size_t size)
  {
    return stream.Read(reinterpret_cast<char *>(buffer), size);
  });

  auto translate = [](Type t) -> OsmElement::EntityType {
    switch (t)
    {
    case Type::Node: return OsmElement::EntityType::Node;
    case Type::Way: return OsmElement::EntityType::Way;
    case Type::Relation: return OsmElement::EntityType::Relation;
    default: return OsmElement::EntityType::Unknown;
    }
  };

  // Be careful, we could call Nodes(), Members(), Tags() from O5MSource::Entity
  // only once (!). Because these functions read data from file simultaneously with
  // iterating in loop. Furthermore, into Tags() method calls Nodes.Skip() and Members.Skip(),
  // thus first call of Nodes (Members) after Tags() will not return any results.
  // So don not reorder the "for" loops (!).

  for (auto const & entity : dataset)
  {
    OsmElement p;
    p.m_id = entity.id;

    switch (entity.type)
    {
    case Type::Node:
    {
      p.m_type = OsmElement::EntityType::Node;
      p.m_lat = entity.lat;
      p.m_lon = entity.lon;
      break;
    }
    case Type::Way:
    {
      p.m_type = OsmElement::EntityType::Way;
      for (uint64_t nd : entity.Nodes())
        p.AddNd(nd);
      break;
    }
    case Type::Relation:
    {
      p.m_type = OsmElement::EntityType::Relation;
      for (auto const & member : entity.Members())
        p.AddMember(member.ref, translate(member.type), member.role);
      break;
    }
    default: break;
    }

    for (auto const & tag : entity.Tags())
      p.AddTag(tag.key, tag.value);

    processor(&p);
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

///////////////////////////////////////////////////////////////////////////////////////////////////
// Generate functions implementations.
///////////////////////////////////////////////////////////////////////////////////////////////////

bool GenerateRaw(feature::GenerateInfo & info, TranslatorInterface & translators)
{
  auto const fn = [&](OsmElement * element) {
    CHECK(element, ());
    translators.Emit(*element);
  };

  SourceReader reader = info.m_osmFileName.empty() ? SourceReader() : SourceReader(info.m_osmFileName);
  switch (info.m_osmFileType)
  {
  case feature::GenerateInfo::OsmSourceType::XML:
    ProcessOsmElementsFromXML(reader, fn);
    break;
  case feature::GenerateInfo::OsmSourceType::O5M:
    ProcessOsmElementsFromO5M(reader, fn);
    break;
  }

  LOG(LINFO, ("Processing", info.m_osmFileName, "done."));
  if (!translators.Finish())
    return false;

  return true;
}

bool GenerateIntermediateData(feature::GenerateInfo & info)
{
  try
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
  }
  catch (Writer::Exception const & e)
  {
    LOG(LCRITICAL, ("Error with file:", e.what()));
  }
  return true;
}

TranslatorsPool::TranslatorsPool(std::shared_ptr<TranslatorCollection> const & original,
                                 std::shared_ptr<generator::cache::IntermediateData> const & cache,
                                 size_t copyCount)
  : m_translators({original})
  , m_threadPool(copyCount + 1)
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

void TranslatorsPool::Emit(std::vector<OsmElement> & elements)
{
  base::threads::DataWrapper<size_t> d;
  m_freeTranslators.WaitAndPop(d);
  auto const idx = d.Get();
  m_threadPool.Submit([&, idx, elements{std::move(elements)}]() mutable {
    for (auto & element : elements)
      m_translators[idx]->Emit(element);
    m_freeTranslators.Push(idx);
  });
}

bool TranslatorsPool::Finish()
{
  auto const & translator = m_translators.front();
  for (size_t i = 1; i < m_translators.size(); ++i)
    translator->Merge(m_translators[i].get());

  return translator->Finish();
}

RawGeneratorWriter::RawGeneratorWriter(std::shared_ptr<FeatureProcessorQueue> const & queue,
                                       std::string const & path)
  : m_queue(queue), m_path(path) {}


RawGeneratorWriter::~RawGeneratorWriter()
{
  Finish();
}

void RawGeneratorWriter::Run()
{
  m_thread = std::thread([&]() {
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

std::vector<std::string> RawGeneratorWriter::GetNames()
{
  Finish();
  std::vector<std::string> names;
  names.resize(m_collectors.size());
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
      m_collectors.emplace(affiliation, std::make_unique<feature::FeaturesCollector>(std::move(path)));
    }

    m_collectors[affiliation]->Collect(chank.m_fb);
  }
}

void RawGeneratorWriter::Finish()
{
  m_queue->Push({});
  if (m_thread.joinable())
    m_thread.join();
}

RawGenerator::RawGenerator(feature::GenerateInfo & genInfo, size_t threadsCount)
  : m_genInfo(genInfo)
  , m_threadsCount(threadsCount)
  , m_cache(std::make_shared<generator::cache::IntermediateData>(genInfo))
  , m_queue(std::make_shared<FeatureProcessorQueue>())
  , m_translators(std::make_shared<TranslatorCollection>())
{
}

void RawGenerator::GenerateCountries(bool disableAds)
{
  auto processor = CreateProcessor(ProcessorType::Country, m_queue, m_genInfo.m_targetDir, "");
  auto const translatorType = disableAds ? TranslatorType::Country : TranslatorType::CountryWithAds;
  m_translators->Append(CreateTranslator(translatorType, processor, m_cache, m_genInfo));

//  auto finalProcessor = std::make_shared<CountryFinalProcessor>(m_genInfo.m_targetDir, m_genInfo.m_tmpDir,
//                                                                m_threadsCount);
//  finalProcessor->NeedBookig(m_genInfo.m_bookingDatafileName);

//  auto const cityBountaryTmpFilename = m_genInfo.GetIntermediateFileName(CITY_BOUNDARIES_TMP_FILENAME);
//  finalProcessor->UseCityBoundaries(cityBountaryTmpFilename);

//  auto const geomFilename = m_genInfo.GetIntermediateFileName(WORLD_COASTS_FILE_NAME, ".geom");
//  auto const worldCoastsFilename = m_genInfo.GetTmpFileName(WORLD_COASTS_FILE_NAME);
//  finalProcessor->AddCoastlines(geomFilename, worldCoastsFilename);

//  m_finalProcessors.emplace(finalProcessor);
}

void RawGenerator::GenerateWorld(bool disableAds)
{
  auto processor = CreateProcessor(ProcessorType::World, m_queue);
  auto const translatorType = disableAds ? TranslatorType::World : TranslatorType::WorldWithAds;
  m_translators->Append(CreateTranslator(translatorType, processor, m_cache, m_genInfo));

  auto const worldFilename = m_genInfo.GetTmpFileName(WORLD_FILE_NAME, DATA_FILE_EXTENSION_TMP);
  auto const geomFilename = m_genInfo.GetIntermediateFileName(WORLD_COASTS_FILE_NAME, RAW_GEOM_FILE_EXTENSION);
  auto finalProcessor = std::make_shared<WorldFinalProcessor>(worldFilename, geomFilename,
                                                              m_genInfo.m_popularPlacesFilename);
  auto const cityBountaryTmpFilename = m_genInfo.GetIntermediateFileName(CITY_BOUNDARIES_TMP_FILENAME);
  finalProcessor->UseCityBoundaries(cityBountaryTmpFilename);

  m_finalProcessors.emplace(finalProcessor);
}

void RawGenerator::GenerateCoasts()
{
  auto processor = CreateProcessor(ProcessorType::Coastline, m_queue);
  m_translators->Append(CreateTranslator(TranslatorType::Coastline, processor, m_cache));

  auto const worldcoastTmpFilename = m_genInfo.GetTmpFileName(WORLD_COASTS_FILE_NAME, DATA_FILE_EXTENSION_TMP);
  auto finalProcessor = std::make_shared<CoastlineFinalProcessor>(worldcoastTmpFilename);

  auto const geomFilename = m_genInfo.GetIntermediateFileName(WORLD_COASTS_FILE_NAME, ".geom");
  finalProcessor->SetCoastlineGeomFilename(geomFilename);

  auto const rawgeomFilename = m_genInfo.GetIntermediateFileName(WORLD_COASTS_FILE_NAME, RAW_GEOM_FILE_EXTENSION);
  finalProcessor->SetCoastlineRawGeomFilename(rawgeomFilename);

  m_finalProcessors.emplace(finalProcessor);
}

void RawGenerator::GenerateRegionFeatures(std::string const & filename)
{
  auto processor = CreateProcessor(ProcessorType::Simple, m_queue, filename);
  m_translators->Append(CreateTranslator(TranslatorType::Regions, processor, m_cache, m_genInfo));
}

void RawGenerator::GenerateStreetsFeatures(std::string const & filename)
{
  auto processor = CreateProcessor(ProcessorType::Simple, m_queue, filename);
  m_translators->Append(CreateTranslator(TranslatorType::Streets, processor, m_cache));
}

void RawGenerator::GenerateGeoObjectsFeatures(std::string const & filename)
{
  auto processor = CreateProcessor(ProcessorType::Simple, m_queue, filename);
  m_translators->Append(CreateTranslator(TranslatorType::GeoObjects, processor, m_cache));
}

bool RawGenerator::GenerateFilteredFeatures(size_t threadsCount)
{
  SourceReader reader = m_genInfo.m_osmFileName.empty() ? SourceReader()
                                                        : SourceReader(m_genInfo.m_osmFileName);
  ProcessorOsmElementsFromO5M o5mProcessor(reader);

  TranslatorsPool translators(m_translators, m_cache, threadsCount - 1);
  RawGeneratorWriter rawGeneratorWriter(m_queue, m_genInfo.m_tmpDir);
  rawGeneratorWriter.Run();

  size_t const kElementsCount = 1024;
  size_t element_pos = 0;
  std::vector<OsmElement> elements(kElementsCount);
  while(o5mProcessor.TryRead(elements[element_pos++]))
  {
    if (element_pos != kElementsCount)
      continue;

    translators.Emit(elements);
    elements = std::vector<OsmElement>(kElementsCount);
    element_pos = 0;
  }

  LOG(LINFO, ("Input was processed."));
  if (!translators.Finish())
    return false;

  LOG(LINFO, ("Names:", rawGeneratorWriter.GetNames()));
  return true;
}

bool RawGenerator::Execute()
{
  if (!GenerateFilteredFeatures(m_threadsCount))
    return false;

  while (!m_finalProcessors.empty())
  {
    base::thread_pool::computational::ThreadPool threadPool(m_threadsCount);
    auto const finalProcessor = m_finalProcessors.top();
    do
    {
      threadPool.Submit([finalProcessor]() {
        finalProcessor->Process();
      }).get();
      m_finalProcessors.pop();
      if (m_finalProcessors.empty() || *finalProcessor != *m_finalProcessors.top())
        break;
    }
    while (true);
  }

  LOG(LINFO, ("Final processing is finished."));
  return true;
}
}  // namespace generator
