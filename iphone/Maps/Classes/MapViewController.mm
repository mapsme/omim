#import "BookmarksRootVC.h"
#import "BookmarksVC.h"
#import "Common.h"
#import "EAGLView.h"
#import "MapsAppDelegate.h"
#import "MapViewController.h"
#import "MWMAlertViewController.h"
#import "MWMAPIBar.h"
#import "MWMMapViewControlsManager.h"
#import "MWMPageController.h"
#import "MWMTextToSpeech.h"
#import "RouteState.h"
#import "Statistics.h"
#import "UIFont+MapsMeFonts.h"
#import "UIViewController+Navigation.h"
#import <MyTargetSDKCorp/MTRGManager_Corp.h>

#import "3party/Alohalytics/src/alohalytics_objc.h"

#include "Framework.h"
#include "RenderContext.hpp"

#include "anim/controller.hpp"
#include "gui/controller.hpp"

#include "map/country_status_display.hpp"
#include "map/user_mark.hpp"

#include "platform/file_logging.hpp"
#include "platform/platform.hpp"
#include "platform/settings.hpp"

// If you have a "missing header error" here, then please run configure.sh script in the root repo folder.
#import "../../../private.h"

extern NSString * const kAlohalyticsTapEventKey = @"$onClick";
extern NSString * const kUDWhatsNewWasShown = @"WhatsNewWithTTSAndP2PWasShown";
extern char const * kAdForbiddenSettingsKey;
extern char const * kAdServerForbiddenKey;

typedef NS_ENUM(NSUInteger, ForceRoutingStateChange)
{
  ForceRoutingStateChangeNone,
  ForceRoutingStateChangeRestoreRoute,
  ForceRoutingStateChangeStartFollowing
};

typedef NS_ENUM(NSUInteger, UserTouchesAction)
{
  UserTouchesActionNone,
  UserTouchesActionDrag,
  UserTouchesActionScale
};

@interface NSValueWrapper : NSObject

-(NSValue *)getInnerValue;

@end

@implementation NSValueWrapper
{
  NSValue * m_innerValue;
}

-(NSValue *)getInnerValue
{
  return m_innerValue;
}

-(id)initWithValue:(NSValue *)value
{
  self = [super init];
  if (self)
    m_innerValue = value;
  return self;
}

-(BOOL)isEqual:(id)anObject
{
  return [anObject isMemberOfClass:[NSValueWrapper class]];
}

@end

@interface MapViewController () <MTRGNativeAppwallAdDelegate>

@property (nonatomic, readwrite) MWMMapViewControlsManager * controlsManager;
@property (nonatomic) MWMBottomMenuState menuRestoreState;

@property (nonatomic) ForceRoutingStateChange forceRoutingStateChange;
@property (nonatomic) BOOL disableStandbyOnLocationStateMode;

@property (nonatomic) MWMAlertViewController * alertController;

@property (nonatomic) UserTouchesAction userTouchesAction;
@property (nonatomic) MWMPageController * pageViewController;

@property (nonatomic) BOOL skipForceTouch;

@end

@implementation MapViewController

#pragma mark - LocationManager Callbacks

- (void)onLocationError:(location::TLocationError)errorCode
{
  GetFramework().OnLocationError(errorCode);

  switch (errorCode)
  {
    case location::EDenied:
    {
      [self.alertController presentLocationAlert];
      [[MapsAppDelegate theApp].m_locationManager stop:self];
      break;
    }
    case location::ENotSupported:
    {
      [self.alertController presentLocationServiceNotSupportedAlert];
      [[MapsAppDelegate theApp].m_locationManager stop:self];
      break;
    }
    default:
      break;
  }
}

- (void)onLocationUpdate:(location::GpsInfo const &)info
{
  // TODO: Remove this hack for location changing bug
  if (self.navigationController.visibleViewController == self)
  {
    if (info.m_source != location::EPredictor)
      [m_predictor reset:info];
    Framework & frm = GetFramework();
    frm.OnLocationUpdate(info);
    LOG_MEMORY_INFO();

    [self showPopover];
    [self updateRoutingInfo];

    if (self.forceRoutingStateChange == ForceRoutingStateChangeRestoreRoute)
      [self restoreRoute];
  }
}

