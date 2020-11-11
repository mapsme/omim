#import "MWMMapViewControlsManager.h"
#import "MWMAddPlaceNavigationBar.h"
#import "MWMMapDownloadDialog.h"
#import "MWMMapViewControlsManager+AddPlace.h"
#import "MWMNetworkPolicy+UI.h"
#import "MWMPlacePageManager.h"
#import "MWMPlacePageProtocol.h"
#import "MWMSearchManager.h"
#import "MWMSideButtons.h"
#import "MapViewController.h"
#import "MapsAppDelegate.h"
#import "SwiftBridge.h"

#include <CoreApi/Framework.h>
#import <CoreApi/MWMFrameworkHelper.h>

#include "platform/local_country_file_utils.hpp"
#include "platform/platform.hpp"

#include "storage/storage_helpers.hpp"

#include "map/place_page_info.hpp"

namespace {
NSString *const kMapToCategorySelectorSegue = @"MapToCategorySelectorSegue";
}  // namespace

@interface MWMMapViewControlsManager () <MWMSearchManagerObserver>

@property(nonatomic) UIButton *promoButton;
@property(nonatomic) id<MWMPlacePageProtocol> placePageManager;
@property(nonatomic) MWMNavigationDashboardManager *navigationManager;
@property(nonatomic) MWMSearchManager *searchManager;

@property(weak, nonatomic) MapViewController *ownerController;

@property(nonatomic) BOOL disableStandbyOnRouteFollowing;
@property(nonatomic) PromoDiscoveryCampaign *promoDiscoveryCampaign;

@end

@implementation MWMMapViewControlsManager

+ (MWMMapViewControlsManager *)manager {
  return [MapViewController sharedController].controlsManager;
}
- (instancetype)initWithParentController:(MapViewController *)controller {
  if (!controller)
    return nil;
  self = [super init];
  if (!self)
    return nil;
  self.ownerController = controller;
  self.isDirectionViewHidden = YES;
  self.menuRestoreState = MWMBottomMenuStateInactive;
  self.promoDiscoveryCampaign = [ABTestManager manager].promoDiscoveryCampaign;
//  if (_promoDiscoveryCampaign.enabled) {
//    [controller.controlsView addSubview:self.promoButton];
//    self.promoButton.translatesAutoresizingMaskIntoConstraints = NO;
//    [NSLayoutConstraint activateConstraints:@[
//      [self.promoButton.centerXAnchor constraintEqualToAnchor:self.trafficButton.view.centerXAnchor],
//      [self.promoButton.topAnchor constraintEqualToAnchor:self.sideButtons.view.topAnchor]
//    ]];
//    [Statistics logEvent:kStatMapSponsoredButtonShow withParameters:@{kStatTarget: kStatGuidesSubscription}];
//  }
  return self;
}

- (UIStatusBarStyle)preferredStatusBarStyle {
  BOOL const isSearchUnderStatusBar = (self.searchManager.state != MWMSearchManagerStateHidden);
  BOOL const isNavigationUnderStatusBar = self.navigationManager.state != MWMNavigationDashboardStateHidden &&
                                          self.navigationManager.state != MWMNavigationDashboardStateNavigation;
  BOOL const isMenuViewUnderStatusBar = self.menuState == MWMBottomMenuStateActive;
  BOOL const isDirectionViewUnderStatusBar = !self.isDirectionViewHidden;
  BOOL const isAddPlaceUnderStatusBar =
    [self.ownerController.view hasSubviewWithViewClass:[MWMAddPlaceNavigationBar class]];
  BOOL const isNightMode = [UIColor isNightMode];
  BOOL const isSomethingUnderStatusBar = isSearchUnderStatusBar || isNavigationUnderStatusBar ||
                                         isDirectionViewUnderStatusBar || isMenuViewUnderStatusBar ||
                                         isAddPlaceUnderStatusBar;

  return isSomethingUnderStatusBar || isNightMode ? UIStatusBarStyleLightContent : UIStatusBarStyleDefault;
}

#pragma mark - Layout

