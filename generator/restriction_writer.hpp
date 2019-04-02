#pragma once

#include "generator/intermediate_data.hpp"

#include <fstream>
#include <string>

class RelationElement;

namespace routing
{
class RestrictionWriter
{
public:
  enum class ViaType
  {
    Node,
    Way,
  };

  static std::string const kNodeString;
  static std::string const kWayString;

  static ViaType ConvertFromString(std::string const & str);

  void Open(std::string const & fullPath, generator::cache::IntermediateDataReader * cache);

  void Write(RelationElement const & relationElement);

private:
  bool IsOpened() const;

  std::ofstream m_stream;
  generator::cache::IntermediateDataReader * m_cache = nullptr;
};

std::string DebugPrint(RestrictionWriter::ViaType const & type);

}  // namespace routing

