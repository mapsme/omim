#pragma once

#include "generator/feature_generator.hpp"
#include "generator/final_processor_intermediate_mwm.hpp"
#include "generator/processor_interface.hpp"
#include "generator/generate_info.hpp"
#include "generator/intermediate_data.hpp"
#include "generator/osm_o5m_source.hpp"
#include "generator/osm_xml_source.hpp"
#include "generator/translator_collection.hpp"
#include "generator/translator_interface.hpp"

#include "base/queue.hpp"
#include "base/thread_pool_computational.hpp"

#include <functional>
#include <iostream>
#include <memory>
#include <queue>
#include <sstream>
#include <string>
#include <vector>

struct OsmElement;
class FeatureParams;

namespace generator
{
class SourceReader
{
  struct Deleter
  {
    bool m_needDelete;
    Deleter(bool needDelete = true) : m_needDelete(needDelete) {}
    void operator()(std::istream * s) const
    {
      if (m_needDelete)
        delete s;
    }
  };

  std::unique_ptr<std::istream, Deleter> m_file;

public:
  SourceReader();
  explicit SourceReader(std::string const & filename);
  explicit SourceReader(std::istringstream & stream);

  uint64_t Read(char * buffer, uint64_t bufferSize);
};


// This function is needed to generate intermediate data from OSM elements.
// The translators collection contains translators that translate the OSM element into
// some intermediate representation.
//
// To better understand the generation process of this step,
// we are looking at generation using the example of generation for countries.
//
// To start the generation we make the following calls:
// 1. feature::GenerateInfo genInfo;
// ...
// 2. CacheLoader cacheLoader(genInfo);
// 3. TranslatorCollection translators;
// 4. auto processor = CreateProcessor(ProcessorType::Country, genInfo);
// 5. translators.Append(CreateTranslator(TranslatorType::Country, emitter, cacheLoader.GetCache(), genInfo));
// 6. GenerateRaw(genInfo, translators);
//
// In line 5, we create and add a translator for countries to the translator collection.
// TranslatorCountry is inheritor of Translator.
//
// Translator contains several important entities: FeatureMaker, FilterCollection, CollectorCollection
// and Processor. In short,
// * FeatureMaker - an object that can create FeatureBuilder1 from OSM element,
// * FilterCollection - an object that contains a group of filters that may or may not pass OSM elements
// and FeatureBuilder1s,
// * CollectorCollection - an object that contains a group of collectors that collect additional
// information about OSM elements and FeatureBuilder1s (most often it is information that cannot
// be saved in FeatureBuilder1s from OSM element),
// * Processor - an object that converts an element and saves it.
//
// The most important method is Translator::Emit. Please read it to understand how generation works.
// The order of calls is very important. First, the FilterCollection will filter the OSM elements,
// then call the CollectorCollection for OSM elements, then build the FeatureBuilder1 element
// form OSM element, then FilterCollection will filter the FeatureBuilder1, then call the
// CollectorCollection for the FeatureBuilder1, and then FeatureBuilder1 will fall into the emitter.
//
// TranslatorCountry contains for it specific filters, collectors, emitter and FeatureMaker.
// For example, there are FilterPlanet, which only passes relations with types multipolygon or boundary,
// and CameraNodeProcessor, which collects information about the cameras on the roads.
//
// In line 4, we create emitter for countries.
// The emitter is an important entity that needs to transform FeatureBuilder1 and save them in some way.
// The emitter can filter objects and change the representation of an object based on drawing rules
// and other application rules.
// In ProcessorCountry stages are divided into layers. The layers are connected in a chain.
// For example, there are RepresentationLayer, which may change the presentation of the FeatureBuilder1
// depending on the rules of the application, and BookingLayer, which mixes information from booking.
// You can read a more detailed look into the appropriate class code.
bool GenerateRaw(feature::GenerateInfo & info, TranslatorInterface & translators);
bool GenerateRegionFeatures(feature::GenerateInfo & info);
bool GenerateGeoObjectsFeatures(feature::GenerateInfo & info);

bool GenerateIntermediateData(feature::GenerateInfo & info);

void ProcessOsmElementsFromO5M(SourceReader & stream, std::function<void(OsmElement *)> processor);
void ProcessOsmElementsFromXML(SourceReader & stream, std::function<void(OsmElement *)> processor);

class ProcessorOsmElementsFromO5M
{
public:
  ProcessorOsmElementsFromO5M(SourceReader & stream);
  bool TryRead(OsmElement & element);

private:
  SourceReader & m_stream;
  osm::O5MSource m_dataset;
  osm::O5MSource::Iterator m_pos;
};

class TranslatorsPool
{
public:
  explicit TranslatorsPool(std::shared_ptr<TranslatorCollection> const & original,
                           std::shared_ptr<generator::cache::IntermediateData> const & cache,
                           size_t copyCount);

  void Emit(std::vector<OsmElement> & elements);
  bool Finish();

private:
  std::vector<std::shared_ptr<TranslatorInterface>> m_translators;
  base::thread_pool::computational::ThreadPool m_threadPool;
  base::threads::Queue<base::threads::DataWrapper<size_t>> m_freeTranslators;
};

class RawGeneratorWriter
{
public:
  RawGeneratorWriter(std::shared_ptr<FeatureProcessorQueue> const & queue,
                     std::string const & path);
  ~RawGeneratorWriter();

  void Run();
  std::vector<std::string> GetNames();

private:
  void Write(ProcessedData const & chank);
  void Finish();

  std::thread m_thread;
  std::shared_ptr<FeatureProcessorQueue> m_queue;
  std::string m_path;
  std::unordered_map<std::string, std::unique_ptr<feature::FeaturesCollector>> m_collectors;
};

class RawGenerator
{
public:
  explicit RawGenerator(feature::GenerateInfo & genInfo, size_t threadsCount);

  void GenerateCountries(bool disableAds);
  void GenerateWorld(bool disableAds);
  void GenerateCoasts();
  void GenerateRegionFeatures(std::string const & filename);
  void GenerateStreetsFeatures(std::string const & filename);
  void GenerateGeoObjectsFeatures(std::string const & filename);

  bool Execute();

private:
  using FinalProcessorPtr = std::shared_ptr<FinalProcessorIntermediateMwmInteface>;
  struct FinalProcessorPtrCmp
  {
    bool operator()(FinalProcessorPtr const & l, FinalProcessorPtr const & r)
    {
      return *l < *r;
    }
  };

  bool GenerateFilteredFeatures(size_t threadsCount);

  feature::GenerateInfo & m_genInfo;
  size_t m_threadsCount;
  std::shared_ptr<generator::cache::IntermediateData> m_cache;
  std::shared_ptr<FeatureProcessorQueue> m_queue;
  std::shared_ptr<TranslatorCollection> m_translators;
  std::priority_queue<FinalProcessorPtr, std::vector<FinalProcessorPtr>, FinalProcessorPtrCmp> m_finalProcessors;
};
}  // namespace generator
