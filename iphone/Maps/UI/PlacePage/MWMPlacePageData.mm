#import "MWMPlacePageData.h"
#import "AppInfo.h"
#import "MWMBannerHelpers.h"
#import "MWMLocationManager.h"
#import "MWMNetworkPolicy.h"
#import "MWMSettings.h"
#import "Statistics.h"
#import "SwiftBridge.h"

#import <FBAudienceNetwork/FBAudienceNetwork.h>

#include "Framework.h"

#include "partners_api/banner.hpp"

#include "base/string_utils.hpp"

#include "3party/opening_hours/opening_hours.hpp"

namespace
{
NSString * const kUserDefaultsLatLonAsDMSKey = @"UserDefaultsLatLonAsDMS";

}  // namespace

using namespace place_page;

@interface MWMPlacePageData ()

@property(copy, nonatomic) NSString * cachedMinPrice;
@property(nonatomic) id<MWMBanner> nativeAd;
@property(copy, nonatomic) NSArray<MWMGalleryItemModel *> * photos;
@property(copy, nonatomic) NSArray<MWMViatorItemModel *> * viatorItems;
@property(nonatomic) NSNumberFormatter * currencyFormatter;

@end

@implementation MWMPlacePageData
{
  Info m_info;

  std::vector<Sections> m_sections;
  std::vector<PreviewRows> m_previewRows;
  std::vector<ViatorRow> m_viatorRows;
  std::vector<MetainfoRows> m_metainfoRows;
  std::vector<AdRows> m_adRows;
  std::vector<ButtonsRows> m_buttonsRows;
  std::vector<HotelPhotosRow> m_hotelPhotosRows;
  std::vector<HotelDescriptionRow> m_hotelDescriptionRows;
  std::vector<HotelFacilitiesRow> m_hotelFacilitiesRows;
  std::vector<HotelReviewsRow> m_hotelReviewsRows;

  booking::HotelInfo m_hotelInfo;
  std::vector<viator::Product> m_viatorProducts;
}

- (instancetype)initWithPlacePageInfo:(Info const &)info
{
  self = [super init];
  if (self)
    m_info = info;

  return self;
}

- (void)fillSections
{
  m_sections.clear();
  m_previewRows.clear();
  m_metainfoRows.clear();
  m_adRows.clear();
  m_buttonsRows.clear();
  m_hotelPhotosRows.clear();
  m_hotelDescriptionRows.clear();
  m_hotelReviewsRows.clear();
  m_hotelFacilitiesRows.clear();

  m_sections.push_back(Sections::Preview);
  [self fillPreviewSection];

  // It's bookmark.
  if (m_info.IsBookmark())
    m_sections.push_back(Sections::Bookmark);

  // There is always at least coordinate meta field.
  m_sections.push_back(Sections::Metainfo);
  [self fillMetaInfoSection];

  auto const & taxiProviders = [self taxiProviders];
  if (!taxiProviders.empty())
  {
    m_sections.push_back(Sections::Ad);
    m_adRows.push_back(AdRows::Taxi);

    NSString * provider = nil;
    switch (taxiProviders.front())
    {
    case taxi::Provider::Uber: provider = kStatUber; break;
    case taxi::Provider::Yandex: provider = kStatYandex; break;
    }
    [Statistics logEvent:kStatPlacepageTaxiShow withParameters:@{ @"provider" : provider }];
  }

  // There is at least one of these buttons.
  if (m_info.ShouldShowAddPlace() || m_info.ShouldShowEditPlace() ||
      m_info.ShouldShowAddBusiness() || m_info.IsSponsored())
  {
    m_sections.push_back(Sections::Buttons);
    [self fillButtonsSection];
  }
}

