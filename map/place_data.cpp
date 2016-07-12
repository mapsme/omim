#include "map/place_data.hpp"

#include "indexer/cuisines.hpp"
#include "indexer/ftypes_matcher.hpp"

#include "platform/measurement_utils.hpp"

#include "base/string_utils.hpp"

#include "3party/opening_hours/opening_hours.hpp"

namespace place_data
{
char const * const kSubtitleSeparator = " • ";
char const * const kStarSymbol = "★";
char const * const kMountainSymbol = "▲";
char const * const kEmptyRatingSymbol = "-";
char const * const kPricingSymbol = "$";

vector<string> GetLocalizedCuisines(string const & cuisineStr)
{
  vector<string> localized;
  osm::Cuisines::Instance().ParseAndLocalize(cuisineStr, localized);
  return localized;
}

string FormatStars(string const & starsStr)
{
  string stars;
  int count = 0;
  strings::to_int(starsStr, count);
  CHECK(count >= 0, ("Star's count must be positive!"));
  string result;
  for (int i = 0; i < count; ++i)
    result.append(kStarSymbol);
  return result;
}

string GetElevationFormatted(string const & elevationStr)
{
  double value;
  if (strings::to_double(elevationStr, value))
    return measurement_utils::FormatAltitude(value);

  LOG(LWARNING, ("Invalid metadata for elevation:", elevationStr));
  return {};
}

Data::Data(FeatureType const & ft, string const & featureType)
  : m_featureType(featureType)
  , m_isSponsoredHotel(ftypes::IsBookingChecker::Instance()(ft))
  , m_metadata(&ft.GetMetadata())
{
  search::Metadata md;
  ConfigSubtitle(md);
  ConfigHotelRating(m_metadata->Get(feature::Metadata::FMD_RATING));
  ConfigApproximatePricing(m_metadata->Get(feature::Metadata::FMD_PRICE_RATE));
  ConfigIsOpen(m_metadata->Get(feature::Metadata::FMD_OPEN_HOURS));
}

Data::Data(search::Metadata const & md, string const & featureType)
  : m_featureType(featureType), m_isSponsoredHotel(md.m_isSponsoredHotel)
{
  ConfigSubtitle(md);
  ConfigHotelRating(md.m_hotelRating);
  ConfigApproximatePricing(md.m_hotelPriceRate);
  ConfigIsOpen(md.m_schedule);
}

void Data::ConfigSubtitle(search::Metadata const & m)
{
  vector<string> values;
  // Type
  values.push_back(m_featureType);
  bool const isInitialized = m.m_isInitialized;

  // Cuisine
  auto const cuisines = GetLocalizedCuisines(
      isInitialized ? m.m_cuisine : m_metadata->Get(feature::Metadata::FMD_CUISINE));
  for (auto const & cuisine : cuisines)
    values.push_back(cuisine);

  // Hotel's stars
  auto const stars =
      FormatStars(isInitialized ? m.m_stars : m_metadata->Get(feature::Metadata::FMD_STARS));
  if (!stars.empty())
    values.push_back(stars);

  // Operator
  auto const op = isInitialized ? m.m_operator : m_metadata->Get(feature::Metadata::FMD_OPERATOR);
  if (!op.empty())
    values.push_back(op);

  // Elevation
  auto const ele = GetElevationFormatted(
      isInitialized ? m.m_elevation : m_metadata->Get(feature::Metadata::FMD_ELE));
  if (!ele.empty())
    values.push_back(kMountainSymbol + ele);

  m_subtitle = strings::JoinStrings(values, kSubtitleSeparator);
}

void Data::ConfigHotelRating(string const & ratingStr)
{
  if (!IsSponsoredHotel())
    return;

  m_hotelRating = ratingStr.empty() ? kEmptyRatingSymbol : ratingStr;
}

void Data::ConfigApproximatePricing(string const & pricingStr)
{
  if (!IsSponsoredHotel())
    return;

  int pricing;
  strings::to_int(pricingStr, pricing);
  CHECK(pricing >= 0, ("Pricing must be positive!"));
  string result;
  for (int i = 0; i < pricing; ++i)
    result.append(kPricingSymbol);

  m_approximatePricing = result;
}

void Data::ConfigIsOpen(string const & openHoursStr)
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
}  // namespace place_data
