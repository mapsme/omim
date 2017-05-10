#include "indexer/editable_map_object.hpp"
#include "indexer/classificator.hpp"
#include "indexer/cuisines.hpp"
#include "indexer/osm_editor.hpp"
#include "indexer/postcodes_matcher.hpp"

#include "base/macros.hpp"
#include "base/string_utils.hpp"

#include <cctype>
#include <cmath>
#include <regex>
#include <sstream>

namespace
{
size_t const kFakeNamesCount = 2;

bool ExtractName(StringUtf8Multilang const & names, int8_t const langCode,
                 std::vector<osm::LocalizedName> & result)
{
  if (StringUtf8Multilang::kUnsupportedLanguageCode == langCode ||
      StringUtf8Multilang::kDefaultCode == langCode)
  {
    return false;
  }

  // Exclude languages that are already present.
  auto const it =
      find_if(result.begin(), result.end(), [langCode](osm::LocalizedName const & localizedName) {
        return localizedName.m_code == langCode;
      });

  if (result.end() != it)
    return false;

  std::string name;
  names.GetString(langCode, name);
  result.emplace_back(langCode, name);

  return true;
}

size_t PushMwmLanguages(StringUtf8Multilang const & names, std::vector<int8_t> const & mwmLanguages,
                        std::vector<osm::LocalizedName> & result)
{
  size_t count = 0;
  static size_t const kMaxCountMwmLanguages = 2;

  for (size_t i = 0; i < mwmLanguages.size() && count < kMaxCountMwmLanguages; ++i)
  {
    if (ExtractName(names, mwmLanguages[i], result))
      ++count;
  }

  return count;
}

char const * const kWebsiteProtocols[] = {"http://", "https://"};
size_t const kWebsiteProtocolDefaultIndex = 0;

size_t GetProtocolNameLength(std::string const & website)
{
  for (auto const & protocol : kWebsiteProtocols)
  {
    if (strings::StartsWith(website, protocol))
      return strlen(protocol);
  }
  return 0;
}

bool IsProtocolSpecified(std::string const & website)
{
  return GetProtocolNameLength(website) > 0;
}
osm::FakeNames MakeFakeSource(StringUtf8Multilang const & source,
                              std::vector<int8_t> const & mwmLanguages, StringUtf8Multilang & fakeSource)
{
  std::string defaultName;
  // Fake names works for mono language (official) speaking countries-only.
  if (mwmLanguages.size() != 1 || !source.GetString(StringUtf8Multilang::kDefaultCode, defaultName))
  {
    return {};
  }

  osm::FakeNames fakeNames;
  // Mwm name has higher priority then English name.
  array<int8_t, kFakeNamesCount> fillCandidates = {{mwmLanguages.front(), StringUtf8Multilang::kEnglishCode}};
  fakeSource = source;

  std::string tempName;
  for (auto const code : fillCandidates)
  {
    if (!source.GetString(code, tempName))
    {
      tempName = defaultName;
      fakeSource.AddString(code, defaultName);
    }

    fakeNames.m_names.emplace_back(code, tempName);
  }

  fakeNames.m_defaultName = defaultName;

  return fakeNames;
}

// Tries to set default name from the localized name. Returns false when there's no such localized name.
bool TryToFillDefaultNameFromCode(int8_t const code, StringUtf8Multilang & names)
{
  std::string newDefaultName;
  if (code != StringUtf8Multilang::kUnsupportedLanguageCode)
    names.GetString(code, newDefaultName);

  // Default name can not be empty.
  if (!newDefaultName.empty())
  {
    names.AddString(StringUtf8Multilang::kDefaultCode, newDefaultName);
    return true;
  }

  return false;
}

// Tries to set default name to any non-empty localized name.
// This is the case when fake names were cleared.
void TryToFillDefaultNameFromAnyLanguage(StringUtf8Multilang & names)
{
  names.ForEach([&names](int8_t langCode, std::string const & name)
  {
    if (name.empty() || langCode == StringUtf8Multilang::kDefaultCode)
      return true;

    names.AddString(StringUtf8Multilang::kDefaultCode, name);
    return false;
  });
}

void RemoveFakesFromName(osm::FakeNames const & fakeNames, StringUtf8Multilang & name)
{
  std::vector<int8_t> codesToExclude;
  std::string defaultName;
  name.GetString(StringUtf8Multilang::kDefaultCode, defaultName);

  for (auto const & item : fakeNames.m_names)
  {
    std::string tempName;
    if (!name.GetString(item.m_code, tempName))
      continue;
    // No need to save in case when name is empty, duplicate of default name or was not changed.
    if (tempName.empty() || tempName == defaultName ||
        (tempName == item.m_filledName && tempName == fakeNames.m_defaultName))
    {
      codesToExclude.push_back(item.m_code);
    }
  }

  if (codesToExclude.empty())
    return;

  StringUtf8Multilang nameWithoutFakes;
  name.ForEach([&codesToExclude, &nameWithoutFakes](int8_t langCode, std::string const & value)
  {
    auto const it = find(codesToExclude.begin(), codesToExclude.end(), langCode);
    if (it == codesToExclude.end())
      nameWithoutFakes.AddString(langCode, value);

    return true;
  });

  name = nameWithoutFakes;
}
}  // namespace

