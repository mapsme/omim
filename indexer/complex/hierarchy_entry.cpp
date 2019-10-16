#include "indexer/complex/hierarchy_entry.hpp"

#include "indexer/feature_utils.hpp"
#include "indexer/ftypes_matcher.hpp"

#include "coding/string_utf8_multilang.hpp"

#include "base/assert.hpp"
#include "base/string_utils.hpp"
#include "base/stl_helpers.hpp"

#include <algorithm>
#include <sstream>
#include <tuple>

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
bool operator==(HierarchyEntry const & lhs, HierarchyEntry const & rhs)
{
  return base::AlmostEqualAbs(lhs.m_center, rhs.m_center, 1e-7) &&
      (std::tie(lhs.m_id, lhs.m_parentId, lhs.m_depth, lhs.m_name, lhs.m_countryName, lhs.m_type) ==
       std::tie(rhs.m_id, rhs.m_parentId, rhs.m_depth, rhs.m_name, rhs.m_countryName, rhs.m_type));
}

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
namespace hierarchy
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

coding::CSVReader::Row HierarchyEntryToCsvRow(HierarchyEntry const & line)
{
  coding::CSVReader::Row row;
  row.emplace_back(line.m_id.ToString());
  std::string parentId;
  if (line.m_parentId)
    parentId = (*line.m_parentId).ToString();

  row.emplace_back(parentId);
  row.emplace_back(strings::to_string(line.m_depth));
  row.emplace_back(strings::to_string_dac(line.m_center.x, 7));
  row.emplace_back(strings::to_string_dac(line.m_center.y, 7));
  row.emplace_back(strings::to_string(classif().GetReadableObjectName(line.m_type)));
  row.emplace_back(strings::to_string(line.m_name));
  row.emplace_back(strings::to_string(line.m_countryName));
  return row;
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

std::string HierarchyEntryToCsvString(HierarchyEntry const & line, char delim)
{
  return strings::JoinStrings(HierarchyEntryToCsvRow(line), delim);
}

tree_node::types::PtrList<HierarchyEntry> LoadHierachy(std::string const & filename)
{
  std::unordered_map<indexer::CompositeId, tree_node::types::Ptr<indexer::HierarchyEntry>> nodes;
  for (auto const & row : coding::CSVRunner(
         coding::CSVReader(filename, false /* header */, hierarchy::kCsvDelimiter)))
  {
    auto entry = hierarchy::HierarchyEntryFromCsvRow(row);
    auto const id = entry.m_id;
    nodes.emplace(id, tree_node::MakeTreeNode(std::move(entry)));
  }
  for (auto const & pair : nodes)
  {
    auto const & node = pair.second;
    auto const parentIdOpt = node->GetData().m_parentId;
    if (parentIdOpt)
    {
      auto const it = nodes.find(*parentIdOpt);
      CHECK(it != std::cend(nodes), (*it));
      tree_node::Link(node, it->second);
    }
  }
  std::vector<tree_node::types::Ptr<indexer::HierarchyEntry>> trees;
  base::Transform(nodes, std::back_inserter(trees), base::RetrieveSecond());
  base::EraseIf(trees, [](auto const & node) { return node->HasParent(); });
  return trees;
}
}  // namespace hierarchy
}  // namespace indexer
