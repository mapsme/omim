#include "generator/osm2meta.hpp"

#include "platform/measurement_utils.hpp"

#include "coding/url_encode.hpp"

#include "routing/routing_helpers.hpp"

#include "base/logging.hpp"
#include "base/string_utils.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <unordered_set>

using namespace std;

namespace
{

constexpr char const * kOSMMultivalueDelimiter = ";";

// https://en.wikipedia.org/wiki/List_of_tallest_buildings_in_the_world
auto constexpr kMaxBuildingLevelsInTheWorld = 167;
auto constexpr kMinBuildingLevel = -6;

std::unordered_map<std::string, std::string> const kHumanSpeedToNumber =
{
  {"AT:urban", "50"},
  {"AT:rural", "100"},
  {"AT:trunk", "100"},
  {"AT:motorway", "130"},
  {"BE:urban", "50"},
  {"BE:zone", "30"},
  {"BE:motorway", "120"},
  {"BE:zone30", "30"},
  {"BE:rural", "70"},
  {"BE:school", "30"},
  {"CZ:motorway", "130"},
  {"CZ:trunk", "110"},
  {"CZ:rural", "90"},
  {"CZ:urban_motorway", "80"},
  {"CZ:urban_trunk", "80"},
  {"CZ:urban", "50"},
  {"DE:rural", "100"},
  {"DE:urban", "50"},
  {"DE:bicycle_road", "30"},
  {"FR:motorway", "130"},
  {"FR:rural", "80"},
  {"FR:urban", "50"},
  {"HU:living_street", "20"},
  {"HU:motorway", "130"},
  {"HU:rural", "90"},
  {"HU:trunk", "110"},
  {"HU:urban", "50"},
  {"IT:rural", "90"},
  {"IT:motorway", "130"},
  {"IT:urban", "50"},
  {"JP:nsl", "60"},
  {"JP:express", "100"},
  {"LT:rural", "90"},
  {"LT:urban", "50"},
  {"NO:rural", "80"},
  {"NO:urban", "50"},
  {"ON:urban", "50"},
  {"ON:rural", "80"},
  {"PT:motorway", "120"},
  {"PT:rural", "90"},
  {"PT:trunk", "100"},
  {"PT:urban", "50"},
  {"RO:motorway", "130"},
  {"RO:rural", "90"},
  {"RO:trunk", "100"},
  {"RO:urban", "50"},
  {"RU:living_street", "20"},
  {"RU:urban", "60"},
  {"RU:rural", "90"},
  {"RU:motorway", "110"},

  {"GB:motorway", "112"},  // 70 mph = 112.65408 kmph
  {"GB:nsl_dual", "112"},  // 70 mph = 112.65408 kmph
  {"GB:nsl_single", "96"}, // 60 mph = 96.56064 kmph

  {"UK:motorway", "112"},  // 70 mph
  {"UK:nsl_dual", "112"},  // 70 mph
  {"UK:nsl_single", "96"}, // 60 mph

  {"UZ:living_street", "30"},
  {"UZ:urban", "70"},
  {"UZ:rural", "100"},
  {"UZ:motorway", "110"},
};

template <class T>
void RemoveDuplicatesAndKeepOrder(vector<T> & vec)
{
  unordered_set<T> seen;
  auto const predicate = [&seen](T const & value)
  {
    if (seen.find(value) != seen.end())
      return true;
    seen.insert(value);
    return false;
  };
  vec.erase(remove_if(vec.begin(), vec.end(), predicate), vec.end());
}

// Also filters out duplicates.
class MultivalueCollector
{
public:
  void operator()(string const & value)
  {
    if (value.empty() || value == kOSMMultivalueDelimiter)
      return;
    m_values.push_back(value);
  }
  string GetString()
  {
    if (m_values.empty())
      return string();

    RemoveDuplicatesAndKeepOrder(m_values);
    return strings::JoinStrings(m_values, kOSMMultivalueDelimiter);
  }
private:
  vector<string> m_values;
};

void CollapseMultipleConsecutiveCharsIntoOne(char c, string & str)
{
  auto const comparator = [c](char lhs, char rhs) { return lhs == rhs && lhs == c; };
  str.erase(unique(str.begin(), str.end(), comparator), str.end());
}
}  // namespace

