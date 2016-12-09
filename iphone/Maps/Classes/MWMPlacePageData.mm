#import "MWMPlacePageData.h"
#import "MWMNetworkPolicy.h"

#include "Framework.h"

#include "base/string_utils.hpp"

#include "3party/opening_hours/opening_hours.hpp"

namespace
{
NSString * const kUserDefaultsLatLonAsDMSKey = @"UserDefaultsLatLonAsDMS";

}  // namespace

using namespace place_page;

@implementation MWMPlacePageData
{
  Info m_info;

  vector<Sections> m_sections;
  vector<PreviewRows> m_previewRows;
  vector<MetainfoRows> m_metainfoRows;
  vector<ButtonsRows> m_buttonsRows;
}

- (instancetype)initWithPlacePageInfo:(Info const &)info
{
  self = [super init];
  if (self)
  {
    m_info = info;
    [self fillSections];
  }

  return self;
}

- (void)fillSections
{
  m_sections.clear();
  m_previewRows.clear();
  m_metainfoRows.clear();
  m_buttonsRows.clear();

  m_sections.push_back(Sections::Preview);
  [self fillPreviewSection];

  // It's bookmark.
  if (m_info.IsBookmark())
    m_sections.push_back(Sections::Bookmark);

  // There is always at least coordinate meta field.
  m_sections.push_back(Sections::Metainfo);
  [self fillMetaInfoSection];

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
  if (self.subtitle.length) m_previewRows.push_back(PreviewRows::Subtitle);
  if (self.schedule != OpeningHours::Unknown) m_previewRows.push_back(PreviewRows::Schedule);
  if (self.isBooking) m_previewRows.push_back(PreviewRows::Booking);
  if (self.address.length) m_previewRows.push_back(PreviewRows::Address);
  
  NSAssert(!m_previewRows.empty(), @"Preview row's can't be empty!");
  m_previewRows.push_back(PreviewRows::Space);
  if (m_info.HasBanner()) m_previewRows.push_back(PreviewRows::Banner);
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
    case Props::Website: m_metainfoRows.push_back(MetainfoRows::Website); break;
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
  if (m_info.IsReachableByTaxi())
    m_metainfoRows.push_back(MetainfoRows::Taxi);
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
      auto bookmark = static_cast<Bookmark *>(guard.m_controller.GetUserMarkForEdit(bookmarkIndex));
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

#pragma mark - Getters

- (storage::TCountryId const &)countryId { return m_info.m_countryId; }
- (FeatureID const &)featureId { return m_info.GetID(); }
- (NSString *)title { return @(m_info.GetTitle().c_str()); }
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

- (NSString *)bookingRating
{
  return self.isBooking ? @(m_info.GetRatingFormatted().c_str()) : nil;
}

- (NSString *)bookingApproximatePricing
{
  return self.isBooking ? @(m_info.GetApproximatePricing().c_str()) : nil;
}

- (NSURL *)sponsoredURL
{
  return m_info.IsSponsored() ? [NSURL URLWithString:@(m_info.GetSponsoredUrl().c_str())]
                                   : nil;
}

- (NSURL *)sponsoredDescriptionURL
{
  return m_info.IsSponsored()
             ? [NSURL URLWithString:@(m_info.GetSponsoredDescriptionUrl().c_str())]
             : nil;
}

- (NSString *)sponsoredId
{
  return m_info.IsSponsored()
             ? @(m_info.GetMetadata().Get(feature::Metadata::FMD_SPONSORED_ID).c_str())
             : nil;
}

- (NSString *)bannerTitle
{
  return m_info.HasBanner() ? @(m_info.GetBannerTitleId().c_str()) : nil;
}

- (NSString *)bannerContent
{
  return m_info.HasBanner() ? @(m_info.GetBannerMessageId().c_str()) : nil;
}

- (NSURL *)bannerIconURL
{
  return m_info.HasBanner() ? [NSURL URLWithString:@(m_info.GetBannerIconId().c_str())] : nil;
}

- (NSURL *)bannerURL
{
  return m_info.HasBanner() ? [NSURL URLWithString:@(m_info.GetBannerUrl().c_str())] : nil;
}

- (void)assignOnlinePriceToLabel:(UILabel *)label
{
  NSAssert(self.isBooking, @"Online price must be assigned to booking object!");
  if (Platform::ConnectionStatus() == Platform::EConnectionType::CONNECTION_NONE)
    return;

  NSNumberFormatter * currencyFormatter = [[NSNumberFormatter alloc] init];
  if (currencyFormatter.currencyCode.length != 3)
    currencyFormatter.locale = [[NSLocale alloc] initWithLocaleIdentifier:@"en_US"];

  currencyFormatter.numberStyle = NSNumberFormatterCurrencyStyle;
  currencyFormatter.maximumFractionDigits = 0;

  string const currency = currencyFormatter.currencyCode.UTF8String;

  auto const func = [self, label, currency, currencyFormatter](string const & minPrice,
                                                               string const & priceCurrency) {
    if (currency != priceCurrency)
      return;

    NSNumberFormatter * decimalFormatter = [[NSNumberFormatter alloc] init];
    decimalFormatter.numberStyle = NSNumberFormatterDecimalStyle;

    NSString * currencyString = [currencyFormatter
        stringFromNumber:
            [decimalFormatter
                numberFromString:[@(minPrice.c_str())
                                     stringByReplacingOccurrencesOfString:@"."
                                                               withString:decimalFormatter
                                                                              .decimalSeparator]]];

    NSString * pattern =
        [L(@"place_page_starting_from") stringByReplacingOccurrencesOfString:@"%s"
                                                                  withString:@"%@"];

    dispatch_async(dispatch_get_main_queue(), ^{
      label.text = [NSString stringWithFormat:pattern, currencyString];
    });
  };

  network_policy::CallPartnersApi(
      [self, currency, func](platform::NetworkPolicy const & canUseNetwork) {
        auto const api = GetFramework().GetBookingApi(canUseNetwork);
        if (api)
          api->GetMinPrice(self.sponsoredId.UTF8String, currency, func);
      });
}

- (NSString *)address { return @(m_info.GetAddress().c_str()); }
- (NSString *)apiURL { return @(m_info.GetApiUrl().c_str()); }
- (NSString *)externalTitle
{
  return m_info.IsBookmark() && m_info.m_bookmarkTitle != m_info.GetTitle()
             ? @(m_info.m_bookmarkTitle.c_str())
             : nil;
}

- (NSString *)bookmarkColor
{
  return m_info.IsBookmark() ? @(m_info.m_bookmarkColorName.c_str()) : nil;
  ;
}

- (NSString *)bookmarkDescription
{
  return m_info.IsBookmark() ? @(m_info.m_bookmarkDescription.c_str()) : nil;
}

- (NSString *)bookmarkCategory
{
  return m_info.IsBookmark() ? @(m_info.m_bookmarkCategoryName.c_str()) : nil;
  ;
}

- (BookmarkAndCategory)bac;
{
  return m_info.IsBookmark() ? m_info.m_bac : BookmarkAndCategory();
}

- (vector<Sections> const &)sections { return m_sections; }
- (vector<PreviewRows> const &)previewRows { return m_previewRows; }
- (vector<MetainfoRows> const &)metainfoRows { return m_metainfoRows; }
- (vector<MetainfoRows> &)mutableMetainfoRows { return m_metainfoRows; }
- (vector<ButtonsRows> const &)buttonsRows { return m_buttonsRows; }
- (NSString *)stringForRow:(MetainfoRows)row
{
  switch (row)
  {
  case MetainfoRows::Taxi:
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
- (BOOL)isMyPosition { return m_info.IsMyPosition(); }
- (BOOL)isHTMLDescription { return strings::IsHTML(m_info.m_bookmarkDescription); }

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

- (NSArray<NSString *> *)statisticsTags
{
  NSMutableArray<NSString *> * result = [@[] mutableCopy];
  for (auto const & s : m_info.GetRawTypes())
    [result addObject:@(s.c_str())];
  return result.copy;
}

@end