- (void)fillPreviewSection
{
  if (self.title.length) m_previewRows.push_back(PreviewRows::Title);
  if (self.externalTitle.length) m_previewRows.push_back(PreviewRows::ExternalTitle);
  if (self.subtitle.length || self.isMyPosition) m_previewRows.push_back(PreviewRows::Subtitle);
  if (self.schedule != OpeningHours::Unknown) m_previewRows.push_back(PreviewRows::Schedule);
  if (self.isBooking) m_previewRows.push_back(PreviewRows::Booking);
  if (self.address.length) m_previewRows.push_back(PreviewRows::Address);
  
  NSAssert(!m_previewRows.empty(), @"Preview row's can't be empty!");
  m_previewRows.push_back(PreviewRows::Space);
  if (network_policy::CanUseNetwork() && ![MWMSettings adForbidden] && m_info.HasBanner() &&
      ![self isViator])
  {
    __weak auto wSelf = self;
    [[MWMBannersCache cache]
        getWithCoreBanners:banner_helpers::MatchPriorityBanners(m_info.GetBanners())
                 cacheOnly:NO
                   loadNew:YES
                completion:^(id<MWMBanner> ad, BOOL isAsync) {
                  __strong auto self = wSelf;
                  if (!self)
                    return;

                  self.nativeAd = ad;
                  self->m_previewRows.push_back(PreviewRows::Banner);
                  if (isAsync)
                    self.bannerIsReadyCallback();
                }];
  }
}

- (void)fillMetaInfoSection
{
  using namespace osm;
  auto const availableProperties = m_info.AvailableProperties();
  // We can't match each metadata property to its UI field and thats why we need to use our own
  // enum.
  for (auto const p : availableProperties)
  {
    switch (p)
    {
    case Props::OpeningHours: m_metainfoRows.push_back(MetainfoRows::OpeningHours); break;
    case Props::Phone: m_metainfoRows.push_back(MetainfoRows::Phone); break;
    case Props::Website:
      if (!self.isBooking)
        m_metainfoRows.push_back(MetainfoRows::Website);
      break;
    case Props::Email: m_metainfoRows.push_back(MetainfoRows::Email); break;
    case Props::Cuisine: m_metainfoRows.push_back(MetainfoRows::Cuisine); break;
    case Props::Operator: m_metainfoRows.push_back(MetainfoRows::Operator); break;
    case Props::Internet: m_metainfoRows.push_back(MetainfoRows::Internet); break;

    case Props::Wikipedia:
    case Props::Elevation:
    case Props::Stars:
    case Props::Flats:
    case Props::BuildingLevels:
    case Props::Fax: break;
    }
  }

  auto const address = m_info.GetAddress();
  if (!address.empty())
    m_metainfoRows.push_back(MetainfoRows::Address);

  m_metainfoRows.push_back(MetainfoRows::Coordinate);

  switch (m_info.GetLocalAdsStatus())
  {
  case place_page::LocalAdsStatus::NotAvailable: break;
  case place_page::LocalAdsStatus::Candidate:
    m_metainfoRows.push_back(MetainfoRows::LocalAdsCandidate);
    break;
  case place_page::LocalAdsStatus::Customer:
    m_metainfoRows.push_back(MetainfoRows::LocalAdsCustomer);
    [self logLocalAdsEvent:local_ads::EventType::OpenInfo];
    break;
  }
}

- (void)fillButtonsSection
{
  // We don't have to show edit, add place or business if it's booking object.
  if (self.isBooking)
  {
    m_buttonsRows.push_back(ButtonsRows::HotelDescription);
    return;
  }

  if (m_info.ShouldShowAddPlace())
    m_buttonsRows.push_back(ButtonsRows::AddPlace);

  if (m_info.ShouldShowEditPlace())
    m_buttonsRows.push_back(ButtonsRows::EditPlace);

  if (m_info.ShouldShowAddBusiness())
    m_buttonsRows.push_back(ButtonsRows::AddBusiness);
}

