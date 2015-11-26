#import "LocationManager.h"
#import "LocationPredictor.h"
#import "ViewController.h"
#import <MyTargetSDKCorp/MTRGNativeAppwallAd.h>

#include "geometry/point2d.hpp"
#include "geometry/rect2d.hpp"
#include "indexer/map_style.hpp"

namespace search { struct AddressInfo; }

@class MWMMapViewControlsManager;
@class ShareActionSheet;
@class MWMAPIBar;

@interface MapViewController : ViewController <LocationObserver, UIPopoverControllerDelegate>
{
	bool m_isSticking;
	size_t m_StickyThreshold;
	m2::PointD m_Pt1, m_Pt2;

  /// Temporary solution to improve long touch detection.
  m2::PointD m_touchDownPoint;

  CGPoint m_popoverPos;
  
  LocationPredictor * m_predictor;
}

- (void)setupMeasurementSystem;

// called when app is terminated by system
- (void)onTerminate;
- (void)onEnterForeground;
- (void)onEnterBackground;

- (void)dismissPopover;

- (void)setMapStyle:(MapStyle)mapStyle;

- (void)updateStatusBarStyle;

- (void)showAPIBar;

- (void)presentUpdateMapsAlert;

- (void)performAction:(NSString *)action;

- (void)openBookmarks;

- (void)refreshAd;
@property (nonatomic) MTRGNativeAppwallAd * appWallAd;
@property (nonatomic, readonly) BOOL isAppWallAdActive;

@property (nonatomic) UIPopoverController * popoverVC;
@property (nonatomic) ShareActionSheet * shareActionSheet;
@property (nonatomic, readonly) MWMMapViewControlsManager * controlsManager;
@property (nonatomic) m2::PointD restoreRouteDestination;
@property (nonatomic) MWMAPIBar * apiBar;

@end
