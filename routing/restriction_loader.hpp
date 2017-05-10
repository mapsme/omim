#pragma once

#include "routing/index_graph.hpp"
#include "routing/restrictions_serialization.hpp"

#include "indexer/index.hpp"

#include "coding/file_container.hpp"

#include <string>
#include <memory>

namespace routing
{
class RestrictionLoader
{
public:
  explicit RestrictionLoader(MwmValue const & mwmValue, IndexGraph const & graph);

  bool HasRestrictions() const { return !m_restrictions.empty(); }
  RestrictionVec && StealRestrictions() { return move(m_restrictions); }

private:
  std::unique_ptr<FilesContainerR::TReader> m_reader;
  RestrictionHeader m_header;
  RestrictionVec m_restrictions;
  std::string const m_countryFileName;
};

void ConvertRestrictionsOnlyToNoAndSort(IndexGraph const & graph,
                                        RestrictionVec const & restrictionsOnly,
                                        RestrictionVec & restrictionsNo);
}  // namespace routing
