#include "local_ads/event.hpp"

#include "partners_api/booking_api.hpp"

#include "map/place_page_info.hpp"

#include "std/vector.hpp"

@class MWMPlacePageData;
@class MWMUGCReviewVM;

namespace ugc
{
struct Review;
}

namespace place_page
{
enum class Sections
{
  Preview,
  Bookmark,
  HotelPhotos,
  HotelDescription,
  HotelFacilities,
  HotelReviews,
  Metainfo,
  Ad,
  UGC,
  Buttons
};

enum class PreviewRows
{
  Title,
  ExternalTitle,
  Subtitle,
  Schedule,
  Booking,
  Address,
  Space,
  Banner
};

enum class HotelDescriptionRow
{
  Regular,
  ShowMore
};

enum class HotelPhotosRow
{
  Regular
};

enum class HotelFacilitiesRow
{
  Regular,
  ShowMore
};

enum class HotelReviewsRow
{
  Header,
  Regular,
  ShowMore
};

enum class MetainfoRows
{
  OpeningHours,
  ExtendedOpeningHours,
  Phone,
  Address,
  Website,
  Email,
  Cuisine,
  Operator,
  Internet,
  Coordinate,
  LocalAdsCandidate,
  LocalAdsCustomer
};

enum class AdRows
{
  Taxi
};

enum class UGCRow
{
  SelectImpression,
  Comment,
  ShowMore
};

enum class ButtonsRows
{
  AddBusiness,
  EditPlace,
  AddPlace,
  HotelDescription,
  Other
};

enum class OpeningHours
{
  AllDay,
  Open,
  Closed,
  Unknown
};

using NewSectionsAreReady = void (^)(NSRange const & range, MWMPlacePageData * data, BOOL isSection);
using BannerIsReady = void (^)();

}  // namespace place_page


@class MWMGalleryItemModel;
@protocol MWMBanner;

/// ViewModel for place page.
@interface MWMPlacePageData : NSObject

@property(copy, nonatomic) place_page::NewSectionsAreReady sectionsAreReadyCallback;
@property(copy, nonatomic) place_page::BannerIsReady bannerIsReadyCallback;

// ready callback will be called from main queue.
- (instancetype)initWithPlacePageInfo:(place_page::Info const &)info;

- (void)fillSections;

- (void)updateBookmarkStatus:(BOOL)isBookmark;

/// Country id for changing place page's fields which depend on application state.
- (storage::TCountryId const &)countryId;
- (FeatureID const &)featureId;

// Regular
- (NSString *)title;
- (NSString *)subtitle;
- (place_page::OpeningHours)schedule;
- (NSString *)address;

// Booking
- (void)fillOnlineBookingSections;
- (NSString *)bookingRating;
- (NSString *)bookingApproximatePricing;
- (NSURL *)sponsoredURL;
- (NSURL *)sponsoredDescriptionURL;
- (NSURL *)bookingSearchURL;
- (NSString *)sponsoredId;
- (void)assignOnlinePriceToLabel:(UILabel *)label;
- (NSString *)hotelDescription;
- (vector<booking::HotelFacility> const &)facilities;
- (vector<booking::HotelReview> const &)hotelReviews;
- (NSUInteger)numberOfHotelReviews;
- (NSURL *)URLToAllReviews;
- (NSArray<MWMGalleryItemModel *> *)photos;

// UGC
- (MWMUGCReviewVM *)reviewViewModel;
- (vector<ugc::Review> const &)ugcReviews;

// Route points
- (RouteMarkType)routeMarkType;
- (int8_t)intermediateIndex;

// Banner
- (id<MWMBanner>)nativeAd;

// API
- (NSString *)apiURL;

// Bookmark
- (NSString *)externalTitle;
- (NSString *)bookmarkColor;
- (NSString *)bookmarkDescription;
- (NSString *)bookmarkCategory;
- (BookmarkAndCategory)bac;

// Local Ads
- (NSString *)localAdsURL;
- (void)logLocalAdsEvent:(local_ads::EventType)type;

// Table view's data
- (vector<place_page::Sections> const &)sections;
- (vector<place_page::PreviewRows> const &)previewRows;
- (vector<place_page::HotelPhotosRow> const &)photosRows;
- (vector<place_page::HotelDescriptionRow> const &)descriptionRows;
- (vector<place_page::HotelFacilitiesRow> const &)hotelFacilitiesRows;
- (vector<place_page::HotelReviewsRow> const &)hotelReviewsRows;
- (vector<place_page::MetainfoRows> const &)metainfoRows;
- (vector<place_page::AdRows> const &)adRows;
- (vector<place_page::UGCRow> const &)ugcRows;
- (vector<place_page::ButtonsRows> const &)buttonsRows;

// Table view metainfo rows
- (NSString *)stringForRow:(place_page::MetainfoRows)row;

// Helpers
- (NSString *)phoneNumber;
- (BOOL)isBookmark;
- (BOOL)isApi;
- (BOOL)isBooking;
- (BOOL)isOpentable;
- (BOOL)isBookingSearch;
- (BOOL)isHTMLDescription;
- (BOOL)isMyPosition;
- (BOOL)isRoutePoint;

// Coordinates
- (m2::PointD const &)mercator;
- (ms::LatLon)latLon;

- (NSString *)statisticsTags;

// TODO(Vlad): Use MWMSettings to store coordinate format.
+ (void)toggleCoordinateSystem;

@end
