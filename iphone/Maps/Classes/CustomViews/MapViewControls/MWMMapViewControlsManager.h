#import "MWMBottomMenuViewController.h"
#import "MWMNavigationDashboardManager.h"
#import "MWMRoutingProtocol.h"

#include "map/user_mark.hpp"
#include "platform/location.hpp"

@class MapViewController;
@class MWMPlacePageEntity;

@interface MWMMapViewControlsManager : NSObject <MWMRoutingProtocol>

@property (nonatomic) BOOL hidden;
@property (nonatomic) BOOL zoomHidden;
@property (nonatomic) MWMBottomMenuState menuState;
@property (nonatomic, readonly) MWMNavigationDashboardState navigationState;
@property (nonatomic, readonly) MWMPlacePageEntity * placePageEntity;
@property (nonatomic) BOOL searchHidden;

- (instancetype)init __attribute__((unavailable("init is not available")));
- (instancetype)initWithParentController:(MapViewController *)controller;

#pragma mark - Layout

- (void)refreshLayout;
- (void)refresh;

- (void)willRotateToInterfaceOrientation:(UIInterfaceOrientation)toInterfaceOrientation
                                duration:(NSTimeInterval)duration;
- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:(id<UIViewControllerTransitionCoordinator>)coordinator;

#pragma mark - MWMPlacePageViewManager

@property (nonatomic, readonly) BOOL isDirectionViewShown;

- (void)dismissPlacePage;
- (void)showPlacePage;
- (void)reloadPlacePage;

#pragma mark - MWMNavigationDashboardManager

- (void)setupRoutingDashboard:(location::FollowingInfo const &)info;
- (void)restoreRouteTo:(m2::PointD const &)to;
- (void)routingHidden;
- (void)routingReady;
- (void)routingPrepare;
- (void)routingNavigation;
- (void)handleRoutingError;

@end