- (void)fillOnlineViatorSection
{
  if (!self.isViator)
    return;

  if (Platform::ConnectionStatus() == Platform::EConnectionType::CONNECTION_NONE)
    return;

  network_policy::CallPartnersApi([self](platform::NetworkPolicy const & canUseNetwork) {
    auto api = GetFramework().GetViatorApi(canUseNetwork);
    if (!api)
      return;

    std::string const currency = self.currencyFormatter.currencyCode.UTF8String;
    std::string const viatorId = [self sponsoredId].UTF8String;

    api->GetTop5Products(viatorId, currency,
      [viatorId, self](std::string const & destId, std::vector<viator::Product> const & products)
      {
        if (viatorId != destId)
          return;
        dispatch_async(dispatch_get_main_queue(), [products, self] {
          m_viatorProducts = products;

          auto & sections = self->m_sections;
          auto const begin = sections.begin();
          auto const end = sections.end();

          sections.insert(find(begin, end, Sections::Preview) + 1, Sections::Viator);
          m_viatorRows.emplace_back(ViatorRow::Regular);

          self.sectionsAreReadyCallback({1, 1}, self);
        });
      });
  });
}

- (void)fillOnlineBookingSections
{
  if (!self.isBooking)
    return;

  if (Platform::ConnectionStatus() == Platform::EConnectionType::CONNECTION_NONE)
    return;

  network_policy::CallPartnersApi([self](platform::NetworkPolicy const & canUseNetwork)
  {
    auto api = GetFramework().GetBookingApi(canUseNetwork);
    if (!api)
      return;

    std::string const hotelId = self.sponsoredId.UTF8String;
    api->GetHotelInfo(hotelId, [[AppInfo sharedInfo] twoLetterLanguageId].UTF8String,
                      [hotelId, self](booking::HotelInfo const & hotelInfo)
    {
      if (hotelId != hotelInfo.m_hotelId)
        return;

      dispatch_async(dispatch_get_main_queue(), [hotelInfo, self]
      {
        m_hotelInfo = hotelInfo;

        auto & sections = self->m_sections;
        auto const begin = sections.begin();
        auto const end = sections.end();

        NSUInteger const position = find(begin, end, Sections::Bookmark) != end ? 2 : 1;
        NSUInteger length = 0;
        auto it = m_sections.begin() + position;

        if (!hotelInfo.m_photos.empty())
        {
          it = sections.insert(it, Sections::HotelPhotos) + 1;
          m_hotelPhotosRows.emplace_back(HotelPhotosRow::Regular);
          length++;
        }

        if (!hotelInfo.m_description.empty())
        {
          it = sections.insert(it, Sections::HotelDescription) + 1;
          m_hotelDescriptionRows.emplace_back(HotelDescriptionRow::Regular);
          m_hotelDescriptionRows.emplace_back(HotelDescriptionRow::ShowMore);
          length++;
        }

        auto const & facilities = hotelInfo.m_facilities;
        if (!facilities.empty())
        {
          it = sections.insert(it, Sections::HotelFacilities) + 1;
          auto & facilitiesRows = self->m_hotelFacilitiesRows;
          auto const size = facilities.size();
          auto constexpr maxNumberOfHotelCellsInPlacePage = 3UL;

          if (size > maxNumberOfHotelCellsInPlacePage)
          {
            facilitiesRows.insert(facilitiesRows.begin(), maxNumberOfHotelCellsInPlacePage, HotelFacilitiesRow::Regular);
            facilitiesRows.emplace_back(HotelFacilitiesRow::ShowMore);
          }
          else
          {
            facilitiesRows.insert(facilitiesRows.begin(), size, HotelFacilitiesRow::Regular);
          }

          length++;
        }

        auto const & reviews = hotelInfo.m_reviews;
        if (!reviews.empty())
        {
          sections.insert(it, Sections::HotelReviews);
          auto const size = reviews.size();
          auto & reviewsRows = self->m_hotelReviewsRows;

          reviewsRows.emplace_back(HotelReviewsRow::Header);
          reviewsRows.insert(reviewsRows.end(), size, HotelReviewsRow::Regular);
          reviewsRows.emplace_back(HotelReviewsRow::ShowMore);
          length++;
        }

        self.sectionsAreReadyCallback({position, length}, self);
      });
    });
  });
}

