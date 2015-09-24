#import "Common.h"
#import "EAGLView.h"
#import "MapsAppDelegate.h"
#import "MapViewController.h"
#import "MWMAlertViewController.h"
#import "MWMAPIBar.h"
#import "MWMMapViewControlsManager.h"
#import "RouteState.h"
#import "UIFont+MapsMeFonts.h"
#import "UIViewController+Navigation.h"

#import "3party/Alohalytics/src/alohalytics_objc.h"

#include "Framework.h"

#include "../Statistics/Statistics.h"

#include "map/user_mark.hpp"

#include "drape_frontend/user_event_stream.hpp"

#include "../../../platform/file_logging.hpp"
#include "../../../platform/platform.hpp"
#include "../../../platform/settings.hpp"

extern NSString * const kAlohalyticsTapEventKey = @"$onClick";

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

@interface MapViewController ()

@property (nonatomic, readwrite) MWMMapViewControlsManager * controlsManager;
@property (nonatomic) MWMSideMenuState menuRestoreState;

@property (nonatomic) ForceRoutingStateChange forceRoutingStateChange;
@property (nonatomic) BOOL disableStandbyOnLocationStateMode;

@property (nonatomic) MWMAlertViewController * alertController;

@property (nonatomic) UserTouchesAction userTouchesAction;

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
  
  [self.controlsManager playTurnNotifications];
}

- (void)onCompassUpdate:(location::CompassInfo const &)info
{
  // TODO: Remove this hack for orientation changing bug
  if (self.navigationController.visibleViewController == self)
    GetFramework().OnCompassUpdate(info);
}

- (void)onLocationStateModeChanged:(location::EMyPositionMode)newMode
{
  [m_predictor setMode:newMode];
  [self.controlsManager setMyPositionMode:newMode];

  switch (newMode)
  {
    case location::MODE_UNKNOWN_POSITION:
    {
      self.disableStandbyOnLocationStateMode = NO;
      [[MapsAppDelegate theApp].m_locationManager stop:self];
      break;
    }
    case location::MODE_PENDING_POSITION:
      self.disableStandbyOnLocationStateMode = NO;
      [[MapsAppDelegate theApp].m_locationManager start:self];
      break;
    case location::MODE_NOT_FOLLOW:
      self.disableStandbyOnLocationStateMode = NO;
      break;
    case location::MODE_FOLLOW:
    case location::MODE_ROTATE_AND_FOLLOW:
      self.disableStandbyOnLocationStateMode = YES;
      break;
  }
}

#pragma mark - Restore route

- (void)restoreRoute
{
  self.forceRoutingStateChange = ForceRoutingStateChangeStartFollowing;
  auto & f = GetFramework();
  m2::PointD const location = ToMercator([MapsAppDelegate theApp].m_locationManager.lastLocation.coordinate);
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
  if (mark == nullptr)
  {
    [self dismissPlacePage];
    
    auto & f = GetFramework();
    if (!f.HasActiveUserMark() && self.controlsManager.searchHidden && !f.IsRouteNavigable())
      self.controlsManager.hidden = !self.controlsManager.hidden;
  }
  else
  {
    self.controlsManager.hidden = NO;
    [self.controlsManager showPlacePageWithUserMark:move(mark)];
  }
}

- (void)onMyPositionClicked:(id)sender
{
  GetFramework().SwitchMyPositionNextMode();
}

- (void)popoverControllerDidDismissPopover:(UIPopoverController *)popoverController
{
  [self destroyPopover];
}

- (void)checkMaskedPointer:(UITouch *)touch withEvent:(df::TouchEvent &)e
{
  int64_t id = reinterpret_cast<int64_t>(touch);
  int8_t pointerIndex = df::TouchEvent::INVALID_MASKED_POINTER;
  if (e.m_touches[0].m_id == id)
    pointerIndex = 0;
  else if (e.m_touches[1].m_id == id)
    pointerIndex = 1;

  if (e.GetFirstMaskedPointer() == df::TouchEvent::INVALID_MASKED_POINTER)
    e.SetFirstMaskedPointer(pointerIndex);
  else
    e.SetSecondMaskedPointer(pointerIndex);
}

- (void)sendTouchType:(df::TouchEvent::ETouchType)type withTouches:(NSSet *)touches andEvent:(UIEvent *)event
{
  NSArray * allTouches = [[event allTouches] allObjects];
  if ([allTouches count] < 1)
    return;

  UIView * v = self.view;
  CGFloat const scaleFactor = v.contentScaleFactor;

  df::TouchEvent e;
  UITouch * touch = [allTouches objectAtIndex:0];
  CGPoint const pt = [touch locationInView:v];
  e.m_type = type;
  e.m_touches[0].m_id = reinterpret_cast<int64_t>(touch);
  e.m_touches[0].m_location = m2::PointD(pt.x * scaleFactor, pt.y * scaleFactor);
  if (allTouches.count > 1)
  {
    UITouch * touch = [allTouches objectAtIndex:1];
    CGPoint const pt = [touch locationInView:v];
    e.m_touches[1].m_id = reinterpret_cast<int64_t>(touch);
    e.m_touches[1].m_location = m2::PointD(pt.x * scaleFactor, pt.y * scaleFactor);
  }

  NSArray * toggledTouches = [touches allObjects];
  if (toggledTouches.count > 0)
    [self checkMaskedPointer:[toggledTouches objectAtIndex:0] withEvent:e];

  if (toggledTouches.count > 1)
    [self checkMaskedPointer:[toggledTouches objectAtIndex:1] withEvent:e];

  Framework & f = GetFramework();
  f.TouchEvent(e);
}

