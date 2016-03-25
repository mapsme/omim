#import "LocationManager.h"
#import "MWMCircularProgress.h"
#import "MWMNavigationViewProtocol.h"
#import "MWMRoutePreview.h"

#include "Framework.h"
#include "platform/location.hpp"

typedef NS_ENUM(NSUInteger, MWMNavigationDashboardState)
{
  MWMNavigationDashboardStateHidden,
  MWMNavigationDashboardStatePrepare,
  MWMNavigationDashboardStatePlanning,
  MWMNavigationDashboardStateError,
  MWMNavigationDashboardStateReady,
  MWMNavigationDashboardStateNavigation
};

@protocol MWMNavigationDashboardManagerProtocol <MWMNavigationViewProtocol>

- (void)buildRoute;
- (BOOL)isPossibleToBuildRoute;
- (BOOL)didStartFollowing;
- (void)didCancelRouting;
- (void)updateStatusBarStyle;
- (void)didStartEditingRoutePoint:(BOOL)isSource;
- (void)swapPointsAndRebuildRouteIfPossible;

@end

@class MWMNavigationDashboardEntity;

@interface MWMNavigationDashboardManager : NSObject <LocationObserver, MWMCircularProgressProtocol>

@property (nonatomic, readonly) MWMNavigationDashboardEntity * entity;
@property (weak, nonatomic, readonly) MWMRoutePreview * _Nullable routePreview;
@property (nonatomic) MWMNavigationDashboardState state;
@property (weak, nonatomic, readonly) id<MWMNavigationDashboardManagerProtocol> _Nullable delegate;
@property (nonatomic) CGFloat topBound;
@property (nonatomic) CGFloat leftBound;
@property (nonatomic, readonly) CGFloat height;

- (instancetype _Nullable)init __attribute__((unavailable("init is not available")));
- (instancetype _Nullable)initWithParentView:(UIView *)view delegate:(id<MWMNavigationDashboardManagerProtocol, MWMRoutePreviewDataSource>)delegate;
- (void)setupDashboard:(location::FollowingInfo const &)info;
- (void)updateDashboard;
- (void)willRotateToInterfaceOrientation:(UIInterfaceOrientation)orientation;
- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:(id<UIViewControllerTransitionCoordinator>)coordinator;
- (void)setRouteBuilderProgress:(CGFloat)progress;
- (void)showHelperPanels;
- (void)hideHelperPanels;
- (void)setupActualRoute;
- (void)mwm_refreshUI;

@end
