#pragma once

#include "indexer/classificator.hpp"
#include "indexer/composite_id.hpp"
#include "indexer/feature_data.hpp"

#include "geometry/point2d.hpp"

#include "coding/csv_reader.hpp"

#include <cstdint>
#include <string>

#include <boost/optional.hpp>

namespace indexer
{
struct HierarchyEntry
{
  indexer::CompositeId m_id;
  boost::optional<indexer::CompositeId> m_parentId;
  size_t m_depth = 0;
  std::string m_name;
  std::string m_countryName;
  m2::PointD m_center;
  uint32_t m_type = ftype::GetEmptyValue();
};

std::string DebugPrint(HierarchyEntry const & line);

namespace popularity
{
uint32_t GetMainType(FeatureParams::Types const & types);
std::string GetName(StringUtf8Multilang const & str);

std::string HierarchyEntryToCsvString(HierarchyEntry const & line, char delimiter=';');
HierarchyEntry HierarchyEntryFromCsvRow(coding::CSVReader::Row const & row);
}  // namespace popularity
}  // namespace indexer
