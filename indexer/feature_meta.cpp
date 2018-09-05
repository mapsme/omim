#include "indexer/feature_meta.hpp"

#include "std/target_os.hpp"

using namespace std;

namespace feature
{
namespace
{
char constexpr const * kBaseWikiUrl =
#ifdef OMIM_OS_MOBILE
    ".m.wikipedia.org/wiki/";
#else
    ".wikipedia.org/wiki/";
#endif
} // namespace

string Metadata::GetWikiURL() const
{
  string v = this->Get(FMD_WIKIPEDIA);
  if (v.empty())
    return v;

  auto const colon = v.find(':');
  if (colon == string::npos)
    return v;

  // Spaces and % sign should be replaced in urls.
  replace(v.begin() + colon, v.end(), ' ', '_');
  string::size_type percent, pos = colon;
  string const escapedPercent("%25");
  while ((percent = v.find('%', pos)) != string::npos)
  {
    v.replace(percent, 1, escapedPercent);
    pos = percent + escapedPercent.size();
  }
  // Trying to avoid redirects by constructing the right link.
  // TODO: Wikipedia article could be opened it a user's language, but need
  // generator level support to check for available article languages first.
  return "https://" + v.substr(0, colon) + kBaseWikiUrl + v.substr(colon + 1);
}

// static
bool Metadata::TypeFromString(string const & k, Metadata::EType & outType)
{
  if (k == "cuisine")
    outType = Metadata::FMD_CUISINE;
  else if (k == "opening_hours")
    outType = Metadata::FMD_OPEN_HOURS;
  else if (k == "phone" || k == "contact:phone")
    outType = Metadata::FMD_PHONE_NUMBER;
  else if (k == "fax" || k == "contact:fax")
    outType = Metadata::EType::FMD_FAX_NUMBER;
  else if (k == "stars")
    outType = Metadata::FMD_STARS;
  else if (k == "operator")
    outType = Metadata::FMD_OPERATOR;
  else if (k == "url")  // TODO: Should we match url to website here?
    outType = Metadata::FMD_WEBSITE;
  else if (k == "website" || k == "contact:website")
    outType = Metadata::FMD_WEBSITE;
  else if (k == "internet_access" || k == "wifi")
    outType = Metadata::FMD_INTERNET;
  else if (k == "ele")
    outType = Metadata::FMD_ELE;
  else if (k == "turn:lanes")
    outType = Metadata::FMD_TURN_LANES;
  else if (k == "turn:lanes:forward")
    outType = Metadata::FMD_TURN_LANES_FORWARD;
  else if (k == "turn:lanes:backward")
    outType = Metadata::FMD_TURN_LANES_BACKWARD;
  else if (k == "email" || k == "contact:email")
    outType = Metadata::FMD_EMAIL;
  else if (k == "addr:postcode")
    outType = Metadata::FMD_POSTCODE;
  else if (k == "wikipedia")
    outType = Metadata::FMD_WIKIPEDIA;
  else if (k == "addr:flats")
    outType = Metadata::FMD_FLATS;
  else if (k == "height")
    outType = Metadata::FMD_HEIGHT;
  else if (k == "min_height")
    outType = Metadata::FMD_MIN_HEIGHT;
  else if (k == "building:levels")
    outType = Metadata::FMD_BUILDING_LEVELS;
  else if (k == "denomination")
    outType = Metadata::FMD_DENOMINATION;
  else if (k == "ref:sponsored")
    outType = Metadata::FMD_SPONSORED_ID;
  else if (k == "price_rate")
    outType = Metadata::FMD_PRICE_RATE;
  else if (k == "rating:sponsored")
    outType = Metadata::FMD_RATING;
  else if (k == "banner_url")
    outType = Metadata::FMD_BANNER_URL;
  else if (k == "level")
    outType = Metadata::FMD_LEVEL;
  else
    return false;

  return true;
}

// static
bool Metadata::IsSponsoredType(Metadata::EType const & type)
{
  switch (type)
  {
  case Metadata::FMD_SPONSORED_ID:
  case Metadata::FMD_PRICE_RATE:
  case Metadata::FMD_RATING:
  case Metadata::FMD_BANNER_URL:
    return true;
  default:
    return false;
  }
}

void RegionData::SetLanguages(vector<string> const & codes)
{
  string value;
  for (string const & code : codes)
  {
    int8_t const lang = StringUtf8Multilang::GetLangIndex(code);
    if (lang != StringUtf8Multilang::kUnsupportedLanguageCode)
      value.push_back(lang);
  }
  MetadataBase::Set(RegionData::Type::RD_LANGUAGES, value);
}

void RegionData::GetLanguages(vector<int8_t> & langs) const
{
  for (auto const lang : Get(RegionData::Type::RD_LANGUAGES))
    langs.push_back(lang);
}

bool RegionData::HasLanguage(int8_t const lang) const
{
  for (auto const lng : Get(RegionData::Type::RD_LANGUAGES))
  {
    if (lng == lang)
      return true;
  }
  return false;
}

bool RegionData::IsSingleLanguage(int8_t const lang) const
{
  string const value = Get(RegionData::Type::RD_LANGUAGES);
  if (value.size() != 1)
    return false;
  return value.front() == lang;
}

void RegionData::AddPublicHoliday(int8_t month, int8_t offset)
{
  string value = Get(RegionData::Type::RD_PUBLIC_HOLIDAYS);
  value.push_back(month);
  value.push_back(offset);
  Set(RegionData::Type::RD_PUBLIC_HOLIDAYS, value);
}
}  // namespace feature

// Warning: exact osm tag keys should be returned for valid enum values.
string ToString(feature::Metadata::EType type)
{
  using feature::Metadata;
  switch (type)
  {
  case Metadata::FMD_CUISINE: return "cuisine";
  case Metadata::FMD_OPEN_HOURS: return "opening_hours";
  case Metadata::FMD_PHONE_NUMBER: return "phone";
  case Metadata::FMD_FAX_NUMBER: return "fax";
  case Metadata::FMD_STARS: return "stars";
  case Metadata::FMD_OPERATOR: return "operator";
  case Metadata::FMD_URL: return "url";
  case Metadata::FMD_WEBSITE: return "website";
  case Metadata::FMD_INTERNET: return "internet_access";
  case Metadata::FMD_ELE: return "ele";
  case Metadata::FMD_TURN_LANES: return "turn:lanes";
  case Metadata::FMD_TURN_LANES_FORWARD: return "turn:lanes:forward";
  case Metadata::FMD_TURN_LANES_BACKWARD: return "turn:lanes:backward";
  case Metadata::FMD_EMAIL: return "email";
  case Metadata::FMD_POSTCODE: return "addr:postcode";
  case Metadata::FMD_WIKIPEDIA: return "wikipedia";
  case Metadata::FMD_FLATS: return "addr:flats";
  case Metadata::FMD_HEIGHT: return "height";
  case Metadata::FMD_MIN_HEIGHT: return "min_height";
  case Metadata::FMD_DENOMINATION: return "denomination";
  case Metadata::FMD_BUILDING_LEVELS: return "building:levels";
  case Metadata::FMD_SPONSORED_ID: return "ref:sponsored";
  case Metadata::FMD_PRICE_RATE: return "price_rate";
  case Metadata::FMD_RATING: return "rating:sponsored";
  case Metadata::FMD_BANNER_URL: return "banner_url";
  case Metadata::FMD_LEVEL: return "level";
  case Metadata::FMD_TEST_ID: return "test_id";
  case Metadata::FMD_COUNT: CHECK(false, ("FMD_COUNT can not be used as a type."));
  };

  return string();
}