- (void)updateBookmarkStatus:(BOOL)isBookmark
{
  auto & f = GetFramework();
  auto & bmManager = f.GetBookmarkManager();
  if (isBookmark)
  {
    auto const categoryIndex = f.LastEditedBMCategory();
    BookmarkData bmData{m_info.FormatNewBookmarkName(), f.LastEditedBMType()};
    auto const bookmarkIndex = bmManager.AddBookmark(categoryIndex, self.mercator, bmData);

    auto category = f.GetBmCategory(categoryIndex);
    NSAssert(category, @"Category can't be nullptr!");
    {
      BookmarkCategory::Guard guard(*category);
      auto bookmark = static_cast<Bookmark const *>(guard.m_controller.GetUserMark(bookmarkIndex));
      f.FillBookmarkInfo(*bookmark, {bookmarkIndex, categoryIndex}, m_info);
    }
    m_sections.insert(m_sections.begin() + 1, Sections::Bookmark);
  }
  else
  {
    auto const & bac = m_info.GetBookmarkAndCategory();
    auto category = bmManager.GetBmCategory(bac.m_categoryIndex);
    NSAssert(category, @"Category can't be nullptr!");
    {
      BookmarkCategory::Guard guard(*category);
      guard.m_controller.DeleteUserMark(bac.m_bookmarkIndex);
    }
    category->SaveToKMLFile();

    m_info.m_bac = {};
    m_sections.erase(remove(m_sections.begin(), m_sections.end(), Sections::Bookmark));
  }
}

- (void)dealloc
{
  auto nativeAd = self.nativeAd;
  if (nativeAd)
    [[MWMBannersCache cache] bannerIsOutOfScreenWithCoreBanner:nativeAd];
}

#pragma mark - Getters

- (storage::TCountryId const &)countryId { return m_info.m_countryId; }
- (FeatureID const &)featureId { return m_info.GetID(); }
- (NSString *)title
{
  auto title = @(m_info.GetTitle().c_str());
  if (m_info.IsBookmark() && [title isEqualToString:L(@"placepage_unknown_place")])
  {
    auto bookmarkTitle = @(m_info.m_bookmarkTitle.c_str());
    auto wCharacters = [NSCharacterSet whitespaceAndNewlineCharacterSet];
    if ([bookmarkTitle stringByTrimmingCharactersInSet:wCharacters].length != 0)
      title = bookmarkTitle;
  }
  return title;
}
- (NSString *)subtitle { return @(m_info.GetSubtitle().c_str()); }
- (place_page::OpeningHours)schedule;
{
  using type = place_page::OpeningHours;
  auto const raw = m_info.GetOpeningHours();
  if (raw.empty())
    return type::Unknown;

  auto const t = time(nullptr);
  osmoh::OpeningHours oh(raw);
  if (!oh.IsValid())
    return type::Unknown;
  if (oh.IsTwentyFourHours())
    return type::AllDay;
  if (oh.IsOpen(t))
    return type::Open;
  if (oh.IsClosed(t))
    return type::Closed;

  return type::Unknown;
}

#pragma mark - Sponsored

- (NSString *)bookingRating { return self.isBooking ? @(m_info.GetRatingFormatted().c_str()) : nil; }
- (NSString *)bookingApproximatePricing { return self.isBooking ? @(m_info.GetApproximatePricing().c_str()) : nil; }
- (NSURL *)sponsoredURL
{
  if (m_info.IsSponsored())
  {
    auto urlString = [@(m_info.GetSponsoredUrl().c_str())
        stringByAddingPercentEncodingWithAllowedCharacters:NSCharacterSet
                                                               .URLQueryAllowedCharacterSet];
    auto url = [NSURL URLWithString:urlString];
    return url;
  }
  return nil;
}

