#pragma once

#include "generator/feature_builder.hpp"

#include "base/assert.hpp"
#include "base/queue.hpp"

#include <memory>
#include <string>
#include <vector>

class FeatureParams;

namespace generator
{
class ProcessorCoastline;
class ProcessorCountry;
class ProcessorNoop;
class ProcessorRestaurants;
class ProcessorSimple;
class ProcessorWorld;

// Implementing this interface allows an object to process FeatureBuilder1 objects and broadcast them.
class FeatureProcessorInterface
{
public:
  virtual ~FeatureProcessorInterface() = default;

  virtual std::shared_ptr<FeatureProcessorInterface> Clone() const = 0;

  // This method is used by OsmTranslator to pass |fb| to Processor for further processing.
  virtual void Process(FeatureBuilder1 & fb) = 0;
  // Finish is used in GenerateFeatureImpl to make whatever work is needed after all OsmElements
  // are processed.
  virtual bool Finish() = 0;

  virtual void Merge(FeatureProcessorInterface const *) = 0;

  virtual void MergeInto(ProcessorCoastline *) const { FailIfMethodUnsuppirted(); }
  virtual void MergeInto(ProcessorCountry *) const { FailIfMethodUnsuppirted(); }
  virtual void MergeInto(ProcessorNoop *) const { FailIfMethodUnsuppirted(); }
  virtual void MergeInto(ProcessorRestaurants *) const { FailIfMethodUnsuppirted(); }
  virtual void MergeInto(ProcessorSimple *) const { FailIfMethodUnsuppirted(); }
  virtual void MergeInto(ProcessorWorld *) const { FailIfMethodUnsuppirted(); }

private:
  void FailIfMethodUnsuppirted() const { CHECK(false, ("This method is unsupported.")); }
};

struct ProcessedData
{
  FeatureBuilder1 m_fb;
  std::vector<std::string> m_affiliations;
};

using FeatureProcessorChank = base::threads::DataWrapper<ProcessedData>;
using FeatureProcessorQueue = base::threads::Queue<FeatureProcessorChank>;

}  // namespace generator
