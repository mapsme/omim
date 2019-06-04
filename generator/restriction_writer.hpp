#pragma once

#include "generator/collector_interface.hpp"
#include "generator/intermediate_data.hpp"

#include <sstream>
#include <string>

class RelationElement;

namespace routing
{
class RestrictionWriter : public generator::CollectorInterface
{
public:

  enum class ViaType
  {
    Node,
    Way,

    Count,
  };

  static std::string const kNodeString;
  static std::string const kWayString;

  RestrictionWriter(std::string const & filename,
                    generator::cache::IntermediateDataReader const & cache);

  // generator::CollectorInterface overrides:
  void CollectRelation(RelationElement const & relationElement) override;
  void Save() override;

  void Merge(generator::CollectorInterface const * collector) override;
  void MergeInto(RestrictionWriter * collector) const override;

  static ViaType ConvertFromString(std::string const & str);

private:
  std::stringstream m_stream;
  generator::cache::IntermediateDataReader const & m_cache;
};

std::string DebugPrint(RestrictionWriter::ViaType const & type);
}  // namespace routing

