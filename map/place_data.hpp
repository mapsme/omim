#pragma once

#include "search/metadata.hpp"

#include "indexer/feature.hpp"

#include "editor/yes_no_unknown.hpp"

#include "std/string.hpp"
#include "std/vector.hpp"

namespace place_data
{
extern char const * const kSubtitleSeparator;
extern char const * const kStarSymbol;
extern char const * const kMountainSymbol;
extern char const * const kEmptyRatingSymbol;
extern char const * const kPricingSymbol;

/// Contains formatted feature type, booking rating and approximate pricing, which should be displayed in the GUI.
class Data
{
public:
  Data() = default;

  Data(FeatureType const & ft, string const & featureType);

  Data(search::Metadata const & md, string const & featureType);

  string const & GetSubtitle() const { return m_subtitle; }
  string const & GetHotelRating() const { return m_hotelRating; }
  string const & GetApproximatePricing() const { return m_approximatePricing; }
  osm::YesNoUnknown IsOpen() const { return m_isOpenNow; }
  bool IsSponsoredHotel() const { return m_isSponsoredHotel; }

private:
  void ConfigSubtitle(search::Metadata const & m);
  void ConfigHotelRating(string const & ratingStr);
  void ConfigApproximatePricing(string const & pricingStr);
  void ConfigIsOpen(string const & openHoursStr);

  // This is a non-owning pointer.
  feature::Metadata const * m_metadata = nullptr;
  bool m_isSponsoredHotel = false;
  osm::YesNoUnknown m_isOpenNow = osm::YesNoUnknown::Unknown;
  string m_approximatePricing;
  string m_featureType;
  string m_hotelRating;
  string m_subtitle;
};
}  // namespace place_data