- (void)updateRoutingInfo
{
  Framework & frm = GetFramework();
  if (!frm.IsRoutingActive())
    return;

  location::FollowingInfo res;
  frm.GetRouteFollowingInfo(res);

  if (res.IsValid())
    [self.controlsManager setupRoutingDashboard:res];

  if (frm.IsOnRoute())
    [[MWMTextToSpeech tts] playTurnNotifications];
}

- (void)onCompassUpdate:(location::CompassInfo const &)info
{
  // TODO: Remove this hack for orientation changing bug
  if (self.navigationController.visibleViewController == self)
    GetFramework().OnCompassUpdate(info);
}

- (void)onLocationStateModeChanged:(location::State::Mode)newMode
{
  switch (newMode)
  {
    case location::State::UnknownPosition:
    {
      self.disableStandbyOnLocationStateMode = NO;
      [[MapsAppDelegate theApp].m_locationManager stop:self];
      break;
    }
    case location::State::PendingPosition:
      self.disableStandbyOnLocationStateMode = NO;
      [[MapsAppDelegate theApp].m_locationManager start:self];
      break;
    case location::State::NotFollow:
      self.disableStandbyOnLocationStateMode = NO;
      break;
    case location::State::Follow:
    case location::State::RotateAndFollow:
      self.disableStandbyOnLocationStateMode = YES;
      break;
  }
}

#pragma mark - Restore route

- (void)restoreRoute
{
  self.forceRoutingStateChange = ForceRoutingStateChangeStartFollowing;
  auto & f = GetFramework();
  m2::PointD const location = [MapsAppDelegate theApp].m_locationManager.lastLocation.mercator;
  f.SetRouter(f.GetBestRouter(location, self.restoreRouteDestination));
  GetFramework().BuildRoute(location, self.restoreRouteDestination, 0 /* timeoutSec */);
}

#pragma mark - Map Navigation

- (void)dismissPlacePage
{
  [self.controlsManager dismissPlacePage];
}

- (void)onUserMarkClicked:(unique_ptr<UserMarkCopy>)mark
{
  [self.controlsManager showPlacePageWithUserMark:std::move(mark)];
}

- (void)processMapClickAtPoint:(CGPoint)point longClick:(BOOL)isLongClick
{
  CGFloat const scaleFactor = self.view.contentScaleFactor;
  m2::PointD const pxClicked(point.x * scaleFactor, point.y * scaleFactor);

  Framework & f = GetFramework();
  UserMark const * userMark = f.GetUserMark(pxClicked, isLongClick);
  if (!f.HasActiveUserMark() && self.controlsManager.searchHidden && !f.IsRouteNavigable()
      && MapsAppDelegate.theApp.routingPlaneMode == MWMRoutingPlaneModeNone)
  {
    if (userMark == nullptr)
      self.controlsManager.hidden = !self.controlsManager.hidden;
    else
      self.controlsManager.hidden = NO;
  }
  f.GetBalloonManager().OnShowMark(userMark);
}

- (void)onSingleTap:(NSValueWrapper *)point
{
  [self processMapClickAtPoint:[[point getInnerValue] CGPointValue] longClick:NO];
}

- (void)onLongTap:(NSValueWrapper *)point
{
  [self processMapClickAtPoint:[[point getInnerValue] CGPointValue] longClick:YES];
}

- (void)popoverControllerDidDismissPopover:(UIPopoverController *)popoverController
{
  [self destroyPopover];
  [self invalidate];
}

