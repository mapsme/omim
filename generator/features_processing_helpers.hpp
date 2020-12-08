#pragma once

#include "generator/affiliation.hpp"
#include "generator/feature_builder.hpp"

#include "base/thread_safe_queue.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace generator
{
size_t static const kAffiliationsBufferSize = 512;

struct ProcessedData
{
  explicit ProcessedData(feature::FeatureBuilder::Buffer && buffer,
                         std::vector<feature::CountryPolygonsPtr> && affiliations)
    : m_buffer(std::move(buffer)), m_affiliations(std::move(affiliations))
  {
  }

  feature::FeatureBuilder::Buffer m_buffer;
  std::vector<feature::CountryPolygonsPtr> m_affiliations;
};

using FeatureProcessorChunk = std::optional<std::vector<ProcessedData>>;
using FeatureProcessorQueue = base::threads::ThreadSafeQueue<FeatureProcessorChunk>;
}  // namespace generator
