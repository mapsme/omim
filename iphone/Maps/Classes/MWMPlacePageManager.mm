#import "MWMPlacePageManager.h"
#import <Pushwoosh/PushNotificationManager.h>
#import "MWMAPIBar.h"
#import "MWMActivityViewController.h"
#import "MWMCircularProgress.h"
#import "MWMEditBookmarkController.h"
#import "MWMFrameworkListener.h"
#import "MWMFrameworkObservers.h"
#import "MWMLocationManager.h"
#import "MWMPlacePageData.h"
#import "MWMPlacePageLayout.h"
#import "MWMRouter.h"
#import "MWMStorage.h"
#import "MWMViewController.h"
#import "MapViewController.h"
#import "Statistics.h"

#include "geometry/distance_on_sphere.hpp"

#include "platform/measurement_utils.hpp"

@interface MWMPlacePageManager ()<MWMFrameworkStorageObserver, MWMPlacePageLayoutDelegate,
                                  MWMPlacePageLayoutDataSource, MWMLocationObserver>

@property(nonatomic) MWMPlacePageEntity * entity;

@property(nonatomic) MWMPlacePageLayout * layout;
@property(nonatomic) MWMPlacePageData * data;

@property(nonatomic) storage::NodeStatus currentDownloaderStatus;

@end

@implementation MWMPlacePageManager

- (void)showPlacePage:(place_page::Info const &)info
{
  self.currentDownloaderStatus = storage::NodeStatus::Undefined;
  [MWMFrameworkListener addObserver:self];

  self.data = [[MWMPlacePageData alloc] initWithPlacePageInfo:info];

  if (!self.layout)
  {
    self.layout = [[MWMPlacePageLayout alloc] initWithOwnerView:self.ownerViewController.view
                                                       delegate:self
                                                     dataSource:self];
  }

  [self.layout showWithData:self.data];

  // Call for the first time to produce changes
  [self processCountryEvent:self.data.countryId];

  if (![MWMLocationManager lastHeading])
    return;

  [MWMLocationManager addObserver:self];
  [self.layout setDistanceToObject:self.distanceToObject];
}

- (void)closePlacePage
{
  [self.layout close];
  [MWMLocationManager removeObserver:self];
  [MWMFrameworkListener removeObserver:self];
}

#pragma mark - MWMPlacePageLayoutDataSource

- (void)downloadSelectedArea
{
  auto const & countryId = self.data.countryId;
  NodeAttrs nodeAttrs;
  GetFramework().GetStorage().GetNodeAttrs(countryId, nodeAttrs);
  MWMAlertViewController * avc = [MapViewController controller].alertController;
  switch (nodeAttrs.m_status)
  {
  case NodeStatus::NotDownloaded:
  case NodeStatus::Partly:
    [MWMStorage downloadNode:countryId alertController:avc onSuccess:nil];
    break;
  case NodeStatus::Undefined:
  case NodeStatus::Error: [MWMStorage retryDownloadNode:countryId]; break;
  case NodeStatus::OnDiskOutOfDate: [MWMStorage updateNode:countryId alertController:avc]; break;
  case NodeStatus::Downloading:
  case NodeStatus::InQueue: [MWMStorage cancelDownloadNode:countryId]; break;
  case NodeStatus::OnDisk: break;
  }
}

- (NSString *)distanceToObject
{
  CLLocation * lastLocation = [MWMLocationManager lastLocation];
  if (!lastLocation)
    return @"";
  string distance;
  CLLocationCoordinate2D const coord = lastLocation.coordinate;
  ms::LatLon const target = self.data.latLon;
  measurement_utils::FormatDistance(
      ms::DistanceOnEarth(coord.latitude, coord.longitude, target.lat, target.lon), distance);
  return @(distance.c_str());
}

#pragma mark - MWMFrameworkStorageObserver

- (void)processCountryEvent:(TCountryId const &)countryId
{
  if (countryId == kInvalidCountryId)
  {
    [self.layout processDownloaderEventWithStatus:storage::NodeStatus::Undefined progress:0];
    return;
  }

  if (self.data.countryId != countryId)
    return;

  NodeStatuses statuses;
  GetFramework().GetStorage().GetNodeStatuses(countryId, statuses);

  auto const status = statuses.m_status;
  if (status == self.currentDownloaderStatus)
    return;

  self.currentDownloaderStatus = status;
  [self.layout processDownloaderEventWithStatus:status progress:0];
}

- (void)processCountry:(TCountryId const &)countryId
              progress:(MapFilesDownloader::TProgress const &)progress
{
  if (countryId == kInvalidCountryId || self.data.countryId != countryId)
    return;

  [self.layout
      processDownloaderEventWithStatus:storage::NodeStatus::Downloading
                              progress:static_cast<CGFloat>(progress.first) / progress.second];
}

#pragma mark - MWMPlacePageLayout

- (void)onTopBoundChanged:(CGFloat)bound
{
  [[MWMMapViewControlsManager manager]
      dragPlacePage:{{0, self.ownerViewController.view.height - bound}, {}}];
}

- (void)shouldDestroyLayout
{
  self.layout = nil;
  GetFramework().DeactivateMapSelection(false);
}

#pragma mark - MWMLocationObserver

- (void)onHeadingUpdate:(location::CompassInfo const &)info
{
  CLLocation * lastLocation = [MWMLocationManager lastLocation];
  if (!lastLocation)
    return;

  CGFloat const angle = ang::AngleTo(lastLocation.mercator, self.data.mercator) + info.m_bearing;
  [self.layout rotateDirectionArrowToAngle:angle];
}

