#import "LocationManager.h"
#import "LocationPredictor.h"
#import "MWMViewController.h"
#import <MyTargetSDKCorp/MTRGNativeAppwallAd.h>

#include "geometry/point2d.hpp"
#include "geometry/rect2d.hpp"
#include "indexer/map_style.hpp"

namespace search { struct AddressInfo; }

@class MWMMapViewControlsManager;
@class MWMAPIBar;

@interface MapViewController : MWMViewController <LocationObserver, UIPopoverControllerDelegate>
{
  CGPoint m_popoverPos;
  
  LocationPredictor * m_predictor;
}

// called when app is terminated by system
- (void)onTerminate;
- (void)onEnterForeground;
- (void)onEnterBackground;

- (void)dismissPopover;

- (void)setMapStyle:(MapStyle)mapStyle;

- (void)updateStatusBarStyle;

- (void)showAPIBar;

- (void)performAction:(NSString *)action;

- (void)openBookmarks;
- (void)openMapsDownloader;

- (void)refreshAd;

- (void)initialize;

@property (nonatomic) MTRGNativeAppwallAd * appWallAd;
@property (nonatomic, readonly) BOOL isAppWallAdActive;

@property (nonatomic) UIPopoverController * popoverVC;
@property (nonatomic, readonly) MWMMapViewControlsManager * controlsManager;
@property (nonatomic) m2::PointD restoreRouteDestination;
@property (nonatomic) MWMAPIBar * apiBar;
@property (nonatomic) BOOL skipPlacePageDismissOnViewDisappear;
@property (nonatomic, readonly) MWMAlertViewController * alertController;

@end