- (NSURL *)sponsoredDescriptionURL
{
  return m_info.IsSponsored()
             ? [NSURL URLWithString:@(m_info.GetSponsoredDescriptionUrl().c_str())]
             : nil;
}

- (NSURL *)bookingSearchURL
{
  auto const & url = m_info.GetBookingSearchUrl();
  return url.empty() ? nil : [NSURL URLWithString:@(url.c_str())];
}

- (NSString *)sponsoredId
{
  return m_info.IsSponsored()
             ? @(m_info.GetMetadata().Get(feature::Metadata::FMD_SPONSORED_ID).c_str())
             : nil;
}

- (void)assignOnlinePriceToLabel:(UILabel *)label
{
  NSAssert(self.isBooking, @"Online price must be assigned to booking object!");
  if (self.cachedMinPrice.length)
  {
    label.text = self.cachedMinPrice;
    return;
  }

  if (Platform::ConnectionStatus() == Platform::EConnectionType::CONNECTION_NONE)
    return;

  std::string const currency = self.currencyFormatter.currencyCode.UTF8String;

  auto const func = [self, label, currency](std::string const & hotelId, std::string const & minPrice,
                                            std::string const & priceCurrency) {
    if (currency != priceCurrency)
      return;

    NSNumberFormatter * decimalFormatter = [[NSNumberFormatter alloc] init];
    decimalFormatter.numberStyle = NSNumberFormatterDecimalStyle;

    NSNumber * currencyNumber = [decimalFormatter
        numberFromString:[@(minPrice.c_str())
                             stringByReplacingOccurrencesOfString:@"."
                                                       withString:decimalFormatter
                                                                      .decimalSeparator]];
    NSString * currencyString = [self.currencyFormatter stringFromNumber:currencyNumber];

    NSString * pattern =
        [L(@"place_page_starting_from") stringByReplacingOccurrencesOfString:@"%s"
                                                                  withString:@"%@"];

    self.cachedMinPrice = [NSString stringWithFormat:pattern, currencyString];
    dispatch_async(dispatch_get_main_queue(), ^{
      label.text = self.cachedMinPrice;
    });
  };

  network_policy::CallPartnersApi(
      [self, currency, func](platform::NetworkPolicy const & canUseNetwork) {
        auto const api = GetFramework().GetBookingApi(canUseNetwork);
        if (api)
          api->GetMinPrice(self.sponsoredId.UTF8String, currency, func);
  });
}

- (NSNumberFormatter *)currencyFormatter
{
  if (!_currencyFormatter)
  {
    _currencyFormatter = [[NSNumberFormatter alloc] init];
    if (_currencyFormatter.currencyCode.length != 3)
      _currencyFormatter.locale = [[NSLocale alloc] initWithLocaleIdentifier:@"en_US"];

    _currencyFormatter.numberStyle = NSNumberFormatterCurrencyStyle;
    _currencyFormatter.maximumFractionDigits = 0;
  }
  return _currencyFormatter;
}

- (NSString *)hotelDescription { return @(m_hotelInfo.m_description.c_str()); }
- (std::vector<booking::HotelFacility> const &)facilities { return m_hotelInfo.m_facilities; }
- (std::vector<booking::HotelReview> const &)reviews { return m_hotelInfo.m_reviews; }
- (NSUInteger)numberOfReviews { return m_hotelInfo.m_scoreCount; }
- (NSURL *)URLToAllReviews { return [NSURL URLWithString:@(m_info.GetSponsoredReviewUrl().c_str())]; }
- (NSArray<MWMGalleryItemModel *> *)photos
{
  if (_photos)
    return _photos;

  NSMutableArray<MWMGalleryItemModel *> * res = [@[] mutableCopy];
  for (auto const & p : m_hotelInfo.m_photos)
  {
    auto big = [NSURL URLWithString:@(p.m_original.c_str())];
    auto preview = [NSURL URLWithString:@(p.m_small.c_str())];
    if (!big || !preview)
      continue;
    
    auto photo = [[MWMGalleryItemModel alloc] initWithImageURL:big previewURL:preview];
    [res addObject:photo];
  }

  self.photos = res;
  return _photos;
}

