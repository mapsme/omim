#include "indexer/complex/hierarchy_entry.hpp"

#include "indexer/feature_utils.hpp"
#include "indexer/ftypes_matcher.hpp"

#include "coding/string_utf8_multilang.hpp"

#include "base/assert.hpp"
#include "base/string_utils.hpp"
#include "base/stl_helpers.hpp"

#include <algorithm>
#include <sstream>

namespace
{
// GetRussianName returns a russian feature name if it's possible.
// Otherwise, GetRussianName function returns a name that GetReadableName returns.
std::string GetRussianName(StringUtf8Multilang const & str)
{
  auto const deviceLang = StringUtf8Multilang::GetLangIndex("ru");
  std::string result;
  feature::GetReadableName({} /* regionData */, str, deviceLang, false /* allowTranslit */, result);
  for (auto const & ch : {';', '\n', '\t'})
    std::replace(std::begin(result), std::end(result), ch, ',');
  return result;
}
}  // namespace

namespace indexer
{
std::string DebugPrint(HierarchyEntry const & line)
{
  std::stringstream stream;
  stream << std::fixed << std::setprecision(7);
  stream << DebugPrint(line.m_id) << ';';
  if (line.m_parentId)
    stream << DebugPrint(*line.m_parentId);
  stream << ';';
  stream << line.m_depth << ';';
  stream << line.m_center.x << ';';
  stream << line.m_center.y << ';';
  stream << classif().GetReadableObjectName(line.m_type) << ';';
  stream << line.m_name << ';';
  stream << line.m_countryName;
  return stream.str();
}
namespace popularity
{
uint32_t GetMainType(FeatureParams::Types const & types)
{
  auto const & airportChecker = ftypes::IsAirportChecker::Instance();
  auto it = base::FindIf(types, airportChecker);
  if (it != std::cend(types))
    return *it;

  auto const & attractChecker = ftypes::AttractionsChecker::Instance();
  auto const type = attractChecker.GetBestType(types);
  if (type != ftype::GetEmptyValue())
    return type;

  auto const & eatChecker = ftypes::IsEatChecker::Instance();
  it = base::FindIf(types, eatChecker);
  return it != std::cend(types) ? *it : ftype::GetEmptyValue();
}

std::string GetName(StringUtf8Multilang const & str) { return GetRussianName(str); }

std::string HierarchyEntryToCsvString(HierarchyEntry const & line, char delimiter)
{
  std::stringstream stream;
  stream << std::fixed << std::setprecision(7);
  stream << line.m_id.ToString() << delimiter;
  if (line.m_parentId)
  {
    auto const parentId = *line.m_parentId;
    stream << parentId.ToString();
  }
  stream << delimiter;
  stream << line.m_depth << delimiter;
  stream << line.m_center.x << delimiter;
  stream << line.m_center.y << delimiter;
  stream << classif().GetReadableObjectName(line.m_type) << delimiter;
  stream << line.m_name << delimiter;
  stream << line.m_countryName;
  return stream.str();
}

HierarchyEntry HierarchyEntryFromCsvRow(coding::CSVReader::Row const & row)
{
  CHECK_EQUAL(row.size(), 8, (row));

  auto const & id = row[0];
  auto const & parentId = row[1];
  auto const & depth = row[2];
  auto const & x = row[3];
  auto const & y = row[4];
  auto const & type = row[5];
  auto const & name = row[6];
  auto const & country = row[7];

  HierarchyEntry entry;
  entry.m_id = CompositeId(id);
  if (!parentId.empty())
    entry.m_parentId = CompositeId(parentId);

  CHECK(strings::to_size_t(depth, entry.m_depth), (row));
  CHECK(strings::to_double(x, entry.m_center.x), (row));
  CHECK(strings::to_double(y, entry.m_center.y), (row));
  entry.m_type = classif().GetTypeByReadableObjectName(type);
  entry.m_name = name;
  entry.m_countryName = country;
  return entry;
}
}  // namespace popularity
}  // namespace indexer
