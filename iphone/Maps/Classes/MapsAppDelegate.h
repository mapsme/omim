#import "DownloadIndicatorProtocol.h"
#import "MWMFrameworkListener.h"
#import "NavigationController.h"

#include "indexer/map_style.hpp"

@class MapViewController;
@class LocationManager;

typedef NS_ENUM(NSUInteger, MWMRoutingPlaneMode)
{
  MWMRoutingPlaneModeNone,
  MWMRoutingPlaneModePlacePage,
  MWMRoutingPlaneModeSearchSource,
  MWMRoutingPlaneModeSearchDestination
};

@interface MapsAppDelegate : UIResponder<UIApplicationDelegate, UIAlertViewDelegate,
                                         DownloadIndicatorProtocol>
{
  NSInteger m_activeDownloadsCounter;
  UIBackgroundTaskIdentifier m_backgroundTask;
  UIAlertView * m_loadingAlertView;
}

@property (nonatomic) UIWindow * window;
@property (nonatomic) MWMRoutingPlaneMode routingPlaneMode;

@property (nonatomic, readonly) MapViewController * mapViewController;
@property (nonatomic, readonly) LocationManager * m_locationManager;

@property (nonatomic, readonly) MWMFrameworkListener * frameworkListener;

+ (MapsAppDelegate *)theApp;

- (void)enableStandby;
- (void)disableStandby;
+ (void)customizeAppearance;

- (void)disableDownloadIndicator;
- (void)enableDownloadIndicator;

- (void)showMap;
- (void)changeMapStyleIfNedeed;
- (void)startMapStyleChecker;
- (void)stopMapStyleChecker;

- (void)setMapStyle:(MapStyle)mapStyle;

@end
