#include "generator/osm2meta.hpp"

#include "platform/measurement_utils.hpp"

#include "coding/url_encode.hpp"

#include "base/logging.hpp"
#include "base/string_utils.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <unordered_set>

namespace
{

constexpr char const * kOSMMultivalueDelimiter = ";";

template <class T>
void RemoveDuplicatesAndKeepOrder(vector<T> & vec)
{
  std::unordered_set<T> seen;
  auto const predicate = [&seen](T const & value)
  {
    if (seen.find(value) != seen.end())
      return true;
    seen.insert(value);
    return false;
  };
  vec.erase(std::remove_if(vec.begin(), vec.end(), predicate), vec.end());
}

// Also filters out duplicates.
class MultivalueCollector
{
public:
  void operator()(std::string const & value)
  {
    if (value.empty() || value == kOSMMultivalueDelimiter)
      return;
    m_values.push_back(value);
  }
  std::string GetString()
  {
    if (m_values.empty())
      return std::string();

    RemoveDuplicatesAndKeepOrder(m_values);
    return strings::JoinStrings(m_values, kOSMMultivalueDelimiter);
  }
private:
  vector<std::string> m_values;
};

void CollapseMultipleConsecutiveCharsIntoOne(char c, std::string & str)
{
  auto const comparator = [c](char lhs, char rhs) { return lhs == rhs && lhs == c; };
  str.erase(std::unique(str.begin(), str.end(), comparator), str.end());
}
}  // namespace

std::string MetadataTagProcessorImpl::ValidateAndFormat_maxspeed(std::string const & v) const
{
  if (!ftypes::IsSpeedCamChecker::Instance()(m_params.m_Types))
    return std::string();

  return v;
}

std::string MetadataTagProcessorImpl::ValidateAndFormat_stars(std::string const & v) const
{
  if (v.empty())
    return std::string();

  // We are accepting stars from 1 to 7.
  if (v[0] <= '0' || v[0] > '7')
    return std::string();

  // Ignore numbers large then 9.
  if (v.size() > 1 && ::isdigit(v[1]))
    return std::string();

  return std::string(1, v[0]);
}

std::string MetadataTagProcessorImpl::ValidateAndFormat_price_rate(std::string const & v) const
{
  if (v.size() != 1)
    return std::string();

  // Price rate is a single digit from 0 to 5.
  if (v[0] < '0' || v[0] > '5')
    return std::string();

  return v;
}

std::string MetadataTagProcessorImpl::ValidateAndFormat_operator(std::string const & v) const
{
  auto const & isATM = ftypes::IsATMChecker::Instance();
  auto const & isFuelStation = ftypes::IsFuelStationChecker::Instance();

  if (!(isATM(m_params.m_Types) || isFuelStation(m_params.m_Types)))
    return std::string();

  return v;
}

std::string MetadataTagProcessorImpl::ValidateAndFormat_url(std::string const & v) const
{
  return v;
}

std::string MetadataTagProcessorImpl::ValidateAndFormat_phone(std::string const & v) const
{
  return v;
}

std::string MetadataTagProcessorImpl::ValidateAndFormat_opening_hours(std::string const & v) const
{
  return v;
}

std::string MetadataTagProcessorImpl::ValidateAndFormat_ele(std::string const & v) const
{
  return measurement_utils::OSMDistanceToMetersString(v);
}

std::string MetadataTagProcessorImpl::ValidateAndFormat_turn_lanes(std::string const & v) const
{
  return v;
}

std::string MetadataTagProcessorImpl::ValidateAndFormat_turn_lanes_forward(std::string const & v) const
{
  return v;
}

std::string MetadataTagProcessorImpl::ValidateAndFormat_turn_lanes_backward(std::string const & v) const
{
  return v;
}

std::string MetadataTagProcessorImpl::ValidateAndFormat_email(std::string const & v) const
{
  return v;
}

std::string MetadataTagProcessorImpl::ValidateAndFormat_postcode(std::string const & v) const
{
  return v;
}