- (void)updatePointsFromEvent:(UIEvent *)event
{
  NSSet * allTouches = [event allTouches];

  UIView * v = self.view;
  CGFloat const scaleFactor = v.contentScaleFactor;

  // 0 touches are possible from touchesCancelled.
  switch ([allTouches count])
  {
    case 0:
      break;
    case 1:
    {
      CGPoint const pt = [[[allTouches allObjects] objectAtIndex:0] locationInView:v];
      m_Pt1 = m2::PointD(pt.x * scaleFactor, pt.y * scaleFactor);
      break;
    }
    default:
    {
      NSArray * sortedTouches = [[allTouches allObjects] sortedArrayUsingFunction:compareAddress context:NULL];
      CGPoint const pt1 = [[sortedTouches objectAtIndex:0] locationInView:v];
      CGPoint const pt2 = [[sortedTouches objectAtIndex:1] locationInView:v];

      m_Pt1 = m2::PointD(pt1.x * scaleFactor, pt1.y * scaleFactor);
      m_Pt2 = m2::PointD(pt2.x * scaleFactor, pt2.y * scaleFactor);
      break;
    }
  }
}

- (BOOL)hasForceTouch
{
  if (isIOSVersionLessThan(9))
    return NO;
  return self.view.traitCollection.forceTouchCapability == UIForceTouchCapabilityAvailable;
}

-(void)preformLongTapSelector:(NSValue *)object
{
  if (![self hasForceTouch])
    [self performSelector:@selector(onLongTap:) withObject:[[NSValueWrapper alloc] initWithValue:object] afterDelay:1.0];
}

-(void)performSingleTapSelector:(NSValue *)object
{
  [self performSelector:@selector(onSingleTap:) withObject:[[NSValueWrapper alloc] initWithValue:object] afterDelay:0.3];
}

-(void)cancelLongTap
{
  if (![self hasForceTouch])
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(onLongTap:) object:[[NSValueWrapper alloc] initWithValue:nil]];
}

-(void)cancelSingleTap
{
  [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(onSingleTap:) object:[[NSValueWrapper alloc] initWithValue:nil]];
}

- (void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)event
{
  // To cancel single tap timer
  UITouch * theTouch = (UITouch *)[touches anyObject];
  if (theTouch.tapCount > 1)
    [self cancelSingleTap];

  [self updatePointsFromEvent:event];

  Framework & f = GetFramework();

  if ([event allTouches].count == 1)
  {
    if (f.GetGuiController()->OnTapStarted(m_Pt1))
      return;
    self.userTouchesAction = UserTouchesActionDrag;

    // Start long-tap timer
    [self preformLongTapSelector:[NSValue valueWithCGPoint:[theTouch locationInView:self.view]]];
    // Temporary solution to filter long touch
    m_touchDownPoint = m_Pt1;
  }
  else
  {
    self.userTouchesAction = UserTouchesActionScale;
  }

  m_isSticking = true;
}

- (void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *)event
{
  if (!self.skipForceTouch && [self hasForceTouch])
  {
    UITouch * theTouch = (UITouch *)[touches anyObject];
    if (theTouch.force >= theTouch.maximumPossibleForce)
    {
      self.skipForceTouch = YES;
      [self processMapClickAtPoint:[theTouch locationInView:self.view] longClick:YES];
    }
  }

  m2::PointD const TempPt1 = m_Pt1;
  m2::PointD const TempPt2 = m_Pt2;

  [self updatePointsFromEvent:event];

  // Cancel long-touch timer
  if (!m_touchDownPoint.EqualDxDy(m_Pt1, 9))
    [self cancelLongTap];

  Framework & f = GetFramework();

  if (f.GetGuiController()->OnTapMoved(m_Pt1))
    return;

  if (m_isSticking)
  {
    if ((TempPt1.Length(m_Pt1) > m_StickyThreshold) || (TempPt2.Length(m_Pt2) > m_StickyThreshold))
    {
      m_isSticking = false;
    }
    else
    {
      // Still stickying. Restoring old points and return.
      m_Pt1 = TempPt1;
      m_Pt2 = TempPt2;
      return;
    }
  }

  NSUInteger const touchesCount = [event allTouches].count;
  switch (self.userTouchesAction)
  {
    case UserTouchesActionNone:
      if (touchesCount == 1)
        self.userTouchesAction = UserTouchesActionDrag;
      else
        self.userTouchesAction = UserTouchesActionScale;
      break;
    case UserTouchesActionDrag:
      if (touchesCount == 1)
        f.DoDrag(DragEvent(m_Pt1.x, m_Pt1.y));
      else
        self.userTouchesAction = UserTouchesActionNone;
      break;
    case UserTouchesActionScale:
      if (touchesCount == 2)
        f.DoScale(ScaleEvent(m_Pt1.x, m_Pt1.y, m_Pt2.x, m_Pt2.y));
      else
        self.userTouchesAction = UserTouchesActionNone;
      break;
  }
}