- (void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)event
{
  [self sendTouchType:df::TouchEvent::TOUCH_DOWN withTouches:touches andEvent:event];
}

- (void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *)event
{
  [self sendTouchType:df::TouchEvent::TOUCH_MOVE withTouches:nil andEvent:event];
}

- (void)touchesEnded:(NSSet *)touches withEvent:(UIEvent *)event
{
  [self sendTouchType:df::TouchEvent::TOUCH_UP withTouches:touches andEvent:event];
}

- (void)touchesCancelled:(NSSet *)touches withEvent:(UIEvent *)event
{
  [self sendTouchType:df::TouchEvent::TOUCH_CANCEL withTouches:touches andEvent:event];
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
    [(UIViewController *)self.childViewControllers.firstObject
        willRotateToInterfaceOrientation:toInterfaceOrientation
                                duration:duration];
  [self.controlsManager willRotateToInterfaceOrientation:toInterfaceOrientation duration:duration];
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:(id<UIViewControllerTransitionCoordinator>)coordinator
{
  [self.controlsManager viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
}

- (void)didRotateFromInterfaceOrientation:(UIInterfaceOrientation)fromInterfaceOrientation
{
  [self showPopover];
}

- (void)didReceiveMemoryWarning
{
  GetFramework().MemoryWarning();
  [super didReceiveMemoryWarning];
}

- (void)onTerminate
{
  GetFramework().SaveState();
  [(EAGLView *)self.view deallocateNative];
}

- (void)onEnterBackground
{
  // Save state and notify about entering background.

  Framework & f = GetFramework();
  f.SaveState();
  f.EnterBackground();
}

- (void)setMapStyle:(MapStyle)mapStyle
{
  GetFramework().SetMapStyle(mapStyle);
}

- (void)onEnterForeground
{
  // Notify about entering foreground (should be called on the first launch too).
  GetFramework().EnterForeground();
}

- (void)viewWillAppear:(BOOL)animated
{
  [super viewWillAppear:animated];
  [[NSNotificationCenter defaultCenter] removeObserver:self name:UIDeviceOrientationDidChangeNotification object:nil];

  self.controlsManager.menuState = self.menuRestoreState;
}

- (void)viewDidLoad
{
  [super viewDidLoad];
  self.view.clipsToBounds = YES;
  self.controlsManager = [[MWMMapViewControlsManager alloc] initWithParentController:self];
}

- (void)viewDidAppear:(BOOL)animated
{
  [super viewDidAppear:animated];
  self.menuRestoreState = self.controlsManager.menuState;
}

- (void)viewWillDisappear:(BOOL)animated
{
  [super viewWillDisappear:animated];
  [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(orientationChanged:) name:UIDeviceOrientationDidChangeNotification object:nil];
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
                       self.controlsManager.menuState == MWMSideMenuStateActive ||
                       self.controlsManager.isDirectionViewShown ||
                       (GetFramework().GetMapStyle() == MapStyleDark &&
                        self.controlsManager.navigationState == MWMNavigationDashboardStateHidden);
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

    SEL userMarkSelector = @selector(onUserMarkClicked:);
    UserMarkActivatedFnT userMarkFn = (UserMarkActivatedFnT)[self methodForSelector:userMarkSelector];
    f.SetUserMarkActivationListener(bind(userMarkFn, self, userMarkSelector, _1));
    m_predictor = [[LocationPredictor alloc] initWithObserver:self];

    self.forceRoutingStateChange = ForceRoutingStateChangeNone;
    self.userTouchesAction = UserTouchesActionNone;
    self.menuRestoreState = MWMSideMenuStateInactive;

    f.LoadBookmarks();

    using TLocationStateModeFn = void (*)(id, SEL, location::EMyPositionMode);
    SEL locationStateModeSelector = @selector(onLocationStateModeChanged:);
    TLocationStateModeFn locationStateModeFn = (TLocationStateModeFn)[self methodForSelector:locationStateModeSelector];
    f.SetMyPositionModeListener(bind(locationStateModeFn, self, locationStateModeSelector, _1));

    f.SetDownloadCountryListener([self, &f](storage::TIndex const & idx, int opt)
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
          self.controlsManager.routeBuildingProgress = 100.;
          f.DeactivateUserMark();
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

#pragma mark - Private methods

- (void)destroyPopover
{
  self.popoverVC = nil;
}

- (void)showPopover
{
  Framework & f = GetFramework();
  if (self.popoverVC)
    f.DeactivateUserMark();

  double const sf = self.view.contentScaleFactor;

  m2::PointD tmp = m2::PointD(f.GtoP(m2::PointD(m_popoverPos.x, m_popoverPos.y)));

  [self.popoverVC presentPopoverFromRect:CGRectMake(tmp.x / sf, tmp.y / sf, 1, 1) inView:self.view permittedArrowDirections:UIPopoverArrowDirectionAny animated:YES];
}

- (void)dismissPopover
{
  [self.popoverVC dismissPopoverAnimated:YES];
  [self destroyPopover];
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

@end