string MetadataTagProcessorImpl::ValidateAndFormat_maxspeed(string const & v) const
{
  if (!ftypes::IsSpeedCamChecker::Instance()(m_params.m_types))
    return string();

  auto const & it = kHumanSpeedToNumber.find(v);
  if (it != kHumanSpeedToNumber.cend())
    return it->second;

  string result{};

  int i;
  for (i = 0; i < v.size(); ++i)
  {
    if (isdigit(v[i]))
      result += v[i];
    else
      break;
  }

  while (i < v.size() && isspace(v[i++])) {}

  if (strings::StartsWith(string(v.begin() + i, v.end()), "kmh"))
  {
    return result;
  }
  else if (strings::StartsWith(string(v.begin() + i, v.end()), "mph"))
  {
    int32_t mph;
    if (!strings::to_int(result.c_str(), mph))
      return string();

    mph = static_cast<int32_t>(routing::KMPH2MilesPH(mph));
    return strings::to_string(mph);
  }
  else
  {
    return result;
  }

  return v;
}

string MetadataTagProcessorImpl::ValidateAndFormat_stars(string const & v) const
{
  if (v.empty())
    return string();

  // We are accepting stars from 1 to 7.
  if (v[0] <= '0' || v[0] > '7')
    return string();

  // Ignore numbers large then 9.
  if (v.size() > 1 && ::isdigit(v[1]))
    return string();

  return string(1, v[0]);
}

string MetadataTagProcessorImpl::ValidateAndFormat_price_rate(string const & v) const
{
  if (v.size() != 1)
    return string();

  // Price rate is a single digit from 0 to 5.
  if (v[0] < '0' || v[0] > '5')
    return string();

  return v;
}

string MetadataTagProcessorImpl::ValidateAndFormat_operator(string const & v) const
{
  auto const & isATM = ftypes::IsATMChecker::Instance();
  auto const & isFuelStation = ftypes::IsFuelStationChecker::Instance();

  if (!(isATM(m_params.m_types) || isFuelStation(m_params.m_types)))
    return string();

  return v;
}

string MetadataTagProcessorImpl::ValidateAndFormat_url(string const & v) const
{
  return v;
}

string MetadataTagProcessorImpl::ValidateAndFormat_phone(string const & v) const
{
  return v;
}

string MetadataTagProcessorImpl::ValidateAndFormat_opening_hours(string const & v) const
{
  return v;
}

string MetadataTagProcessorImpl::ValidateAndFormat_ele(string const & v) const
{
  return measurement_utils::OSMDistanceToMetersString(v);
}

string MetadataTagProcessorImpl::ValidateAndFormat_turn_lanes(string const & v) const
{
  return v;
}

string MetadataTagProcessorImpl::ValidateAndFormat_turn_lanes_forward(string const & v) const
{
  return v;
}

string MetadataTagProcessorImpl::ValidateAndFormat_turn_lanes_backward(string const & v) const
{
  return v;
}

string MetadataTagProcessorImpl::ValidateAndFormat_email(string const & v) const
{
  return v;
}

string MetadataTagProcessorImpl::ValidateAndFormat_postcode(string const & v) const
{
  return v;
}

string MetadataTagProcessorImpl::ValidateAndFormat_flats(string const & v) const
{
  return v;
}

string MetadataTagProcessorImpl::ValidateAndFormat_internet(string v) const
{
  // TODO(AlexZ): Reuse/synchronize this code with MapObject::SetInternet().
  strings::AsciiToLower(v);
  if (v == "wlan" || v == "wired" || v == "yes" || v == "no")
    return v;
  // Process wifi=free tag.
  if (v == "free")
    return "wlan";
  return {};
}

string MetadataTagProcessorImpl::ValidateAndFormat_height(string const & v) const
{
  return measurement_utils::OSMDistanceToMetersString(v, false /*supportZeroAndNegativeValues*/, 1);
}