- (UIView *)anchorView {
  return nil;
//  return self.tabBarController.view;
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:(id<UIViewControllerTransitionCoordinator>)coordinator {
  [self.searchManager viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
}

#pragma mark - MWMPlacePageViewManager

- (void)showPlacePageReview {
  [[MWMNetworkPolicy sharedPolicy] callOnlineApi:^(BOOL) {
    [self.placePageManager showReview];
  }];
}

- (void)searchTextOnMap:(NSString *)text forInputLocale:(NSString *)locale {
  if (![self searchText:text forInputLocale:locale])
    return;

  self.searchManager.state = MWMSearchManagerStateMapSearch;
}

- (BOOL)searchText:(NSString *)text forInputLocale:(NSString *)locale {
  if (text.length == 0)
    return NO;

  self.searchManager.state = MWMSearchManagerStateTableSearch;
  [self.searchManager searchText:text forInputLocale:locale];
  return YES;
}

- (void)hideSearch {
  self.searchManager.state = MWMSearchManagerStateHidden;
}

//#pragma mark - BottomMenuDelegate
//
- (void)actionDownloadMaps:(MWMMapDownloaderMode)mode {
  [self.ownerController openMapsDownloader:mode];
}

#pragma mark - MWMNavigationDashboardManager

- (void)setDisableStandbyOnRouteFollowing:(BOOL)disableStandbyOnRouteFollowing {
  if (_disableStandbyOnRouteFollowing == disableStandbyOnRouteFollowing)
    return;
  _disableStandbyOnRouteFollowing = disableStandbyOnRouteFollowing;
  if (disableStandbyOnRouteFollowing)
    [[MapsAppDelegate theApp] disableStandby];
  else
    [[MapsAppDelegate theApp] enableStandby];
}

#pragma mark - MWMSearchManagerObserver

- (void)onSearchManagerStateChanged {
  auto state = [MWMSearchManager manager].state;
  if (!IPAD && state == MWMSearchManagerStateHidden) {
//    self.hidden = NO;
  } else if (state != MWMSearchManagerStateHidden) {
    //    [self hideGuidesNavigationBar];
  }
}

#pragma mark - Routing

- (void)onRoutePrepare {
  auto nm = self.navigationManager;
  [nm onRoutePrepare];
  [nm onRoutePointsUpdated];
  [self.ownerController.bookmarksCoordinator close];
  self.promoButton.hidden = YES;
}

- (void)onRouteRebuild {
  if (IPAD)
    self.searchManager.state = MWMSearchManagerStateHidden;

  [self.ownerController.bookmarksCoordinator close];
  [self.navigationManager onRoutePlanning];
  self.promoButton.hidden = YES;
}

- (void)onRouteReady:(BOOL)hasWarnings {
  self.searchManager.state = MWMSearchManagerStateHidden;
  [self.navigationManager onRouteReady:hasWarnings];
  self.promoButton.hidden = YES;
}

- (void)onRouteStart {
//  self.hidden = NO;
//  self.sideButtons.zoomHidden = self.zoomHidden;
//  self.sideButtonsHidden = NO;
  self.disableStandbyOnRouteFollowing = YES;
//  self.trafficButtonHidden = YES;
  [self.navigationManager onRouteStart];
  self.promoButton.hidden = YES;
}

- (void)onRouteStop {
  self.searchManager.state = MWMSearchManagerStateHidden;
//  self.sideButtons.zoomHidden = self.zoomHidden;
  [self.navigationManager onRouteStop];
  self.disableStandbyOnRouteFollowing = NO;
//  self.trafficButtonHidden = NO;
  self.promoButton.hidden = _promoDiscoveryCampaign.hasBeenActivated;
}

#pragma mark - Properties

- (UIButton *)promoButton {
  if (!_promoButton) {
    PromoCoordinator *coordinator = [[PromoCoordinator alloc] initWithViewController:self.ownerController
                                                                            campaign:_promoDiscoveryCampaign];
    _promoButton = [[PromoButton alloc] initWithCoordinator:coordinator];
  }
  return _promoButton;
}

- (id<MWMPlacePageProtocol>)placePageManager {
  if (!_placePageManager)
    _placePageManager = [[MWMPlacePageManager alloc] init];
  return _placePageManager;
}

- (MWMNavigationDashboardManager *)navigationManager {
  if (!_navigationManager)
    _navigationManager = [[MWMNavigationDashboardManager alloc] initWithParentView:self.ownerController.controlsView];
  return _navigationManager;
}

- (MWMSearchManager *)searchManager {
  if (!_searchManager) {
    _searchManager = [[MWMSearchManager alloc] init];
    [MWMSearchManager addObserver:self];
  }
  return _searchManager;
}

@synthesize menuState = _menuState;

- (void)setMenuState:(MWMBottomMenuState)menuState {
  _menuState = menuState;
}

#pragma mark - MWMFeatureHolder

- (id<MWMFeatureHolder>)featureHolder {
  return self.placePageManager;
}

@end