std::string MetadataTagProcessorImpl::ValidateAndFormat_flats(std::string const & v) const
{
  return v;
}

std::string MetadataTagProcessorImpl::ValidateAndFormat_internet(std::string v) const
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

std::string MetadataTagProcessorImpl::ValidateAndFormat_height(std::string const & v) const
{
  return measurement_utils::OSMDistanceToMetersString(v, false /*supportZeroAndNegativeValues*/, 1);
}

std::string MetadataTagProcessorImpl::ValidateAndFormat_building_levels(std::string v) const
{
  // https://en.wikipedia.org/wiki/List_of_tallest_buildings_in_the_world
  auto constexpr kMaxBuildingLevelsInTheWorld = 167;
  // Some mappers use full width unicode digits. We can handle that.
  strings::NormalizeDigits(v);
  char * stop;
  char const * s = v.c_str();
  double const levels = strtod(s, &stop);
  if (s != stop && std::isfinite(levels) && levels >= 0 && levels <= kMaxBuildingLevelsInTheWorld)
    return strings::to_string_dac(levels, 1);

  return {};
}

std::string MetadataTagProcessorImpl::ValidateAndFormat_sponsored_id(std::string const & v) const
{
  uint64_t id;
  if (!strings::to_uint64(v, id))
    return std::string();
  return v;
}

std::string MetadataTagProcessorImpl::ValidateAndFormat_rating(std::string const & v) const
{
  double rating;
  if (!strings::to_double(v, rating))
    return std::string();
  if (rating > 0 && rating <= 10)
    return strings::to_string_dac(rating, 1);
  return std::string();
}

std::string MetadataTagProcessorImpl::ValidateAndFormat_denomination(std::string const & v) const
{
  return v;
}

std::string MetadataTagProcessorImpl::ValidateAndFormat_cuisine(std::string v) const
{
  strings::MakeLowerCaseInplace(v);
  strings::SimpleTokenizer iter(v, ",;");
  MultivalueCollector collector;
  while (iter) {
    std::string normalized = *iter;
    strings::Trim(normalized, " ");
    CollapseMultipleConsecutiveCharsIntoOne(' ', normalized);
    std::replace(normalized.begin(), normalized.end(), ' ', '_');
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

std::string MetadataTagProcessorImpl::ValidateAndFormat_wikipedia(std::string v) const
{
  strings::Trim(v);
  // Normalize by converting full URL to "lang:title" if necessary
  // (some OSMers do not follow standard for wikipedia tag).
  static std::string const base = ".wikipedia.org/wiki/";
  auto const baseIndex = v.find(base);
  if (baseIndex != std::string::npos)
  {
    auto const baseSize = base.size();
    // Do not allow urls without article name after /wiki/.
    if (v.size() > baseIndex + baseSize)
    {
      auto const slashIndex = v.rfind('/', baseIndex);
      if (slashIndex != std::string::npos && slashIndex + 1 != baseIndex)
      {
        // Normalize article title according to OSM standards.
        std::string title = UrlDecode(v.substr(baseIndex + baseSize));
        std::replace(title.begin(), title.end(), '_', ' ');
        return v.substr(slashIndex + 1, baseIndex - slashIndex - 1) + ":" + title;
      }
    }
    LOG(LDEBUG, ("Invalid Wikipedia tag value:", v));
    return std::string();
  }
  // Standard case: "lang:Article Name With Spaces".
  // Language and article are at least 2 chars each.
  auto const colonIndex = v.find(':');
  if (colonIndex == std::string::npos || colonIndex < 2 || colonIndex + 2 > v.size())
  {
    LOG(LDEBUG, ("Invalid Wikipedia tag value:", v));
    return std::string();
  }
  // Check if it's not a random/invalid link.
  if (v.find("//") != std::string::npos || v.find(".org") != std::string::npos)
  {
    LOG(LDEBUG, ("Invalid Wikipedia tag value:", v));
    return std::string();
  }
  // Normalize to OSM standards.
  std::string normalized(v);
  std::replace(normalized.begin() + colonIndex, normalized.end(), '_', ' ');
  return normalized;
}
