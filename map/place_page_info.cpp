#include "place_page_info.hpp"

#include "indexer/osm_editor.hpp"

#include "platform/preferred_languages.hpp"

namespace place_page
{
void Info::SetFromFeatureType(FeatureType const & ft)
{
  MapObject::SetFromFeatureType(ft);
  m_placeData = place_data::Data(ft, GetLocalizedType());
}

bool Info::IsFeature() const { return m_featureID.IsValid(); }
bool Info::IsBookmark() const { return m_bac != MakeEmptyBookmarkAndCategory(); }
bool Info::IsMyPosition() const { return m_isMyPosition; }
bool Info::IsHotel() const { return m_isHotel; }
bool Info::ShouldShowAddPlace() const
{
  auto const isPointOrBuilding = IsPointType() || IsBuilding();
  return m_canEditOrAdd && !(IsFeature() && isPointOrBuilding);
}

bool Info::ShouldShowAddBusiness() const { return m_canEditOrAdd && IsBuilding(); }

bool Info::ShouldShowEditPlace() const
{
  return m_canEditOrAdd &&
         // TODO(mgsergio): Does IsFeature() imply !IsMyPosition()?
         !IsMyPosition() && IsFeature();
}

bool Info::HasApiUrl() const { return !m_apiUrl.empty(); }
bool Info::HasWifi() const { return GetInternet() == osm::Internet::Wlan; }

string Info::FormatNewBookmarkName() const
{
  string const title = GetTitle();
  if (title.empty())
    return GetLocalizedType();
  return title;
}

string Info::GetTitle() const
{
  if (!m_customName.empty())
    return m_customName;

  // Prefer names in native language over default ones.
  int8_t const langCode = StringUtf8Multilang::GetLangIndex(languages::GetCurrentNorm());
  if (langCode != StringUtf8Multilang::kUnsupportedLanguageCode)
  {
    string native;
    if (m_name.GetString(langCode, native))
      return native;
  }

  return GetDefaultName();
}

string Info::GetSubtitle() const
{
  if (!IsFeature())
  {
    if (IsBookmark())
      return m_bookmarkCategoryName;
    return {};
  }

  vector<string> values;

  // Bookmark category.
  if (IsBookmark())
    values.push_back(m_bookmarkCategoryName);

  values.push_back(m_placeData.GetSubtitle());

  if (HasWifi())
    values.push_back(m_localizedWifiString);

  return strings::JoinStrings(values, place_data::kSubtitleSeparator);
}

string Info::GetCustomName() const { return m_customName; }
BookmarkAndCategory Info::GetBookmarkAndCategory() const { return m_bac; }
string Info::GetBookmarkCategoryName() const { return m_bookmarkCategoryName; }
string const & Info::GetApiUrl() const { return m_apiUrl; }

string const & Info::GetSponsoredBookingUrl() const { return m_sponsoredBookingUrl; }
string const & Info::GetSponsoredDescriptionUrl() const {return m_sponsoredDescriptionUrl; }

void Info::SetMercator(m2::PointD const & mercator) { m_mercator = mercator; }
}  // namespace place_page