- (void)touchesEnded:(NSSet *)touches withEvent:(UIEvent *)event
{
  [self updatePointsFromEvent:event];
  self.userTouchesAction = UserTouchesActionNone;

  UITouch * theTouch = (UITouch *)[touches anyObject];
  NSUInteger const tapCount = theTouch.tapCount;
  NSUInteger const touchesCount = [event allTouches].count;

  Framework & f = GetFramework();

  if (touchesCount == 1)
  {
    // Cancel long-touch timer
    [self cancelLongTap];

    // TapCount could be zero if it was a single long (or moving) tap.
    if (tapCount < 2)
    {
      if (f.GetGuiController()->OnTapEnded(m_Pt1))
        return;
    }

    if (tapCount == 1)
    {
      // Launch single tap timer
      if (m_isSticking && !self.skipForceTouch)
        [self performSingleTapSelector: [NSValue valueWithCGPoint:[theTouch locationInView:self.view]]];
    }
    else if (tapCount == 2 && m_isSticking)
    {
      f.ScaleToPoint(ScaleToPointEvent(m_Pt1.x, m_Pt1.y, 2.0));
    }
  }

  if (touchesCount == 2 && tapCount == 1 && m_isSticking)
  {
    f.Scale(0.5);
    if (!m_touchDownPoint.EqualDxDy(m_Pt1, 9))
    {
      [self cancelLongTap];
      [self cancelSingleTap];
    }
    m_isSticking = NO;
  }
  self.skipForceTouch = NO;
}

- (void)touchesCancelled:(NSSet *)touches withEvent:(UIEvent *)event
{
  self.skipForceTouch = NO;
  [self cancelLongTap];
  [self cancelSingleTap];

  [self updatePointsFromEvent:event];
  self.userTouchesAction = UserTouchesActionNone;
}

#pragma mark - ViewController lifecycle

