#import "PlacePagePreviewData+Core.h"

#import "UgcSummaryRating.h"

#include "3party/opening_hours/opening_hours.hpp"

static PlacePageDataSchedule convertOpeningHours(std::string rawOpeningHours) {
  if (rawOpeningHours.empty())
    return PlacePageDataOpeningHoursUnknown;

  osmoh::OpeningHours oh(rawOpeningHours);
  if (!oh.IsValid()) {
    return PlacePageDataOpeningHoursUnknown;
  }
  if (oh.IsTwentyFourHours()) {
    return PlacePageDataOpeningHoursAllDay;
  }

  auto const t = time(nullptr);
  if (oh.IsOpen(t)) {
    return PlacePageDataOpeningHoursOpen;
  }
  if (oh.IsClosed(t)) {
    return PlacePageDataOpeningHoursClosed;
  }

  return PlacePageDataOpeningHoursUnknown;
}

static PlacePageDataHotelType convertHotelType(boost::optional<ftypes::IsHotelChecker::Type> hotelType) {
  if (!hotelType.has_value()) {
    return PlacePageDataHotelTypeNone;
  }

  switch (hotelType.get()) {
    case ftypes::IsHotelChecker::Type::Hotel:
      return PlacePageDataHotelTypeHotel;
    case ftypes::IsHotelChecker::Type::Apartment:
      return PlacePageDataHotelTypeApartment;
    case ftypes::IsHotelChecker::Type::CampSite:
      return PlacePageDataHotelTypeCampSite;
    case ftypes::IsHotelChecker::Type::Chalet:
      return PlacePageDataHotelTypeChalet;
    case ftypes::IsHotelChecker::Type::GuestHouse:
      return PlacePageDataHotelTypeGuestHouse;
    case ftypes::IsHotelChecker::Type::Hostel:
      return PlacePageDataHotelTypeHostel;
    case ftypes::IsHotelChecker::Type::Motel:
      return PlacePageDataHotelTypeMotel;
    case ftypes::IsHotelChecker::Type::Resort:
      return PlacePageDataHotelTypeResort;
    case ftypes::IsHotelChecker::Type::Count:
      return PlacePageDataHotelTypeNone;
  }
}

@implementation PlacePagePreviewData

@end

@implementation PlacePagePreviewData (Core)

- (instancetype)initWithRawData:(place_page::Info const &)rawData {
  self = [super init];
  if (self) {
    _title = rawData.GetTitle().empty() ? nil : @(rawData.GetTitle().c_str());
    _subtitle = rawData.GetSubtitle().empty() ? nil : @(rawData.GetSubtitle().c_str());
    _address = rawData.GetAddress().empty() ? nil : @(rawData.GetAddress().c_str());
    _pricing = rawData.GetApproximatePricing().empty() ? nil : @(rawData.GetApproximatePricing().c_str());
    _hasBanner = rawData.HasBanner();
    _isPopular = rawData.GetPopularity() > 0;
    _isBookingPlace = rawData.GetSponsoredType() == place_page::SponsoredType::Booking;
    _schedule = convertOpeningHours(rawData.GetOpeningHours());
    _hotelType = convertHotelType(rawData.GetHotelType());
    _showUgc = rawData.ShouldShowUGC();
  }
  return self;
}

@end