- (void)onLocationUpdate:(location::GpsInfo const &)locationInfo
{
  [self.layout setDistanceToObject:self.distanceToObject];
}

- (void)mwm_refreshUI { [self.layout mwm_refreshUI]; }
- (void)dismissPlacePage { [self closePlacePage]; }
- (void)hidePlacePage { [self closePlacePage]; }
- (void)routeFrom
{
  [Statistics logEvent:kStatEventName(kStatPlacePage, kStatBuildRoute)
        withParameters:@{kStatValue : kStatSource}];
  [[MWMRouter router] buildFromPoint:self.target bestRouter:YES];
  [self closePlacePage];
}

- (void)routeTo
{
  [Statistics logEvent:kStatEventName(kStatPlacePage, kStatBuildRoute)
        withParameters:@{kStatValue : kStatDestination}];
  [[MWMRouter router] buildToPoint:self.target bestRouter:YES];
  [self closePlacePage];
}

- (MWMRoutePoint)target
{
  NSString * name = nil;
  if (self.data.title.length > 0)
    name = self.data.title;
  else if (self.data.address.length > 0)
    name = self.data.address;
  else if (self.data.subtitle.length > 0)
    name = self.data.subtitle;
  else if (self.data.isBookmark)
    name = self.data.externalTitle;
  else
    name = L(@"placepage_unknown_place");

  m2::PointD const & org = self.data.mercator;
  return self.data.isMyPosition ? MWMRoutePoint(org) : MWMRoutePoint(org, name);
}

- (void)share
{
  [Statistics logEvent:kStatEventName(kStatPlacePage, kStatShare)];
  MWMActivityViewController * shareVC = [MWMActivityViewController
      shareControllerForPlacePageObject:static_cast<id<MWMPlacePageObject>>(self.data)];
  [shareVC presentInParentViewController:self.ownerViewController
                              anchorView:self.layout.shareAnchor];
}

- (void)editPlace
{
  [Statistics logEvent:kStatEventName(kStatPlacePage, kStatEdit)];
  [[PushNotificationManager pushManager] setTags:@{ @"editor_edit_discovered" : @YES }];
  [(MapViewController *)self.ownerViewController openEditor];
}

- (void)addBusiness
{
  [Statistics logEvent:kStatEditorAddClick withParameters:@{kStatValue : kStatPlacePage}];
  [[MWMMapViewControlsManager manager] addPlace:YES hasPoint:NO point:m2::PointD()];
}

- (void)addPlace
{
  [Statistics logEvent:kStatEditorAddClick
        withParameters:@{kStatValue : kStatPlacePageNonBuilding}];
  [[MWMMapViewControlsManager manager] addPlace:NO hasPoint:YES point:self.data.mercator];
}

- (void)addBookmark
{
  [self.data updateBookmarkStatus:YES];
  [self.layout reloadBookmarkSection:YES];
}

- (void)removeBookmark
{
  [Statistics logEvent:kStatEventName(kStatPlacePage, kStatBookmarks)
        withParameters:@{kStatValue : kStatRemove}];
  [self.data updateBookmarkStatus:NO];
  [self.layout reloadBookmarkSection:NO];
}

- (void)editBookmark { [[MapViewController controller] openBookmarkEditorWithData:self.data]; }
- (void)book:(BOOL)isDescription
{
  NSMutableDictionary * stat = [@{ kStatProvider : kStatBooking } mutableCopy];
  MWMPlacePageData * data = self.data;
  auto const latLon = data.latLon;
  stat[kStatHotel] = data.hotelId;
  stat[kStatHotelLat] = @(latLon.lat);
  stat[kStatHotelLon] = @(latLon.lon);
  [Statistics logEvent:isDescription ? kPlacePageHotelDetails : kPlacePageHotelBook
        withParameters:stat
            atLocation:[MWMLocationManager lastLocation]];

  UIViewController * vc = static_cast<UIViewController *>([MapViewController controller]);
  NSURL * url = isDescription ? self.data.bookingDescriptionURL : self.data.bookingURL;
  NSAssert(url, @"Booking url can't be nil!");
  [vc openUrl:url];
}

- (void)call
{
  NSAssert(self.data.phoneNumber, @"Phone number can't be nil!");
  NSString * phoneNumber = [[@"telprompt:" stringByAppendingString:self.data.phoneNumber]
      stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding];
  [[UIApplication sharedApplication] openURL:[NSURL URLWithString:phoneNumber]];
}

- (void)apiBack
{
  [Statistics logEvent:kStatEventName(kStatPlacePage, kStatAPI)];
  [[UIApplication sharedApplication] openURL:[NSURL URLWithString:self.data.apiURL]];
  [[MapViewController controller].apiBar back];
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:(id<UIViewControllerTransitionCoordinator>)coordinator
{
  [coordinator
      animateAlongsideTransition:^(id<UIViewControllerTransitionCoordinatorContext> context) {
        [self.layout layoutWithSize:size];
      }
                      completion:^(id<UIViewControllerTransitionCoordinatorContext> context){
                      }];
}

#pragma mark - MWMFeatureHolder

- (FeatureID const &)featureId { return self.data.featureId; }
#pragma mark - Owner

- (MapViewController *)ownerViewController { return [MapViewController controller]; }
#pragma mark - Deprecated

@synthesize leftBound = _leftBound;
@synthesize topBound = _topBound;
- (void)setTopBound:(CGFloat)topBound { _topBound = 0; }
- (void)setLeftBound:(CGFloat)leftBound { _leftBound = 0; }
- (void)addSubviews:(NSArray *)views withNavigationController:(UINavigationController *)controller
{
}
- (void)willRotateToInterfaceOrientation:(UIInterfaceOrientation)orientation {}
@end