- (NSArray<MWMViatorItemModel *> *)viatorItems
{
  if (!_viatorItems)
  {
    NSMutableArray<MWMViatorItemModel *> * items = [@[] mutableCopy];
    for (auto const & p : m_viatorProducts)
    {
      auto imageURL = [NSURL URLWithString:@(p.m_photoUrl.c_str())];
      auto pageURL = [NSURL URLWithString:@(p.m_pageUrl.c_str())];
      if (!imageURL || !pageURL)
        continue;
      auto item = [[MWMViatorItemModel alloc] initWithImageURL:imageURL
                                                       pageURL:pageURL
                                                         title:@(p.m_title.c_str())
                                                        rating:p.m_rating
                                                      duration:@(p.m_duration.c_str())
                                                         price:@(p.m_priceFormatted.c_str())];
      [items addObject:item];
    }
    self.viatorItems = items;
  }
  return _viatorItems;
}

#pragma mark - Bookmark

- (NSString *)externalTitle
{
  if (m_info.IsBookmark())
  {
    auto bookmarkTitle = @(m_info.m_bookmarkTitle.c_str());
    if (![[self title] isEqualToString:bookmarkTitle])
      return bookmarkTitle;
  }

  auto const secondaryTitle = m_info.GetSecondaryTitle();
  if (!secondaryTitle.empty())
    return @(secondaryTitle.c_str());

  return nil;
}

- (NSString *)bookmarkColor
{
  return m_info.IsBookmark() ? @(m_info.m_bookmarkColorName.c_str()) : nil;
}

- (NSString *)bookmarkDescription
{
  return m_info.IsBookmark() ? @(m_info.m_bookmarkDescription.c_str()) : nil;
}

- (NSString *)bookmarkCategory
{
  return m_info.IsBookmark() ? @(m_info.m_bookmarkCategoryName.c_str()) : nil;
}

- (BookmarkAndCategory)bac;
{
  return m_info.IsBookmark() ? m_info.m_bac : BookmarkAndCategory();
}

#pragma mark - Local Ads
- (NSString *)localAdsURL { return @(m_info.GetLocalAdsUrl().c_str()); }
- (void)logLocalAdsEvent:(local_ads::EventType)type
{
  if (m_info.GetLocalAdsStatus() != place_page::LocalAdsStatus::Customer)
    return;
  auto const featureID = m_info.GetID();
  auto const & mwmInfo = featureID.m_mwmId.GetInfo();
  if (!mwmInfo)
    return;
  auto & f = GetFramework();
  auto location = [MWMLocationManager lastLocation];
  auto event = local_ads::Event(type, mwmInfo->GetVersion(), mwmInfo->GetCountryName(),
                                featureID.m_index, f.GetDrawScale(),
                                local_ads::Clock::now(), location.coordinate.latitude,
                                location.coordinate.longitude, location.horizontalAccuracy);
  f.GetLocalAdsManager().GetStatistics().RegisterEvent(std::move(event));
}

#pragma mark - Taxi
- (std::vector<taxi::Provider::Type> const &)taxiProviders
{
  return m_info.ReachableByTaxiProviders();
}

#pragma mark - Getters

