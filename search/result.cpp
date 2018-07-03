#include "search/result.hpp"

#include "search/common.hpp"
#include "search/geometry_utils.hpp"

#include "base/string_utils.hpp"

#include <sstream>

using namespace std;

namespace search
{
namespace
{
// Following methods join only non-empty arguments in order with
// commas.
string Join(string const & s)
{
  return s;
}

template <typename... Args>
string Join(string const & s, Args &&... args)
{
  auto const tail = Join(forward<Args>(args)...);
  if (s.empty())
    return tail;
  if (tail.empty())
    return s;
  return s + ", " + tail;
}
}  // namespace

// Result ------------------------------------------------------------------------------------------
Result::Result(FeatureID const & id, m2::PointD const & pt, string const & str,
               string const & address, string const & featureTypeName, uint32_t featureType,
               Metadata const & meta)
  : m_resultType(Type::Feature)
  , m_id(id)
  , m_center(pt)
  , m_str(str.empty() ? featureTypeName : str)
  , m_address(address)
  , m_featureTypeName(featureTypeName)
  , m_featureType(featureType)
  , m_metadata(meta)
{
}

Result::Result(m2::PointD const & pt, string const & latlon, string const & address)
  : m_resultType(Type::LatLon), m_center(pt), m_str(latlon), m_address(address)
{
}

Result::Result(string const & str, string const & suggest)
  : m_resultType(Type::PureSuggest), m_str(str), m_suggestionStr(suggest)
{
}

Result::Result(Result const & res, string const & suggest)
  : m_id(res.m_id)
  , m_center(res.m_center)
  , m_str(res.m_str)
  , m_address(res.m_address)
  , m_featureTypeName(res.m_featureTypeName)
  , m_featureType(res.m_featureType)
  , m_suggestionStr(suggest)
  , m_hightlightRanges(res.m_hightlightRanges)
{
  m_resultType = m_id.IsValid() ? Type::SuggestFromFeature : Type::PureSuggest;
}

bool Result::IsSuggest() const
{
  return m_resultType == Type::SuggestFromFeature || m_resultType == Type::PureSuggest;
}

bool Result::HasPoint() const
{
  return m_resultType != Type::PureSuggest;
}

FeatureID const & Result::GetFeatureID() const
{
  ASSERT(m_resultType == Type::Feature, (m_resultType));
  return m_id;
}

m2::PointD Result::GetFeatureCenter() const
{
  ASSERT(HasPoint(), ());
  return m_center;
}

string const & Result::GetSuggestionString() const
{
  ASSERT(IsSuggest(), ());
  return m_suggestionStr;
}

bool Result::IsEqualSuggest(Result const & r) const
{
  return m_suggestionStr == r.m_suggestionStr;
}

bool Result::IsEqualFeature(Result const & r) const
{
  if (m_resultType != r.m_resultType)
    return false;

  ASSERT_EQUAL(m_resultType, Result::Type::Feature, ());

  ASSERT(m_id.IsValid() && r.m_id.IsValid(), ());
  if (m_id == r.m_id)
    return true;

  // This function is used to filter duplicate results in cases:
  // - emitted World.mwm and Country.mwm
  // - after additional search in all mwm
  // so it's suitable here to test for 500m
  return m_str == r.m_str && m_address == r.m_address && m_featureType == r.m_featureType &&
         PointDistance(m_center, r.m_center) < 500.0;
}

void Result::AddHighlightRange(pair<uint16_t, uint16_t> const & range)
{
  m_hightlightRanges.push_back(range);
}

pair<uint16_t, uint16_t> const & Result::GetHighlightRange(size_t idx) const
{
  ASSERT(idx < m_hightlightRanges.size(), ());
  return m_hightlightRanges[idx];
}

void Result::PrependCity(string const & name)
{
  // It is expected that if |m_address| is not empty,
  // it starts with the region name. Avoid duplication
  // in the case where this region name coincides with
  // the city name and prepend otherwise.
  strings::SimpleTokenizer tok(m_address, ",");
  if (tok && *tok != name)
    m_address = Join(name, m_address);
}

string Result::ToStringForStats() const
{
  string s;
  s.append(GetString());
  s.append("|");
  s.append(GetFeatureTypeName());
  s.append("|");
  s.append(IsSuggest() ? "1" : "0");
  return s;
}

string DebugPrint(Result::Type type)
{
  switch (type)
  {
  case Result::Type::Feature: return "Feature";
  case Result::Type::LatLon: return "LatLon";
  case Result::Type::PureSuggest: return "PureSuggest";
  case Result::Type::SuggestFromFeature: return "SuggestFromFeature";
  }

  return "Unknown";
}

string DebugPrint(Result const & result)
{
  ostringstream os;
  os << "Result [";
  os << "name: " << result.GetString() << ", ";
  os << "type: " << result.GetFeatureTypeName() << ", ";
  os << "info: " << DebugPrint(result.GetRankingInfo());
  os << "]";
  return os.str();
}

// Results -----------------------------------------------------------------------------------------
Results::Results()
{
  Clear();
}

bool Results::AddResult(Result && result)
{
  // Find first feature result.
  auto it = find_if(m_results.begin(), m_results.end(),
                    [](Result const & r) { return r.GetResultType() == Result::Type::Feature; });

  if (result.IsSuggest())
  {
    auto const d = distance(m_results.begin(), it);
    if (d >= static_cast<decltype(d)>(kMaxNumSuggests))
      return false;

    for (auto i = m_results.begin(); i != it; ++i)
      if (result.IsEqualSuggest(*i))
        return false;
    InsertResult(it, move(result));
  }
  else
  {
    for (; it != m_results.end(); ++it)
    {
      if (result.IsEqualFeature(*it))
        return false;
    }
    InsertResult(m_results.end(), move(result));
  }

  return true;
}

void Results::AddResultNoChecks(Result && result)
{
  InsertResult(m_results.end(), move(result));
}

void Results::AddResultsNoChecks(ConstIter first, ConstIter last)
{
  while (first != last)
  {
    auto resultCopy = *first++;
    AddResultNoChecks(move(resultCopy));
  }
}

void Results::AddBookmarkResult(bookmarks::Result const & result)
{
  m_bookmarksResults.push_back(result);
}

void Results::Clear()
{
  m_results.clear();
  m_bookmarksResults.clear();
  m_status = Status::None;
  m_hotelsClassif.Clear();
}

size_t Results::GetSuggestsCount() const
{
  size_t i = 0;

  // Suggests always go first, so we need to compute length of prefix
  // of suggests.
  while (i < m_results.size() && m_results[i].IsSuggest())
    ++i;
  return i;
}

bookmarks::Results const & Results::GetBookmarksResults() const
{
  return m_bookmarksResults;
}

void Results::InsertResult(vector<Result>::iterator where, Result && result)
{
  ASSERT_LESS(m_results.size(), numeric_limits<int32_t>::max(), ());

  for (auto it = where; it != m_results.end(); ++it)
  {
    auto & r = *it;
    auto const position = r.GetPositionInResults();
    r.SetPositionInResults(position + 1);
  }

  result.SetPositionInResults(static_cast<int32_t>(distance(m_results.begin(), where)));
  m_hotelsClassif.Add(result);
  m_results.insert(where, move(result));
}

string DebugPrint(search::Results const & results)
{
  return ::my::impl::DebugPrintSequence(results.begin(), results.end());
}

// AddressInfo -------------------------------------------------------------------------------------
bool AddressInfo::IsEmptyName() const
{
  return m_name.empty() && m_house.empty();
}

string AddressInfo::GetPinName() const
{
  if (IsEmptyName() && !m_types.empty())
    return m_types[0];
  return m_name.empty() ? m_house : m_name;
}

string AddressInfo::GetPinType() const
{
  return GetBestType();
}

string AddressInfo::FormatPinText() const
{
  // select name or house if name is empty
  string const & ret = (m_name.empty() ? m_house : m_name);

  string const type = GetBestType();
  if (type.empty())
    return ret;

  return ret.empty() ? type : (ret + " (" + type + ')');
}

string AddressInfo::FormatHouseAndStreet(Type type /* = Type::Default */) const
{
  // Check whether we can format address according to the query type
  // and actual address distance.

  // TODO (@m, @y): we can add "Near" prefix here in future according
  // to the distance.
  if (m_distanceMeters > 0.0)
  {
    if (type == Type::SearchResult && m_distanceMeters > 50.0)
      return {};
    if (m_distanceMeters > 200.0)
      return {};
  }

  return Join(m_street, m_house);
}

string AddressInfo::FormatAddress(Type type /* = Type::Default */) const
{
  return Join(FormatHouseAndStreet(type), m_city, m_country);
}

string AddressInfo::FormatNameAndAddress(Type type /* = Type::Default */) const
{
  return Join(m_name, FormatAddress(type));
}

string AddressInfo::FormatTypes() const
{
  string result;
  for (size_t i = 0; i < m_types.size(); ++i)
  {
    ASSERT(!m_types.empty(), ());
    if (!result.empty())
      result += ' ';
    result += m_types[i];
  }
  return result;
}

string AddressInfo::GetBestType() const
{
  if (m_types.empty())
    return {};

  /// @TODO(@m, @y): probably, we should skip some "common" types here
  /// like in TypesHolder::SortBySpec.
  ASSERT(!m_types[0].empty(), ());
  return m_types[0];
}

void AddressInfo::Clear()
{
  m_country.clear();
  m_city.clear();
  m_street.clear();
  m_house.clear();
  m_name.clear();
  m_types.clear();
}

string DebugPrint(AddressInfo const & info)
{
  return info.FormatNameAndAddress();
}
}  // namespace search