- (void)dealloc
{
  [self destroyPopover];
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (BOOL)shouldAutorotateToInterfaceOrientation: (UIInterfaceOrientation)interfaceOrientation
{
  return YES; // We support all orientations
}

- (void)willRotateToInterfaceOrientation:(UIInterfaceOrientation)toInterfaceOrientation
                                duration:(NSTimeInterval)duration
{
  if (isIOSVersionLessThan(8))
    [self.alertController willRotateToInterfaceOrientation:toInterfaceOrientation duration:duration];
  [self.controlsManager willRotateToInterfaceOrientation:toInterfaceOrientation duration:duration];
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:(id<UIViewControllerTransitionCoordinator>)coordinator
{
  [self.controlsManager viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
  [self.pageViewController viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
}

- (void)didRotateFromInterfaceOrientation:(UIInterfaceOrientation)fromInterfaceOrientation
{
  [self showPopover];
  [self invalidate];
}

- (void)didReceiveMemoryWarning
{
  GetFramework().MemoryWarning();
  [super didReceiveMemoryWarning];
}

- (void)onTerminate
{
  GetFramework().SaveState();
}

- (void)onEnterBackground
{
  // Save state and notify about entering background.

  Framework & f = GetFramework();
  f.SaveState();
  f.SetUpdatesEnabled(false);
  f.EnterBackground();
}

- (void)setMapStyle:(MapStyle)mapStyle
{
  EAGLView * v = (EAGLView *)self.view;
  [v setMapStyle: mapStyle];
}

- (void)onEnterForeground
{
  // Notify about entering foreground (should be called on the first launch too).
  GetFramework().EnterForeground();

  if (self.isViewLoaded && self.view.window)
  {
    [self invalidate]; // only invalidate when map is displayed on the screen
    [self.controlsManager onEnterForeground];
  }
}

- (void)viewWillAppear:(BOOL)animated
{
  [super viewWillAppear:animated];
  [[NSNotificationCenter defaultCenter] removeObserver:self name:UIDeviceOrientationDidChangeNotification object:nil];
  [self invalidate];

  self.controlsManager.menuState = self.menuRestoreState;

  [self refreshAd];
}

- (void)viewDidLoad
{
  [super viewDidLoad];
  EAGLView * v = (EAGLView *)self.view;
  [v initRenderPolicy];
  self.view.clipsToBounds = YES;
  [MTRGManager setMyCom:YES];
  self.controlsManager = [[MWMMapViewControlsManager alloc] initWithParentController:self];
  if (!isIOSVersionLessThan(8))
    [self showWhatsNewIfNeeded];
}

- (void)showWhatsNewIfNeeded
{
  NSUserDefaults * ud = [NSUserDefaults standardUserDefaults];
  BOOL const whatsNewWasShown = [ud boolForKey:kUDWhatsNewWasShown];
  if (whatsNewWasShown)
    return;

  if (![Alohalytics isFirstSession])
    [self configureAndShowPageController];

  [ud setBool:YES forKey:kUDWhatsNewWasShown];
  [ud synchronize];
}

- (void)configureAndShowPageController
{
  self.pageViewController = [MWMPageController pageControllerWithParent:self];
  [self.pageViewController show];
}

- (void)viewWillDisappear:(BOOL)animated
{
  [super viewWillDisappear:animated];
  self.menuRestoreState = self.controlsManager.menuState;
  GetFramework().SetUpdatesEnabled(false);
  [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(orientationChanged:) name:UIDeviceOrientationDidChangeNotification object:nil];
}

- (void)presentViewController:(UIViewController *)viewControllerToPresent
                     animated:(BOOL)flag
                   completion:(void (^__nullable)(void))completion
{
  if (isIOSVersionLessThan(8))
    self.menuRestoreState = self.controlsManager.menuState;
  [super presentViewController:viewControllerToPresent animated:flag completion:completion];
}

- (void)orientationChanged:(NSNotification *)notification
{
  [self willRotateToInterfaceOrientation:self.interfaceOrientation duration:0.];
}

- (BOOL)prefersStatusBarHidden
{
  return self.apiBar.isVisible;
}

- (UIStatusBarStyle)preferredStatusBarStyle
{
  BOOL const isLight = !self.controlsManager.searchHidden ||
                       self.controlsManager.menuState == MWMBottomMenuStateActive ||
                       self.controlsManager.isDirectionViewShown ||
                       (GetFramework().GetMapStyle() == MapStyleDark &&
                        self.controlsManager.navigationState == MWMNavigationDashboardStateHidden) ||
                        MapsAppDelegate.theApp.routingPlaneMode != MWMRoutingPlaneModeNone;
  if (isLight)
    return UIStatusBarStyleLightContent;
  return UIStatusBarStyleDefault;
}

- (void)updateStatusBarStyle
{
  [self setNeedsStatusBarAppearanceUpdate];
}

- (id)initWithCoder:(NSCoder *)coder
{
  NSLog(@"MapViewController initWithCoder Started");

  if ((self = [super initWithCoder:coder]))
  {
    Framework & f = GetFramework();

    typedef void (*UserMarkActivatedFnT)(id, SEL, unique_ptr<UserMarkCopy>);
    typedef void (*PlacePageDismissedFnT)(id, SEL);

    PinClickManager & manager = f.GetBalloonManager();

    SEL userMarkSelector = @selector(onUserMarkClicked:);
    UserMarkActivatedFnT userMarkFn = (UserMarkActivatedFnT)[self methodForSelector:userMarkSelector];
    manager.ConnectUserMarkListener(bind(userMarkFn, self, userMarkSelector, _1));

    SEL dismissSelector = @selector(dismissPlacePage);
    PlacePageDismissedFnT dismissFn = (PlacePageDismissedFnT)[self methodForSelector:dismissSelector];
    manager.ConnectDismissListener(bind(dismissFn, self, dismissSelector));

    typedef void (*LocationStateModeFnT)(id, SEL, location::State::Mode);
    SEL locationStateModeSelector = @selector(onLocationStateModeChanged:);
    LocationStateModeFnT locationStateModeFn = (LocationStateModeFnT)[self methodForSelector:locationStateModeSelector];

    f.GetLocationState()->AddStateModeListener(bind(locationStateModeFn, self, locationStateModeSelector, _1));

    m_predictor = [[LocationPredictor alloc] initWithObserver:self];

    m_StickyThreshold = 10;

    self.forceRoutingStateChange = ForceRoutingStateChangeNone;
    self.userTouchesAction = UserTouchesActionNone;
    self.menuRestoreState = MWMBottomMenuStateInactive;

    // restore previous screen position
    if (!f.LoadState())
      f.SetMaxWorldRect();

    f.Invalidate();
    f.LoadBookmarks();

    f.GetCountryStatusDisplay()->SetDownloadCountryListener([self, &f](storage::TIndex const & idx, int opt)
    {
      ActiveMapsLayout & layout = f.GetCountryTree().GetActiveMapLayout();
      if (opt == -1)
      {
        layout.RetryDownloading(idx);
      }
      else
      {
        LocalAndRemoteSizeT sizes = layout.GetRemoteCountrySizes(idx);
        uint64_t sizeToDownload = sizes.first;
        MapOptions options = static_cast<MapOptions>(opt);
        if(HasOptions(options, MapOptions::CarRouting))
          sizeToDownload += sizes.second;

        NSString * name = @(layout.GetCountryName(idx).c_str());
        Platform::EConnectionType const connection = Platform::ConnectionStatus();
        if (connection != Platform::EConnectionType::CONNECTION_NONE)
        {
          if (connection == Platform::EConnectionType::CONNECTION_WWAN && sizeToDownload > 50 * MB)
          {
            [self.alertController presentnoWiFiAlertWithName:name downloadBlock:^
            {
              layout.DownloadMap(idx, static_cast<MapOptions>(opt));
            }];
            return;
          }
        }
        else
        {
          [self.alertController presentNoConnectionAlert];
          return;
        }

        layout.DownloadMap(idx, static_cast<MapOptions>(opt));
      }
    });

    f.SetRouteBuildingListener([self, &f](routing::IRouter::ResultCode code, vector<storage::TIndex> const & absentCountries, vector<storage::TIndex> const & absentRoutes)
    {
      switch (code)
      {
        case routing::IRouter::ResultCode::NoError:
        {
          f.GetBalloonManager().RemovePin();
          f.GetBalloonManager().Dismiss();
          self.controlsManager.routeBuildingProgress = 100.;
          self.controlsManager.searchHidden = YES;
          if (self.forceRoutingStateChange == ForceRoutingStateChangeStartFollowing)
            [self.controlsManager routingNavigation];
          else
            [self.controlsManager routingReady];
          [self updateRoutingInfo];
          self.forceRoutingStateChange = ForceRoutingStateChangeNone;
          bool isDisclaimerApproved = false;
          (void)Settings::Get("IsDisclaimerApproved", isDisclaimerApproved);
          if (!isDisclaimerApproved)
          {
            [self presentRoutingDisclaimerAlert];
            Settings::Set("IsDisclaimerApproved", true);
          }
          break;
        }
        case routing::IRouter::RouteFileNotExist:
        case routing::IRouter::InconsistentMWMandRoute:
        case routing::IRouter::NeedMoreMaps:
        case routing::IRouter::FileTooOld:
        case routing::IRouter::RouteNotFound:
          [self.controlsManager handleRoutingError];
          [self presentDownloaderAlert:code countries:absentCountries routes:absentRoutes];
          self.forceRoutingStateChange = ForceRoutingStateChangeNone;
          break;
        case routing::IRouter::Cancelled:
          self.forceRoutingStateChange = ForceRoutingStateChangeNone;
          break;
        default:
          [self.controlsManager handleRoutingError];
          [self presentDefaultAlert:code];
          self.forceRoutingStateChange = ForceRoutingStateChangeNone;
          break;
      }
    });
    f.SetRouteProgressListener([self](float progress)
    {
      self.controlsManager.routeBuildingProgress = progress;
    });
  }

  NSLog(@"MapViewController initWithCoder Ended");
  return self;
}

- (void)openBookmarks
{
  BOOL const oneCategory = (GetFramework().GetBmCategoriesCount() == 1);
  TableViewController * vc =
      oneCategory ? [[BookmarksVC alloc] initWithCategory:0] : [[BookmarksRootVC alloc] init];
  [self.navigationController pushViewController:vc animated:YES];
}

#pragma mark - 3d touch

- (void)performAction:(NSString *)action
{
  [self.navigationController popToRootViewControllerAnimated:NO];
  self.controlsManager.searchHidden = YES;
  [self.controlsManager routingHidden];
  if ([action isEqualToString:@"me.maps.3daction.bookmarks"])
  {
    [self openBookmarks];
  }
  else if ([action isEqualToString:@"me.maps.3daction.search"])
  {
    self.controlsManager.searchHidden = NO;
  }
  else if ([action isEqualToString:@"me.maps.3daction.route"])
  {
    [MapsAppDelegate theApp].routingPlaneMode = MWMRoutingPlaneModePlacePage;
    [self.controlsManager routingPrepare];
  }
}

#pragma mark - myTarget

- (void)refreshAd
{
  bool adServerForbidden = false;
  (void)Settings::Get(kAdServerForbiddenKey, adServerForbidden);
  bool adForbidden = false;
  (void)Settings::Get(kAdForbiddenSettingsKey, adForbidden);
  if (isIOSVersionLessThan(8) || adServerForbidden || adForbidden)
  {
    self.appWallAd = nil;
    return;
  }
  if (self.isAppWallAdActive)
    return;
  self.appWallAd = [[MTRGNativeAppwallAd alloc]initWithSlotId:@(MY_TARGET_KEY)];
  self.appWallAd.handleLinksInApp = YES;
  self.appWallAd.closeButtonTitle = L(@"close");
  self.appWallAd.delegate = self;
  [self.appWallAd load];
}

- (void)onLoadWithAppwallBanners:(NSArray *)appwallBanners appwallAd:(MTRGNativeAppwallAd *)appwallAd
{
  if (![appwallAd isEqual:self.appWallAd])
    return;
  if (appwallBanners.count == 0)
    self.appWallAd = nil;
  [self.controlsManager refreshLayout];
}

- (void)onNoAdWithReason:(NSString *)reason appwallAd:(MTRGNativeAppwallAd *)appwallAd
{
  if (![appwallAd isEqual:self.appWallAd])
    return;
  self.appWallAd = nil;
}

#pragma mark - API bar

- (MWMAPIBar *)apiBar
{
  if (!_apiBar)
    _apiBar = [[MWMAPIBar alloc] initWithController:self];
  return _apiBar;
}

- (void)showAPIBar
{
  self.apiBar.isVisible = YES;
}

#pragma mark - ShowDialog callback

- (void)presentDownloaderAlert:(routing::IRouter::ResultCode)code
                     countries:(vector<storage::TIndex> const &)countries
                        routes:(vector<storage::TIndex> const &)routes
{
  if (countries.size() || routes.size())
    [self.alertController presentDownloaderAlertWithCountries:countries routes:routes code:code];
  else
    [self presentDefaultAlert:code];
}

- (void)presentDisabledLocationAlert
{
  [self.alertController presentDisabledLocationAlert];
}

- (void)presentDefaultAlert:(routing::IRouter::ResultCode)type
{
  [self.alertController presentAlert:type];
}

- (void)presentRoutingDisclaimerAlert
{
  [self.alertController presentRoutingDisclaimerAlert];
}

#pragma mark - Getters

- (MWMAlertViewController *)alertController
{
  if (!_alertController)
    _alertController = [[MWMAlertViewController alloc] initWithViewController:self];
  return _alertController;
}

#pragma mark - Public methods

- (void)setupMeasurementSystem
{
  GetFramework().SetupMeasurementSystem();
}

#pragma mark - Private methods

NSInteger compareAddress(id l, id r, void * context)
{
  return l < r;
}

- (void)invalidate
{
  Framework & f = GetFramework();
  if (!f.SetUpdatesEnabled(true))
    f.Invalidate();
}

- (void)destroyPopover
{
  self.popoverVC = nil;
}

- (void)showPopover
{
  if (self.popoverVC)
    GetFramework().GetBalloonManager().Hide();

  double const sf = self.view.contentScaleFactor;

  Framework & f = GetFramework();
  m2::PointD tmp = m2::PointD(f.GtoP(m2::PointD(m_popoverPos.x, m_popoverPos.y)));

  [self.popoverVC presentPopoverFromRect:CGRectMake(tmp.x / sf, tmp.y / sf, 1, 1) inView:self.view permittedArrowDirections:UIPopoverArrowDirectionAny animated:YES];
}

- (void)dismissPopover
{
  [self.popoverVC dismissPopoverAnimated:YES];
  [self destroyPopover];
  [self invalidate];
}

- (void)setRestoreRouteDestination:(m2::PointD)restoreRouteDestination
{
  _restoreRouteDestination = restoreRouteDestination;
  self.forceRoutingStateChange = ForceRoutingStateChangeRestoreRoute;
}

- (void)setDisableStandbyOnLocationStateMode:(BOOL)disableStandbyOnLocationStateMode
{
  if (_disableStandbyOnLocationStateMode == disableStandbyOnLocationStateMode)
    return;
  _disableStandbyOnLocationStateMode = disableStandbyOnLocationStateMode;
  if (disableStandbyOnLocationStateMode)
    [[MapsAppDelegate theApp] disableStandby];
  else
    [[MapsAppDelegate theApp] enableStandby];
}

- (void)setUserTouchesAction:(UserTouchesAction)userTouchesAction
{
  if (_userTouchesAction == userTouchesAction)
    return;
  Framework & f = GetFramework();
  switch (userTouchesAction)
  {
    case UserTouchesActionNone:
      if (_userTouchesAction == UserTouchesActionDrag)
        f.StopDrag(DragEvent(m_Pt1.x, m_Pt1.y));
      else if (_userTouchesAction == UserTouchesActionScale)
        f.StopScale(ScaleEvent(m_Pt1.x, m_Pt1.y, m_Pt2.x, m_Pt2.y));
      break;
    case UserTouchesActionDrag:
      f.StartDrag(DragEvent(m_Pt1.x, m_Pt1.y));
      break;
    case UserTouchesActionScale:
      f.StartScale(ScaleEvent(m_Pt1.x, m_Pt1.y, m_Pt2.x, m_Pt2.y));
      break;
  }
  _userTouchesAction = userTouchesAction;
}

#pragma mark - Properties

- (void)setAppWallAd:(MTRGNativeAppwallAd *)appWallAd
{
  _appWallAd = appWallAd;
  [self.controlsManager refreshLayout];
}

- (MWMMapViewControlsManager *)controlsManager
{
  if (!_controlsManager)
    _controlsManager = [[MWMMapViewControlsManager alloc] initWithParentController:self];
  return _controlsManager;
}

- (BOOL)isAppWallAdActive
{
  BOOL const haveAppWall = (self.appWallAd != nil);
  BOOL const haveBanners = (self.appWallAd.banners && self.appWallAd.banners != 0);
  return haveAppWall && haveBanners;
}

- (BOOL)hasNavigationBar
{
  return NO;
}

@end