namespace osm
{
// LocalizedName -----------------------------------------------------------------------------------

LocalizedName::LocalizedName(int8_t const code, std::string const & name)
  : m_code(code)
  , m_lang(StringUtf8Multilang::GetLangByCode(code))
  , m_langName(StringUtf8Multilang::GetLangNameByCode(code))
  , m_name(name)
{
}

LocalizedName::LocalizedName(std::string const & langCode, std::string const & name)
  : m_code(StringUtf8Multilang::GetLangIndex(langCode))
  , m_lang(StringUtf8Multilang::GetLangByCode(m_code))
  , m_langName(StringUtf8Multilang::GetLangNameByCode(m_code))
  , m_name(name)
{
}

// EditableMapObject -------------------------------------------------------------------------------

// static
int8_t const EditableMapObject::kMaximumLevelsEditableByUsers = 25;

bool EditableMapObject::IsNameEditable() const { return m_editableProperties.m_name; }
bool EditableMapObject::IsAddressEditable() const { return m_editableProperties.m_address; }

vector<Props> EditableMapObject::GetEditableProperties() const
{
  return MetadataToProps(m_editableProperties.m_metadata);
}

vector<feature::Metadata::EType> const & EditableMapObject::GetEditableFields() const
{
  return m_editableProperties.m_metadata;
}

StringUtf8Multilang const & EditableMapObject::GetName() const { return m_name; }

NamesDataSource EditableMapObject::GetNamesDataSource(bool needFakes /* = true */)
{
  auto const mwmInfo = GetID().m_mwmId.GetInfo();

  if (!mwmInfo)
    return NamesDataSource();

  std::vector<int8_t> mwmLanguages;
  mwmInfo->GetRegionData().GetLanguages(mwmLanguages);

  auto const userLangCode = StringUtf8Multilang::GetLangIndex(languages::GetCurrentNorm());

  if (needFakes)
  {
    StringUtf8Multilang fakeSource;
    m_fakeNames = MakeFakeSource(m_name, mwmLanguages, fakeSource);

    if (!m_fakeNames.m_names.empty())
      return GetNamesDataSource(fakeSource, mwmLanguages, userLangCode);
  }
  else
  {
    RemoveFakeNames(m_fakeNames, m_name);
  }

  return GetNamesDataSource(m_name, mwmLanguages, userLangCode);
}

// static
NamesDataSource EditableMapObject::GetNamesDataSource(StringUtf8Multilang const & source,
                                                      std::vector<int8_t> const & mwmLanguages,
                                                      int8_t const userLangCode)
{
  NamesDataSource result;
  auto & names = result.names;
  auto & mandatoryCount = result.mandatoryNamesCount;
  // Push Mwm languages.
  mandatoryCount = PushMwmLanguages(source, mwmLanguages, names);

  // Push english name.
  if (ExtractName(source, StringUtf8Multilang::kEnglishCode, names))
    ++mandatoryCount;

  // Push user's language.
  if (ExtractName(source, userLangCode, names))
    ++mandatoryCount;

  // Push other languages.
  source.ForEach([&names, mandatoryCount](int8_t const code, std::string const & name) -> bool {
    // Exclude default name.
    if (StringUtf8Multilang::kDefaultCode == code)
      return true;

    auto const mandatoryNamesEnd = names.begin() + mandatoryCount;
    // Exclude languages which are already in container (languages with top priority).
    auto const it = find_if(
        names.begin(), mandatoryNamesEnd,
        [code](LocalizedName const & localizedName) { return localizedName.m_code == code; });

    if (mandatoryNamesEnd == it)
      names.emplace_back(code, name);

    return true;
  });

  return result;
}

vector<LocalizedStreet> const & EditableMapObject::GetNearbyStreets() const { return m_nearbyStreets; }
std::string const & EditableMapObject::GetHouseNumber() const { return m_houseNumber; }

std::string EditableMapObject::GetPostcode() const
{
  return m_metadata.Get(feature::Metadata::FMD_POSTCODE);
}

std::string EditableMapObject::GetWikipedia() const
{
  return m_metadata.Get(feature::Metadata::FMD_WIKIPEDIA);
}

uint64_t EditableMapObject::GetTestId() const
{
  std::istringstream iss(m_metadata.Get(feature::Metadata::FMD_TEST_ID));
  uint64_t id;
  iss >> id;
  return id;
}

void EditableMapObject::SetTestId(uint64_t id)
{
  std::ostringstream oss;
  oss << id;
  m_metadata.Set(feature::Metadata::FMD_TEST_ID, oss.str());
}

void EditableMapObject::SetEditableProperties(osm::EditableProperties const & props)
{
  m_editableProperties = props;
}

void EditableMapObject::SetName(StringUtf8Multilang const & name) { m_name = name; }

void EditableMapObject::SetName(std::string name, int8_t langCode)
{
  strings::Trim(name);

  if (m_namesAdvancedMode)
  {
    m_name.AddString(langCode, name);
    return;
  }

  if (!name.empty() && !Editor::Instance().OriginalFeatureHasDefaultName(GetID()))
  {
    const auto mwmInfo = GetID().m_mwmId.GetInfo();

    if (mwmInfo)
    {
      std::vector<int8_t> mwmLanguages;
      mwmInfo->GetRegionData().GetLanguages(mwmLanguages);

      if (CanUseAsDefaultName(langCode, mwmLanguages))
        m_name.AddString(StringUtf8Multilang::kDefaultCode, name);
    }
  }

  m_name.AddString(langCode, name);
}

// static
bool EditableMapObject::CanUseAsDefaultName(int8_t const lang, std::vector<int8_t> const & mwmLanguages)
{
  for (auto const & mwmLang : mwmLanguages)
  {
    if (StringUtf8Multilang::kUnsupportedLanguageCode == mwmLang)
      continue;

    if (lang == mwmLang)
      return true;
  }

  return false;
}

// static
void EditableMapObject::RemoveFakeNames(FakeNames const & fakeNames, StringUtf8Multilang & name)
{
  if (fakeNames.m_names.empty())
    return;

  int8_t newDefaultNameCode = StringUtf8Multilang::kUnsupportedLanguageCode;
  size_t changedCount = 0;
  std::string defaultName;
  name.GetString(StringUtf8Multilang::kDefaultCode, defaultName);

  // New default name calculation priority: 1. name on mwm language, 2. english name.
  for (auto it = fakeNames.m_names.rbegin(); it != fakeNames.m_names.rend(); ++it)
  {
    std::string tempName;
    if (!name.GetString(it->m_code, tempName))
      continue;

    if (tempName != it->m_filledName)
    {
      if (!tempName.empty())
        newDefaultNameCode = it->m_code;

      ++changedCount;
    }
  }

  // If all previously filled fake names were changed - try to change the default name.
  if (changedCount == fakeNames.m_names.size())
  {
    if (!TryToFillDefaultNameFromCode(newDefaultNameCode, name))
      TryToFillDefaultNameFromAnyLanguage(name);
  }

  RemoveFakesFromName(fakeNames, name);
}

void EditableMapObject::SetMercator(m2::PointD const & center) { m_mercator = center; }

void EditableMapObject::SetType(uint32_t featureType)
{
  if (m_types.GetGeoType() == feature::EGeomType::GEOM_UNDEFINED)
  {
    // Support only point type for newly created features.
    m_types = feature::TypesHolder(feature::EGeomType::GEOM_POINT);
    m_types.Assign(featureType);
  }
  else
  {
    // Correctly replace "main" type in cases when feature holds more types.
    ASSERT(!m_types.Empty(), ());
    feature::TypesHolder copy = m_types;
    // TODO(mgsergio): Replace by correct sorting from editor's config.
    copy.SortBySpec();
    m_types.Remove(*copy.begin());
    m_types.Add(featureType);
  }
}

void EditableMapObject::SetID(FeatureID const & fid) { m_featureID = fid; }
void EditableMapObject::SetStreet(LocalizedStreet const & st) { m_street = st; }

void EditableMapObject::SetNearbyStreets(std::vector<LocalizedStreet> && streets)
{
  m_nearbyStreets = move(streets);
}

void EditableMapObject::SetHouseNumber(std::string const & houseNumber)
{
  m_houseNumber = houseNumber;
}

void EditableMapObject::SetPostcode(std::string const & postcode)
{
  m_metadata.Set(feature::Metadata::FMD_POSTCODE, postcode);
}

void EditableMapObject::SetPhone(std::string const & phone)
{
  m_metadata.Set(feature::Metadata::FMD_PHONE_NUMBER, phone);
}

void EditableMapObject::SetFax(std::string const & fax)
{
  m_metadata.Set(feature::Metadata::FMD_FAX_NUMBER, fax);
}

void EditableMapObject::SetEmail(std::string const & email)
{
  m_metadata.Set(feature::Metadata::FMD_EMAIL, email);
}

void EditableMapObject::SetWebsite(std::string website)
{
  if (!website.empty() && !IsProtocolSpecified(website))
    website = kWebsiteProtocols[kWebsiteProtocolDefaultIndex] + website;

  m_metadata.Set(feature::Metadata::FMD_WEBSITE, website);
  m_metadata.Drop(feature::Metadata::FMD_URL);
}

void EditableMapObject::SetInternet(Internet internet)
{
  m_metadata.Set(feature::Metadata::FMD_INTERNET, DebugPrint(internet));

  static auto const wifiType = classif().GetTypeByPath({"internet_access", "wlan"});

  if (m_types.Has(wifiType) && internet != Internet::Wlan)
    m_types.Remove(wifiType);
  else if (!m_types.Has(wifiType) && internet == Internet::Wlan)
    m_types.Add(wifiType);
}

void EditableMapObject::SetStars(int stars)
{
  if (stars > 0 && stars <= 7)
    m_metadata.Set(feature::Metadata::FMD_STARS, strings::to_string(stars));
  else
    LOG(LWARNING, ("Ignored invalid value to Stars:", stars));
}

void EditableMapObject::SetOperator(std::string const & op)
{
  m_metadata.Set(feature::Metadata::FMD_OPERATOR, op);
}

void EditableMapObject::SetElevation(double ele)
{
  // TODO: Reuse existing validadors in generator (osm2meta).
  constexpr double kMaxElevationOnTheEarthInMeters = 10000;
  constexpr double kMinElevationOnTheEarthInMeters = -15000;
  if (ele < kMaxElevationOnTheEarthInMeters && ele > kMinElevationOnTheEarthInMeters)
    m_metadata.Set(feature::Metadata::FMD_ELE, strings::to_string_dac(ele, 1));
  else
    LOG(LWARNING, ("Ignored invalid value to Elevation:", ele));
}

void EditableMapObject::SetWikipedia(std::string const & wikipedia)
{
  m_metadata.Set(feature::Metadata::FMD_WIKIPEDIA, wikipedia);
}

void EditableMapObject::SetFlats(std::string const & flats)
{
  m_metadata.Set(feature::Metadata::FMD_FLATS, flats);
}

void EditableMapObject::SetBuildingLevels(std::string const & buildingLevels)
{
  m_metadata.Set(feature::Metadata::FMD_BUILDING_LEVELS, buildingLevels);
}

LocalizedStreet const & EditableMapObject::GetStreet() const { return m_street; }

void EditableMapObject::SetCuisines(std::vector<std::string> const & cuisine)
{
  m_metadata.Set(feature::Metadata::FMD_CUISINE, strings::JoinStrings(cuisine, ';'));
}

void EditableMapObject::SetOpeningHours(std::string const & openingHours)
{
  m_metadata.Set(feature::Metadata::FMD_OPEN_HOURS, openingHours);
}

void EditableMapObject::SetPointType() { m_geomType = feature::EGeomType::GEOM_POINT; }


void EditableMapObject::RemoveBlankNames()
{
  StringUtf8Multilang nameWithoutBlanks;
  m_name.ForEach([&nameWithoutBlanks](int8_t langCode, std::string const & name)
  {
    if (!name.empty())
      nameWithoutBlanks.AddString(langCode, name);

    return true;
  });

  m_name = nameWithoutBlanks;
}

void EditableMapObject::RemoveNeedlessNames()
{
  if (!IsNamesAdvancedModeEnabled())
    RemoveFakeNames(m_fakeNames, m_name);

  RemoveBlankNames();
}

// static
bool EditableMapObject::ValidateBuildingLevels(std::string const & buildingLevels)
{
  if (buildingLevels.empty())
    return true;

  if (buildingLevels.size() > 18 /* max number of digits in uint_64 */)
    return false;

  if ('0' == buildingLevels.front())
    return false;

  uint64_t levels;
  return strings::to_uint64(buildingLevels, levels) && levels > 0 && levels <= kMaximumLevelsEditableByUsers;
}

// static
bool EditableMapObject::ValidateHouseNumber(std::string const & houseNumber)
{
  // TODO(mgsergio): Use LooksLikeHouseNumber!

  if (houseNumber.empty())
    return true;

  strings::UniString us = strings::MakeUniString(houseNumber);
  // TODO: Improve this basic limit - it was choosen by @Zverik.
  auto constexpr kMaxHouseNumberLength = 15;
  if (us.size() > kMaxHouseNumberLength)
    return false;

  // TODO: Should we allow arabic numbers like U+0661 ١	Arabic-Indic Digit One?
  strings::NormalizeDigits(us);
  for (auto const c : us)
  {
    // Valid house numbers contain at least one digit.
    if (strings::IsASCIIDigit(c))
      return true;
  }
  return false;
}

// static
bool EditableMapObject::ValidateFlats(std::string const & flats)
{
  for (auto it = strings::SimpleTokenizer(flats, ";"); it; ++it)
  {
    auto token = *it;
    strings::Trim(token);

    std::vector<std::string> range;
    for (auto i = strings::SimpleTokenizer(token, "-"); i; ++i)
      range.push_back(*i);
    if (range.empty() || range.size() > 2)
      return false;

    for (auto const & rangeBorder : range)
    {
      if (!std::all_of(std::begin(rangeBorder), std::end(rangeBorder), ::isalnum))
        return false;
    }
  }
  return true;
}

// static
bool EditableMapObject::ValidatePostCode(std::string const & postCode)
{
  if (postCode.empty())
    return true;
  return search::LooksLikePostcode(postCode, false /* IsPrefix */);
}

// static
bool EditableMapObject::ValidatePhone(std::string const & phone)
{
  if (phone.empty())
    return true;

  auto curr = begin(phone);
  auto const last = end(phone);

  auto const kMaxNumberLen = 15;
  auto const kMinNumberLen = 5;

  if (*curr == '+')
    ++curr;

  auto digitsCount = 0;
  for (; curr != last; ++curr)
  {
    auto const isCharValid = std::isdigit(*curr) || *curr == '(' ||
                             *curr == ')' || *curr == ' ' || *curr == '-';
    if (!isCharValid)
      return false;

    if (std::isdigit(*curr))
      ++digitsCount;
  }

  return kMinNumberLen <= digitsCount && digitsCount <= kMaxNumberLen;
}

// static
bool EditableMapObject::ValidateWebsite(std::string const & site)
{
  if (site.empty())
    return true;

  auto const startPos = GetProtocolNameLength(site);

  if (startPos >= site.size())
    return false;

  // Site should contain at least one dot but not at the begining/end.
  if ('.' == site[startPos] || '.' == site.back())
    return false;

  if (std::string::npos == site.find("."))
    return false;

  if (std::string::npos != site.find(".."))
    return false;

  return true;
}

// static
bool EditableMapObject::ValidateEmail(std::string const & email)
{
  if (email.empty())
    return true;

  if (strings::IsASCIIString(email))
    return std::regex_match(email, std::regex(R"([^@\s]+@[a-zA-Z0-9-]+(\.[a-zA-Z0-9-]+)+$)"));

  if ('@' == email.front() || '@' == email.back())
    return false;

  if ('.' == email.back())
    return false;

  auto const atPos = find(begin(email), end(email), '@');
  if (atPos == end(email))
    return false;

  // There should be only one '@' sign.
  if (find(next(atPos), end(email), '@') != end(email))
    return false;

  // There should be at least one '.' sign after '@'
  if (find(next(atPos), end(email), '.') == end(email))
    return false;

  return true;
}
}  // namespace osm
