#pragma once

#include "routing/index_graph.hpp"
#include "routing/restrictions_serialization.hpp"

#include "coding/file_container.hpp"

#include "std/string.hpp"
#include "std/unique_ptr.hpp"

class MwmValue;

namespace routing
{
/// \returns if features |r1| and |r2| have a common end returns its joint id.
/// If not, returns Joint::kInvalidId.
/// \note It's possible that the both ends of |r1| and |r2| have common joint ids.
/// In that case returns any of them.
/// \note In general case ends of features don't have to be joints. For example all
/// loose feature ends aren't joints. But if ends of r1 and r2 are connected at this
/// point there has to be a joint. So the method is valid.
Joint::Id GetCommonEndJoint(RoadJointIds const & r1, RoadJointIds const & r2);

class RestrictionLoader
{
public:
  explicit RestrictionLoader(MwmValue const & mwmValue, IndexGraph const & graph);

  bool HasRestrictions() const { return !m_restrictions.empty(); }
  RestrictionVec && StealRestrictions() { return std::move(m_restrictions); }

private:
  unique_ptr<FilesContainerR::TReader> m_reader;
  RestrictionHeader m_header;
  RestrictionVec m_restrictions;
  string const m_countryFileName;
};

void ConvertRestrictionsOnlyToNoAndSort(IndexGraph const & graph,
                                        RestrictionVec & restrictionsOnly,
                                        RestrictionVec & restrictionsNo);
}  // namespace routing
