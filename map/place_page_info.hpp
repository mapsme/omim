#pragma once

#include "map/bookmark.hpp"

#include "storage/index.hpp"

#include "indexer/feature_data.hpp"
#include "indexer/feature_meta.hpp"
#include "indexer/map_object.hpp"

#include "geometry/latlon.hpp"
#include "geometry/mercator.hpp"
#include "geometry/point2d.hpp"

#include "std/string.hpp"

enum class SponsoredType
{
  None,
  Booking,
  Opentable,
  Geochat
};

namespace place_page
{
class Info : public osm::MapObject
{
public:
  static char const * const kSubtitleSeparator;
  static char const * const kStarSymbol;
  static char const * const kMountainSymbol;
  static char const * const kEmptyRatingSymbol;
  static char const * const kPricingSymbol;

  bool IsFeature() const;
  bool IsBookmark() const;
  bool IsMyPosition() const;
  bool IsSponsored() const;

  bool ShouldShowAddPlace() const;
  bool ShouldShowAddBusiness() const;
  bool ShouldShowEditPlace() const;

  /// @returns true if Back API button should be displayed.
  bool HasApiUrl() const;

  /// TODO: Support all possible Internet types in UI. @See MapObject::GetInternet().
  bool HasWifi() const;

  string GetAddress() const { return m_address; }

  /// Should be used by UI code to generate cool name for new bookmarks.
  // TODO: Tune new bookmark name. May be add address or some other data.
  string FormatNewBookmarkName() const;

  /// Convenient wrapper for feature's name and custom name.
  string GetTitle() const;
  /// Convenient wrapper for type, cuisines, elevation, stars, wifi etc.
  string GetSubtitle() const;
  /// @returns empty string or GetStars() count of ★ symbol.
  string FormatStars() const;

  /// @returns coordinate in DMS format if isDMS is true
  string GetFormattedCoordinate(bool isDMS) const;

  string GetCustomName() const;
  BookmarkAndCategory GetBookmarkAndCategory() const;
  string GetBookmarkCategoryName() const;
  string const & GetApiUrl() const;

  string const & GetSponsoredUrl() const;
  string const & GetSponsoredDescriptionUrl() const;

  /// @returns formatted rating string for booking object, or empty if it isn't booking object
  string GetRatingFormatted() const;
  /// @returns string with |kPricingSymbol| signs or empty string if it isn't booking object
  string GetApproximatePricing() const;

  void SetMercator(m2::PointD const & mercator);

  /// Comes from API, shared links etc.
  string m_customName;
  /// If not empty, bookmark is bound to this place page.
  BookmarkAndCategory m_bac;
  /// Bookmark category name. Empty, if it's not bookmark;
  string m_bookmarkCategoryName;
  /// Bookmark title. Empty, if it's not bookmark;
  string m_bookmarkTitle;
  /// Bookmark color name. Empty, if it's not bookmark;
  string m_bookmarkColorName;
  /// Bookmark description. Empty, if it's not bookmark;
  string m_bookmarkDescription;
  /// Api ID passed for the selected object. It's automatically included in api url below.
  string m_apiId;
  /// [Deep] link to open when "Back" button is pressed in a Place Page.
  string m_apiUrl;
  /// Formatted feature address.
  string m_address;
  /// Sponsored type or None.
  SponsoredType m_sponsoredType = SponsoredType::None;

  /// Sponsored feature urls.
  string m_sponsoredUrl;
  string m_sponsoredDescriptionUrl;

  /// Which country this MapObject is in.
  /// For a country point it will be set to topmost node for country.
  storage::TCountryId m_countryId = storage::kInvalidCountryId;

  bool m_isMyPosition = false;
  /// True if editing of a selected point is allowed by basic logic.
  /// See initialization in framework.
  bool m_canEditOrAdd = false;

  // TODO(AlexZ): Temporary solution. It's better to use a wifi icon in UI instead of text.
  string m_localizedWifiString;
  /// Booking rating string
  string m_localizedRatingString;
};
}  // namespace place_page
