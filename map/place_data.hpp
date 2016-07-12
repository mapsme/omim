#pragma once

#include "base/string_utils.hpp"

#include "indexer/cuisines.hpp"
#include "indexer/feature.hpp"
#include "indexer/ftypes_matcher.hpp"

#include "editor/yes_no_unknown.hpp"

#include "platform/measurement_utils.hpp"

#include "search/metadata.hpp"

#include "std/string.hpp"
#include "std/vector.hpp"

#include "3party/opening_hours/opening_hours.hpp"

namespace place_data
{
extern char const * kSubtitleSeparator;
extern char const * kStarSymbol;
extern char const * kMountainSymbol;
extern char const * kEmptyRatingSymbol;
extern char const * kPricingSymbol;

/// Contains formatted feature type, booking rating and approximate pricing, which should be displayed in the GUI.
template <typename Owner>
class Data
{
public:
  Data() = default;

  Data(FeatureType const & ft, Owner const * ow) : m_owner(ow),
                                                   m_isSponsoredHotel(ftypes::IsBookingChecker::Instance()(ft)),
                                                   m_metadata(&ft.GetMetadata())
  {
    search::Metadata md;
    ConfigSubtitle(md);
    ConfigHotelRating(m_metadata->Get(feature::Metadata::FMD_RATING));
    ConfigApproximatePricing(m_metadata->Get(feature::Metadata::FMD_PRICE_RATE));
    ConfigIsOpen(m_metadata->Get(feature::Metadata::FMD_OPEN_HOURS));
  }

  Data(search::Metadata const & md, Owner const * ow) : m_owner(ow),
                                                        m_isSponsoredHotel(md.m_isSponsoredHotel)
  {
    ConfigSubtitle(md);
    ConfigHotelRating(md.m_hotelRating);
    ConfigApproximatePricing(md.m_hotelPriceRate);
    ConfigIsOpen(md.m_schedule);
  }

  string GetSubtitle() const { return m_subtitle; }
  string GetHotelRating() const { return m_hotelRating; }
  string GetApproximatePricing() const { return m_approximatePricing; }
  osm::YesNoUnknown IsOpen() const { return m_isOpenNow; }
  bool IsSponsoredHotel() const { return m_isSponsoredHotel; }

private:
  void ConfigSubtitle(search::Metadata const & m)
  {
    vector<string> values;
    // Type
    values.push_back(m_owner->GetFeatureType());
    bool const isSearch = m.m_isInitialized;

    // Cuisine
    for (auto const & cuisine : GetLocalizedCuisines(isSearch ? m.m_cuisine :
                                                     m_metadata->Get(feature::Metadata::FMD_CUISINE)))
      values.push_back(cuisine);

    // Hotel's stars
    auto const stars = FormatStars(isSearch ? m.m_stars : m_metadata->Get(feature::Metadata::FMD_STARS));
    if (!stars.empty())
      values.push_back(stars);

    // Operator
    auto const op = isSearch ? m.m_operator : m_metadata->Get(feature::Metadata::FMD_OPERATOR);
    if (!op.empty())
      values.push_back(op);

    // Elevation
    auto const ele = GetElevationFormatted(isSearch ? m.m_elevation : m_metadata->Get(feature::Metadata::FMD_ELE));
    if (!ele.empty())
      values.push_back(kMountainSymbol + ele);

    m_subtitle = strings::JoinStrings(values, kSubtitleSeparator);
  }

  vector<string> GetLocalizedCuisines(string const & cuisineStr) const
  {
    vector<string> localized;
    osm::Cuisines::Instance().ParseAndLocalize(cuisineStr, localized);
    return localized;
  }

  string FormatStars(string const & starsStr) const
  {
    string stars;
    int count = 0;
    strings::to_int(starsStr, count);
    for (int i = 0; i < count; ++i)
      stars.append(kStarSymbol);

    return stars;
  }

  string GetElevationFormatted(string const & elevationStr) const
  {
    double value;
    if (strings::to_double(elevationStr, value))
      return measurement_utils::FormatAltitude(value);

    LOG(LWARNING, ("Invalid metadata for elevation:", elevationStr));
    return {};
  }

  void ConfigHotelRating(string const & ratingStr)
  {
    if (!IsSponsoredHotel())
      return;

    m_hotelRating = ratingStr.empty() ? kEmptyRatingSymbol : ratingStr;
  }

  void ConfigApproximatePricing(string const & pricingStr)
  {
    if (!IsSponsoredHotel())
      return;

    int pricing;
    strings::to_int(pricingStr, pricing);
    string result;
    for (auto i = 0; i < pricing; i++)
      result.append(kPricingSymbol);
    
    m_approximatePricing = result;
  }

  void ConfigIsOpen(string const & openHoursStr)
  {
    if (openHoursStr.empty())
      return;
    osmoh::OpeningHours const oh(openHoursStr);
    // TODO: We should check closed/open time for specific feature's timezone.
    time_t const now = time(nullptr);
    if (oh.IsValid() && !oh.IsUnknown(now))
      m_isOpenNow = oh.IsOpen(now) ? osm::Yes : osm::No;
    // In else case value us osm::Unknown, it's set in preview's constructor.
  }

private:
  feature::Metadata const * m_metadata;
  Owner const * m_owner;
  bool m_isSponsoredHotel = false;
  osm::YesNoUnknown m_isOpenNow = osm::YesNoUnknown::Unknown;
  string m_subtitle;
  string m_approximatePricing;
  string m_hotelRating;
};
}  // namespace place_data