- (RouteMarkType)routeMarkType { return m_info.m_routeMarkType; }
- (int8_t)intermediateIndex { return m_info.m_intermediateIndex; }
- (NSString *)address { return @(m_info.GetAddress().c_str()); }
- (NSString *)apiURL { return @(m_info.GetApiUrl().c_str()); }
- (std::vector<Sections> const &)sections { return m_sections; }
- (std::vector<PreviewRows> const &)previewRows { return m_previewRows; }
- (std::vector<place_page::ViatorRow> const &)viatorRows { return m_viatorRows; }
- (std::vector<MetainfoRows> const &)metainfoRows { return m_metainfoRows; }
- (std::vector<MetainfoRows> &)mutableMetainfoRows { return m_metainfoRows; }
- (std::vector<AdRows> const &)adRows { return m_adRows; }
- (std::vector<ButtonsRows> const &)buttonsRows { return m_buttonsRows; }
- (std::vector<HotelPhotosRow> const &)photosRows { return m_hotelPhotosRows; }
- (std::vector<HotelDescriptionRow> const &)descriptionRows { return m_hotelDescriptionRows; }
- (std::vector<HotelFacilitiesRow> const &)hotelFacilitiesRows { return m_hotelFacilitiesRows; }
- (std::vector<HotelReviewsRow> const &)hotelReviewsRows { return m_hotelReviewsRows; }
- (NSString *)stringForRow:(MetainfoRows)row
{
  switch (row)
  {
  case MetainfoRows::ExtendedOpeningHours: return nil;
  case MetainfoRows::OpeningHours: return @(m_info.GetOpeningHours().c_str());
  case MetainfoRows::Phone: return @(m_info.GetPhone().c_str());
  case MetainfoRows::Address: return @(m_info.GetAddress().c_str());
  case MetainfoRows::Website: return @(m_info.GetWebsite().c_str());
  case MetainfoRows::Email: return @(m_info.GetEmail().c_str());
  case MetainfoRows::Cuisine:
    return @(strings::JoinStrings(m_info.GetLocalizedCuisines(), Info::kSubtitleSeparator).c_str());
  case MetainfoRows::Operator: return @(m_info.GetOperator().c_str());
  case MetainfoRows::Internet: return L(@"WiFi_available");
  case MetainfoRows::LocalAdsCandidate: return L(@"create_campaign_button");
  case MetainfoRows::LocalAdsCustomer: return L(@"view_campaign_button");
  case MetainfoRows::Coordinate:
    return @(m_info
                 .GetFormattedCoordinate(
                     [[NSUserDefaults standardUserDefaults] boolForKey:kUserDefaultsLatLonAsDMSKey])
                 .c_str());
  }
}

#pragma mark - Helpres

- (NSString *)phoneNumber { return @(m_info.GetPhone().c_str()); }
- (BOOL)isBookmark { return m_info.IsBookmark(); }
- (BOOL)isApi { return m_info.HasApiUrl(); }
- (BOOL)isBooking { return m_info.m_sponsoredType == SponsoredType::Booking; }
- (BOOL)isOpentable { return m_info.m_sponsoredType == SponsoredType::Opentable; }
- (BOOL)isViator { return m_info.m_sponsoredType == SponsoredType::Viator; }
- (BOOL)isBookingSearch { return !m_info.GetBookingSearchUrl().empty(); }
- (BOOL)isMyPosition { return m_info.IsMyPosition(); }
- (BOOL)isHTMLDescription { return strings::IsHTML(m_info.m_bookmarkDescription); }
- (BOOL)isRoutePoint { return m_info.IsRoutePoint(); }

#pragma mark - Coordinates

- (m2::PointD const &)mercator { return m_info.GetMercator(); }
- (ms::LatLon)latLon { return m_info.GetLatLon(); }
+ (void)toggleCoordinateSystem
{
  // TODO: Move changing latlon's mode to the settings.
  NSUserDefaults * ud = [NSUserDefaults standardUserDefaults];
  [ud setBool:![ud boolForKey:kUserDefaultsLatLonAsDMSKey] forKey:kUserDefaultsLatLonAsDMSKey];
  [ud synchronize];
}

#pragma mark - Stats

- (NSString *)statisticsTags
{
  NSMutableArray<NSString *> * result = [@[] mutableCopy];
  for (auto const & s : m_info.GetRawTypes())
    [result addObject:@(s.c_str())];
  return [result componentsJoinedByString:@", "];
}

@end