string MetadataTagProcessorImpl::ValidateAndFormat_building_levels(string v) const
{
  // Some mappers use full width unicode digits. We can handle that.
  strings::NormalizeDigits(v);
  char * stop;
  char const * s = v.c_str();
  double const levels = strtod(s, &stop);
  if (s != stop && isfinite(levels) && levels >= 0 && levels <= kMaxBuildingLevelsInTheWorld)
    return strings::to_string_dac(levels, 1);

  return {};
}

string MetadataTagProcessorImpl::ValidateAndFormat_level(string v) const
{
  // Some mappers use full width unicode digits. We can handle that.
  strings::NormalizeDigits(v);
  char * stop;
  char const * s = v.c_str();
  double const levels = strtod(s, &stop);
  if (s != stop && isfinite(levels) && levels >= kMinBuildingLevel &&
      levels <= kMaxBuildingLevelsInTheWorld)
  {
    return strings::to_string(levels);
  }

  return {};
}

string MetadataTagProcessorImpl::ValidateAndFormat_sponsored_id(string const & v) const
{
  uint64_t id;
  if (!strings::to_uint64(v, id))
    return string();
  return v;
}

string MetadataTagProcessorImpl::ValidateAndFormat_rating(string const & v) const
{
  double rating;
  if (!strings::to_double(v, rating))
    return string();
  if (rating > 0 && rating <= 10)
    return strings::to_string_dac(rating, 1);
  return string();
}

string MetadataTagProcessorImpl::ValidateAndFormat_denomination(string const & v) const
{
  return v;
}

string MetadataTagProcessorImpl::ValidateAndFormat_cuisine(string v) const
{
  strings::MakeLowerCaseInplace(v);
  strings::SimpleTokenizer iter(v, ",;");
  MultivalueCollector collector;
  while (iter) {
    string normalized = *iter;
    strings::Trim(normalized, " ");
    CollapseMultipleConsecutiveCharsIntoOne(' ', normalized);
    replace(normalized.begin(), normalized.end(), ' ', '_');
    // Avoid duplication for some cuisines.
    if (normalized == "bbq" || normalized == "barbeque")
      normalized = "barbecue";
    if (normalized == "doughnut")
      normalized = "donut";
    if (normalized == "steak")
      normalized = "steak_house";
    if (normalized == "coffee")
      normalized = "coffee_shop";
    collector(normalized);
    ++iter;
  }
  return collector.GetString();
}

string MetadataTagProcessorImpl::ValidateAndFormat_wikipedia(string v) const
{
  strings::Trim(v);
  // Normalize by converting full URL to "lang:title" if necessary
  // (some OSMers do not follow standard for wikipedia tag).
  static string const base = ".wikipedia.org/wiki/";
  auto const baseIndex = v.find(base);
  if (baseIndex != string::npos)
  {
    auto const baseSize = base.size();
    // Do not allow urls without article name after /wiki/.
    if (v.size() > baseIndex + baseSize)
    {
      auto const slashIndex = v.rfind('/', baseIndex);
      if (slashIndex != string::npos && slashIndex + 1 != baseIndex)
      {
        // Normalize article title according to OSM standards.
        string title = UrlDecode(v.substr(baseIndex + baseSize));
        replace(title.begin(), title.end(), '_', ' ');
        return v.substr(slashIndex + 1, baseIndex - slashIndex - 1) + ":" + title;
      }
    }
    LOG(LDEBUG, ("Invalid Wikipedia tag value:", v));
    return string();
  }
  // Standard case: "lang:Article Name With Spaces".
  // Language and article are at least 2 chars each.
  auto const colonIndex = v.find(':');
  if (colonIndex == string::npos || colonIndex < 2 || colonIndex + 2 > v.size())
  {
    LOG(LDEBUG, ("Invalid Wikipedia tag value:", v));
    return string();
  }
  // Check if it's not a random/invalid link.
  if (v.find("//") != string::npos || v.find(".org") != string::npos)
  {
    LOG(LDEBUG, ("Invalid Wikipedia tag value:", v));
    return string();
  }
  // Normalize to OSM standards.
  string normalized(v);
  replace(normalized.begin() + colonIndex, normalized.end(), '_', ' ');
  return normalized;
}
