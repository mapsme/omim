#import "AppInfo.h"
#import "Common.h"
#import "LocalNotificationManager.h"
#import "LocationManager.h"
#import "MapsAppDelegate.h"
#import "MapViewController.h"
#import "MWMAlertViewController.h"
#import "MWMWatchEventInfo.h"
#import "Preferences.h"
#import "RouteState.h"
#import "Statistics.h"
#import <CoreTelephony/CTTelephonyNetworkInfo.h>
#import <FBSDKCoreKit/FBSDKCoreKit.h>
#import <Parse/Parse.h>
#import <ParseFacebookUtilsV4/PFFacebookUtils.h>

#import "3party/Alohalytics/src/alohalytics_objc.h"

#include <sys/xattr.h>

#include "storage/storage_defines.hpp"

#import "platform/http_thread_apple.h"
#include "platform/settings.hpp"
#include "platform/platform_ios.hpp"
#include "platform/preferred_languages.hpp"

// If you have a "missing header error" here, then please run configure.sh script in the root repo folder.
#import "../../../private.h"

extern NSString * const MapsStatusChangedNotification = @"MapsStatusChangedNotification";
// Alert keys.
static NSString * const kUDLastLaunchDateKey = @"LastLaunchDate";
extern NSString * const kUDAlreadyRatedKey = @"UserAlreadyRatedApp";
static NSString * const kUDSessionsCountKey = @"SessionsCount";
static NSString * const kUDFirstVersionKey = @"FirstVersion";
static NSString * const kUDLastRateRequestDate = @"LastRateRequestDate";
extern NSString * const kUDAlreadySharedKey = @"UserAlreadyShared";
static NSString * const kUDLastShareRequstDate = @"LastShareRequestDate";
static NSString * const kNewWatchUserEventKey = @"NewWatchUser";
static NSString * const kOldWatchUserEventKey = @"OldWatchUser";
static NSString * const kUDWatchEventAlreadyTracked = @"WatchEventAlreadyTracked";
static NSString * const kPushDeviceTokenLogEvent = @"iOSPushDeviceToken";
static NSString * const kIOSIDFA = @"IFA";
static NSString * const kBundleVersion = @"BundleVersion";

extern string const kCountryCodeKey;
extern string const kUniqueIdKey;
extern string const kLanguageKey;

/// Adds needed localized strings to C++ code
/// @TODO Refactor localization mechanism to make it simpler
void InitLocalizedStrings()
{
  Framework & f = GetFramework();
  // Texts on the map screen when map is not downloaded or is downloading
  f.AddString("country_status_added_to_queue", [L(@"country_status_added_to_queue") UTF8String]);
  f.AddString("country_status_downloading", [L(@"country_status_downloading") UTF8String]);
  f.AddString("country_status_download", [L(@"country_status_download") UTF8String]);
  f.AddString("country_status_download_without_routing", [L(@"country_status_download_without_routing") UTF8String]);
  f.AddString("country_status_download_failed", [L(@"country_status_download_failed") UTF8String]);
  f.AddString("try_again", [L(@"try_again") UTF8String]);
  // Default texts for bookmarks added in C++ code (by URL Scheme API)
  f.AddString("dropped_pin", [L(@"dropped_pin") UTF8String]);
  f.AddString("my_places", [L(@"my_places") UTF8String]);
  f.AddString("my_position", [L(@"my_position") UTF8String]);
  f.AddString("routes", [L(@"routes") UTF8String]);

  f.AddString("routing_failed_unknown_my_position", [L(@"routing_failed_unknown_my_position") UTF8String]);
  f.AddString("routing_failed_has_no_routing_file", [L(@"routing_failed_has_no_routing_file") UTF8String]);
  f.AddString("routing_failed_start_point_not_found", [L(@"routing_failed_start_point_not_found") UTF8String]);
  f.AddString("routing_failed_dst_point_not_found", [L(@"routing_failed_dst_point_not_found") UTF8String]);
  f.AddString("routing_failed_cross_mwm_building", [L(@"routing_failed_cross_mwm_building") UTF8String]);
  f.AddString("routing_failed_route_not_found", [L(@"routing_failed_route_not_found") UTF8String]);
  f.AddString("routing_failed_internal_error", [L(@"routing_failed_internal_error") UTF8String]);
}

@interface MapsAppDelegate ()

@property (nonatomic) NSInteger standbyCounter;

@end

@implementation MapsAppDelegate
{
  NSString * m_geoURL;
  NSString * m_mwmURL;
  NSString * m_fileURL;

  NSString * m_scheme;
  NSString * m_sourceApplication;
  ActiveMapsObserver * m_mapsObserver;
}

+ (MapsAppDelegate *)theApp
{
  return (MapsAppDelegate *)[UIApplication sharedApplication].delegate;
}

#pragma mark - Notifications

- (void)registerNotifications:(UIApplication *)application launchOptions:(NSDictionary *)launchOptions
{
  [Parse enableLocalDatastore];
  [Parse setApplicationId:@(PARSE_APPLICATION_ID) clientKey:@(PARSE_CLIENT_KEY)];
  [PFFacebookUtils initializeFacebookWithApplicationLaunchOptions:launchOptions];
  UIUserNotificationType userNotificationTypes = (UIUserNotificationTypeAlert | UIUserNotificationTypeBadge | UIUserNotificationTypeSound);
  if ([application respondsToSelector: @selector(registerUserNotificationSettings:)])
  {
    UIUserNotificationSettings * const settings = [UIUserNotificationSettings settingsForTypes:userNotificationTypes categories:nil];
    [application registerUserNotificationSettings:settings];
    [application registerForRemoteNotifications];
  }
  else
  {
    [application registerForRemoteNotificationTypes:userNotificationTypes];
  }
}

- (void)application:(UIApplication *)application didRegisterForRemoteNotificationsWithDeviceToken:(NSData *)deviceToken
{
  PFInstallation * currentInstallation = [PFInstallation currentInstallation];
  [currentInstallation setDeviceTokenFromData:deviceToken];
  AppInfo * appInfo = [AppInfo sharedInfo];
  NSUUID * advertisingId = appInfo.advertisingId;
  if (advertisingId)
    [currentInstallation setObject:advertisingId.UUIDString forKey:kIOSIDFA];
  [currentInstallation setObject:appInfo.countryCode forKey:@(kCountryCodeKey.c_str())];
  [currentInstallation setObject:appInfo.uniqueId forKey:@(kUniqueIdKey.c_str())];
  NSString * languageId = appInfo.languageId;
  if (languageId)
    [currentInstallation setObject:languageId forKey:@(kLanguageKey.c_str())];
  [currentInstallation setObject:appInfo.bundleVersion forKey:kBundleVersion];
  [currentInstallation saveInBackground];

  [Alohalytics logEvent:kPushDeviceTokenLogEvent withValue:currentInstallation.deviceToken];
}

- (void)application:(UIApplication *)application didReceiveRemoteNotification:(NSDictionary *)userInfo fetchCompletionHandler:(void (^)(UIBackgroundFetchResult))completionHandler
{
  [Statistics.instance logEvent:@"Push received" withParameters:userInfo];
  if (![self handleURLPush:userInfo])
    [PFPush handlePush:userInfo];
  completionHandler(UIBackgroundFetchResultNoData);
}

- (BOOL)handleURLPush:(NSDictionary *)userInfo
{
  auto app = UIApplication.sharedApplication;
  if (app.applicationState != UIApplicationStateInactive)
    return NO;
  NSString * openLink = userInfo[@"openURL"];
  if (!openLink)
    return NO;
  [app openURL:[NSURL URLWithString:openLink]];
  return YES;
}

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
  // Initialize all 3party engines.
  BOOL returnValue = [self initStatistics:application didFinishLaunchingWithOptions:launchOptions];

  NSURL * urlUsedToLaunchMaps = launchOptions[UIApplicationLaunchOptionsURLKey];
  if (urlUsedToLaunchMaps != nil)
    returnValue |= [self checkLaunchURL:urlUsedToLaunchMaps];
  else
    returnValue = YES;

  [HttpThread setDownloadIndicatorProtocol:self];

  [self trackWatchUser];

  InitLocalizedStrings();
  
  [self.mapViewController onEnterForeground];

  [Preferences setup:self.mapViewController];
  _m_locationManager = [[LocationManager alloc] init];

  [self subscribeToStorage];

  [self customizeAppearance];
  
  self.standbyCounter = 0;

  NSTimeInterval const minimumBackgroundFetchIntervalInSeconds = 6 * 60 * 60;
  [application setMinimumBackgroundFetchInterval:minimumBackgroundFetchIntervalInSeconds];

  [self registerNotifications:application launchOptions:launchOptions];

  LocalNotificationManager * notificationManager = [LocalNotificationManager sharedManager];
  if (launchOptions[UIApplicationLaunchOptionsLocalNotificationKey])
    [notificationManager processNotification:launchOptions[UIApplicationLaunchOptionsLocalNotificationKey] onLaunch:YES];
  
  if ([Alohalytics isFirstSession])
  {
    [self firstLaunchSetup];
  }
  else
  {
    [self incrementSessionCount];
    [self showAlertIfRequired];
  }
  
  Framework & f = GetFramework();
  application.applicationIconBadgeNumber = f.GetCountryTree().GetActiveMapLayout().GetOutOfDateCount();
  f.InvalidateMyPosition();

  return returnValue;
}

- (void)application:(UIApplication *)application performFetchWithCompletionHandler:(void (^)(UIBackgroundFetchResult))completionHandler
{
  // At the moment, we need to perform 2 asynchronous background tasks simultaneously:
  // 1. Check if map for current location is already downloaded, and if not - notify user to download it.
  // 2. Try to send collected statistics (if any) to our server.
  [Alohalytics forceUpload];
  [[LocalNotificationManager sharedManager] showDownloadMapNotificationIfNeeded:completionHandler];
}

- (void)applicationWillTerminate:(UIApplication *)application
{
  [self.mapViewController onTerminate];
}

- (void)applicationDidEnterBackground:(UIApplication *)application
{
  [self.mapViewController onEnterBackground];
  if (m_activeDownloadsCounter)
  {
    m_backgroundTask = [application beginBackgroundTaskWithExpirationHandler:^{
      [application endBackgroundTask:self->m_backgroundTask];
      self->m_backgroundTask = UIBackgroundTaskInvalid;
    }];
  }
}

- (void)applicationWillResignActive:(UIApplication *)application
{
  [RouteState save];
}

- (void)applicationWillEnterForeground:(UIApplication *)application
{
  [self.m_locationManager orientationChanged];
  [self.mapViewController onEnterForeground];
}

- (void)applicationDidBecomeActive:(UIApplication *)application
{
  Framework & f = GetFramework();
  if (m_geoURL)
  {
    if (f.ShowMapForURL([m_geoURL UTF8String]))
    {
      if ([m_scheme isEqualToString:@"geo"])
        [[Statistics instance] logEvent:@"geo Import"];
      if ([m_scheme isEqualToString:@"ge0"])
        [[Statistics instance] logEvent:@"ge0(zero) Import"];

      [self showMap];
    }
  }
  else if (m_mwmURL)
  {
    if (f.ShowMapForURL([m_mwmURL UTF8String]))
    {
      [[Statistics instance] logApiUsage:m_sourceApplication];
      [self showMap];
      [self.mapViewController showAPIBar];
    }
  }
  else if (m_fileURL)
  {
    if (!f.AddBookmarksFile([m_fileURL UTF8String]))
      [self showLoadFileAlertIsSuccessful:NO];

    [[NSNotificationCenter defaultCenter] postNotificationName:@"KML file added" object:nil];
    [self showLoadFileAlertIsSuccessful:YES];
    [[Statistics instance] logEvent:@"KML Import"];
  }
  else
  {
    UIPasteboard * pasteboard = [UIPasteboard generalPasteboard];
    if ([pasteboard.string length])
    {
      if (f.ShowMapForURL([pasteboard.string UTF8String]))
      {
        [self showMap];
        pasteboard.string = @"";
      }
    }
  }
  m_geoURL = nil;
  m_mwmURL = nil;
  m_fileURL = nil;

  [self restoreRouteState];

  [Statistics.instance applicationDidBecomeActive];
}

- (void)dealloc
{
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  // Global cleanup
  DeleteFramework();
}

- (BOOL)initStatistics:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
  Statistics * statistics = [Statistics instance];
  BOOL returnValue = [statistics application:application didFinishLaunchingWithOptions:launchOptions];
  [statistics logEvent:@"Device Info" withParameters:@{@"Country" : [AppInfo sharedInfo].countryCode}];

  NSString * connectionType;
  switch (Platform::ConnectionStatus())
  {
    case Platform::EConnectionType::CONNECTION_NONE:
      break;
    case Platform::EConnectionType::CONNECTION_WIFI:
      connectionType = @"Wi-Fi";
      break;
    case Platform::EConnectionType::CONNECTION_WWAN:
      connectionType = [[CTTelephonyNetworkInfo alloc] init].currentRadioAccessTechnology;
      break;
  }
  if (!connectionType)
    connectionType = @"Offline";
  [statistics logEvent:@"Connection" withParameters:@{@"Type" : connectionType}];

  return returnValue;
}

- (void)disableDownloadIndicator
{
  --m_activeDownloadsCounter;
  if (m_activeDownloadsCounter <= 0)
  {
    [UIApplication sharedApplication].networkActivityIndicatorVisible = NO;
    m_activeDownloadsCounter = 0;
    if ([UIApplication sharedApplication].applicationState == UIApplicationStateBackground)
    {
      [[UIApplication sharedApplication] endBackgroundTask:m_backgroundTask];
      m_backgroundTask = UIBackgroundTaskInvalid;
    }
  }
}

- (void)enableDownloadIndicator
{
  ++m_activeDownloadsCounter;
  [UIApplication sharedApplication].networkActivityIndicatorVisible = YES;
}


- (void)setMapStyle:(MapStyle)mapStyle
{
  [self.mapViewController setMapStyle: mapStyle];
}

- (void)customizeAppearance
{
  NSMutableDictionary * attributes = [[NSMutableDictionary alloc] init];
  attributes[NSForegroundColorAttributeName] = [UIColor whiteColor];

  Class const navigationControllerClass = [NavigationController class];
  [[UINavigationBar appearanceWhenContainedIn:navigationControllerClass, nil] setTintColor:[UIColor whiteColor]];
  [[UIBarButtonItem appearance] setTitleTextAttributes:attributes forState:UIControlStateNormal];
  [[UINavigationBar appearanceWhenContainedIn:navigationControllerClass, nil] setBarTintColor:[UIColor colorWithColorCode:@"0e8639"]];
  attributes[NSFontAttributeName] = [UIFont fontWithName:@"HelveticaNeue" size:17.5];

  UINavigationBar * navBar = [UINavigationBar appearanceWhenContainedIn:navigationControllerClass, nil];
  navBar.shadowImage = [[UIImage alloc] init];
  navBar.titleTextAttributes = attributes;
}

- (void)application:(UIApplication *)application didReceiveLocalNotification:(UILocalNotification *)notification
{
  [[LocalNotificationManager sharedManager] processNotification:notification onLaunch:NO];
}

- (BOOL)application:(UIApplication *)application openURL:(NSURL *)url sourceApplication:(NSString *)sourceApplication annotation:(id)annotation
{
  m_sourceApplication = sourceApplication;

  if ([self checkLaunchURL:url])
    return YES;

  return [[FBSDKApplicationDelegate sharedInstance] application:application openURL:url sourceApplication:sourceApplication annotation:annotation];
}

- (void)showLoadFileAlertIsSuccessful:(BOOL)successful
{
  m_loadingAlertView = [[UIAlertView alloc] initWithTitle:L(@"load_kmz_title")
                                                  message:
                        (successful ? L(@"load_kmz_successful") : L(@"load_kmz_failed"))
                                                 delegate:nil
                                        cancelButtonTitle:L(@"ok") otherButtonTitles:nil];
  m_loadingAlertView.delegate = self;
  [m_loadingAlertView show];
  [NSTimer scheduledTimerWithTimeInterval:5.0 target:self selector:@selector(dismissAlert) userInfo:nil repeats:NO];
}

- (BOOL)checkLaunchURL:(NSURL *)url
{
  NSString *scheme = url.scheme;
  m_scheme = scheme;
  if ([scheme isEqualToString:@"geo"] || [scheme isEqualToString:@"ge0"])
  {
    m_geoURL = [url absoluteString];
    return YES;
  }
  else if ([scheme isEqualToString:@"mapswithme"] || [scheme isEqualToString:@"mwm"])
  {
    m_mwmURL = [url absoluteString];
    return YES;
  }
  else if ([scheme isEqualToString:@"file"])
  {
    m_fileURL = [url relativePath];
    return YES;
  }
  NSLog(@"Scheme %@ is not supported", scheme);
  return NO;
}

- (void)dismissAlert
{
  if (m_loadingAlertView)
    [m_loadingAlertView dismissWithClickedButtonIndex:0 animated:YES];
}

- (void)alertView:(UIAlertView *)alertView didDismissWithButtonIndex:(NSInteger)buttonIndex
{
  m_loadingAlertView = nil;
}

- (void)showMap
{
  [(UINavigationController *)self.window.rootViewController popToRootViewControllerAnimated:YES];
  [self.mapViewController dismissPopover];
}

- (void)subscribeToStorage
{
  __weak MapsAppDelegate * weakSelf = self;
  m_mapsObserver = new ActiveMapsObserver(weakSelf);
  GetFramework().GetCountryTree().GetActiveMapLayout().AddListener(m_mapsObserver);

  [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(outOfDateCountriesCountChanged:) name:MapsStatusChangedNotification object:nil];
}

- (void)countryStatusChangedAtPosition:(int)position inGroup:(storage::ActiveMapsLayout::TGroup const &)group
{
  ActiveMapsLayout & l = GetFramework().GetCountryTree().GetActiveMapLayout();
  int const outOfDateCount = l.GetOutOfDateCount();
  [[NSNotificationCenter defaultCenter] postNotificationName:MapsStatusChangedNotification object:nil userInfo:@{@"OutOfDate" : @(outOfDateCount)}];
}

- (void)outOfDateCountriesCountChanged:(NSNotification *)notification
{
  [UIApplication sharedApplication].applicationIconBadgeNumber = [[notification userInfo][@"OutOfDate"] integerValue];
}

- (void)application:(UIApplication *)application handleWatchKitExtensionRequest:(NSDictionary *)userInfo reply:(void (^)(NSDictionary *))reply
{
  switch (userInfo.watchEventInfoRequest)
  {
    case MWMWatchEventInfoRequestMoveWritableDir:
      static_cast<CustomIOSPlatform &>(GetPlatform()).MigrateWritableDirForAppleWatch();
      reply([NSDictionary dictionary]);
      break;
  }
  NSUserDefaults * settings = [[NSUserDefaults alloc] initWithSuiteName:kApplicationGroupIdentifier()];
  [settings setBool:YES forKey:kHaveAppleWatch];
  [settings synchronize];
}

- (void)trackWatchUser
{
  if (isIOSVersionLessThan(8))
    return;

  NSUserDefaults *standartDefaults = [NSUserDefaults standardUserDefaults];
  BOOL const userLaunchAppleWatch = [[[NSUserDefaults alloc] initWithSuiteName:kApplicationGroupIdentifier()] boolForKey:kHaveAppleWatch];
  BOOL const appleWatchLaunchingEventAlreadyTracked = [standartDefaults boolForKey:kUDWatchEventAlreadyTracked];
  if (userLaunchAppleWatch && !appleWatchLaunchingEventAlreadyTracked)
  {
    if (self.userIsNew)
    {
      [Alohalytics logEvent:kNewWatchUserEventKey];
      [[Statistics instance] logEvent:kNewWatchUserEventKey];
    }
    else
    {
      [Alohalytics logEvent:kOldWatchUserEventKey];
      [[Statistics instance] logEvent:kOldWatchUserEventKey];
    }
    [standartDefaults setBool:YES forKey:kUDWatchEventAlreadyTracked];
    [standartDefaults synchronize];
  }
}

#pragma mark - Properties

- (MapViewController *)mapViewController
{
  return [(UINavigationController *)self.window.rootViewController viewControllers].firstObject;
}

#pragma mark - Route state

- (void)restoreRouteState
{
  if (GetFramework().IsRoutingActive())
    return;
  RouteState const * const state = [RouteState savedState];
  if (state.hasActualRoute)
    self.mapViewController.restoreRouteDestination = state.endPoint;
  else
    [RouteState remove];
}

#pragma mark - Standby

- (void)enableStandby
{
  self.standbyCounter--;
}

- (void)disableStandby
{
  self.standbyCounter++;
}

- (void)setStandbyCounter:(NSInteger)standbyCounter
{
  _standbyCounter = MAX(0, standbyCounter);
  [UIApplication sharedApplication].idleTimerDisabled = (_standbyCounter != 0);
}

#pragma mark - Alert logic

- (void)firstLaunchSetup
{
  NSString *currentVersion = [[NSBundle mainBundle] objectForInfoDictionaryKey:(NSString *)kCFBundleVersionKey];
  NSUserDefaults *standartDefaults = [NSUserDefaults standardUserDefaults];
  [standartDefaults setObject:currentVersion forKey:kUDFirstVersionKey];
  [standartDefaults setInteger:1 forKey:kUDSessionsCountKey];
  [standartDefaults setObject:NSDate.date forKey:kUDLastLaunchDateKey];
  [standartDefaults synchronize];
}

- (void)incrementSessionCount
{
  NSUserDefaults *standartDefaults = [NSUserDefaults standardUserDefaults];
  NSUInteger sessionCount = [standartDefaults integerForKey:kUDSessionsCountKey];
  NSUInteger const kMaximumSessionCountForShowingShareAlert = 50;
  if (sessionCount > kMaximumSessionCountForShowingShareAlert)
    return;
  
  NSDate *lastLaunchDate = [standartDefaults objectForKey:kUDLastLaunchDateKey];
  NSUInteger daysFromLastLaunch = [self.class daysBetweenNowAndDate:lastLaunchDate];
  if (daysFromLastLaunch > 0)
  {
    sessionCount++;
    [standartDefaults setInteger:sessionCount forKey:kUDSessionsCountKey];
    [standartDefaults setObject:NSDate.date forKey:kUDLastLaunchDateKey];
    [standartDefaults synchronize];
  }
}

- (void)showAlertIfRequired
{
  if ([self shouldShowRateAlert])
    [self performSelector:@selector(showRateAlert) withObject:nil afterDelay:30.0];
  else if ([self shouldShowFacebookAlert])
    [self performSelector:@selector(showFacebookAlert) withObject:nil afterDelay:30.0];
}

- (void)showAlert:(BOOL)isRate
{
  if (!Platform::IsConnected())
    return;
  
  UIViewController * topViewController = [(UINavigationController*)self.window.rootViewController visibleViewController];
  MWMAlertViewController * alert = [[MWMAlertViewController alloc] initWithViewController:topViewController];
  if (isRate)
    [alert presentRateAlert];
  else
  [alert presentFacebookAlert];
  [[NSUserDefaults standardUserDefaults] setObject:NSDate.date forKey:isRate ? kUDLastRateRequestDate : kUDLastShareRequstDate];
}

#pragma mark - Facebook

- (void)showFacebookAlert
{
  [self showAlert:NO];
}

- (BOOL)shouldShowFacebookAlert
{
  NSUInteger const kMaximumSessionCountForShowingShareAlert = 50;
  NSUserDefaults const * const standartDefaults = [NSUserDefaults standardUserDefaults];
  if ([standartDefaults boolForKey:kUDAlreadySharedKey])
    return NO;
  
  NSUInteger const sessionCount = [standartDefaults integerForKey:kUDSessionsCountKey];
  if (sessionCount > kMaximumSessionCountForShowingShareAlert)
    return NO;
  
  NSDate * const lastShareRequestDate = [standartDefaults objectForKey:kUDLastShareRequstDate];
  NSUInteger const daysFromLastShareRequest = [MapsAppDelegate daysBetweenNowAndDate:lastShareRequestDate];
  if (lastShareRequestDate != nil && daysFromLastShareRequest == 0)
    return NO;
  
  if (sessionCount == 30 || sessionCount == kMaximumSessionCountForShowingShareAlert)
    return YES;
  
  if (self.userIsNew)
  {
    if (sessionCount == 12)
      return YES;
  }
  else
  {
    if (sessionCount == 5)
      return YES;
  }
  return NO;
}

#pragma mark - Rate

- (void)showRateAlert
{
  [self showAlert:YES];
}

- (BOOL)shouldShowRateAlert
{
  NSUInteger const kMaximumSessionCountForShowingAlert = 21;
  NSUserDefaults const * const standartDefaults = [NSUserDefaults standardUserDefaults];
  if ([standartDefaults boolForKey:kUDAlreadyRatedKey])
    return NO;
  
  NSUInteger const sessionCount = [standartDefaults integerForKey:kUDSessionsCountKey];
  if (sessionCount > kMaximumSessionCountForShowingAlert)
    return NO;
  
  NSDate * const lastRateRequestDate = [standartDefaults objectForKey:kUDLastRateRequestDate];
  NSUInteger const daysFromLastRateRequest = [MapsAppDelegate daysBetweenNowAndDate:lastRateRequestDate];
  // Do not show more than one alert per day.
  if (lastRateRequestDate != nil && daysFromLastRateRequest == 0)
    return NO;
  
  if (self.userIsNew)
  {
    // It's new user.
    if (sessionCount == 3 || sessionCount == 10 || sessionCount == kMaximumSessionCountForShowingAlert)
      return YES;
  }
  else
  {
    // User just got updated. Show alert, if it first session or if 90 days spent.
    if (daysFromLastRateRequest >= 90 || daysFromLastRateRequest == 0)
      return YES;
  }
  return NO;
}

- (BOOL)userIsNew
{
  NSString *currentVersion = [[NSBundle mainBundle] objectForInfoDictionaryKey:(NSString *)kCFBundleVersionKey];
  NSString *firstVersion = [[NSUserDefaults standardUserDefaults] stringForKey:kUDFirstVersionKey];
  if (!firstVersion.length || firstVersionIsLessThanSecond(firstVersion, currentVersion))
    return NO;
  
  return YES;
}

+ (NSInteger)daysBetweenNowAndDate:(NSDate*)fromDate
{
  if (!fromDate)
    return 0;
  
  NSDate *now = NSDate.date;
  NSCalendar *calendar = [NSCalendar currentCalendar];
  [calendar rangeOfUnit:NSCalendarUnitDay startDate:&fromDate interval:NULL forDate:fromDate];
  [calendar rangeOfUnit:NSCalendarUnitDay startDate:&now interval:NULL forDate:now];
  NSDateComponents *difference = [calendar components:NSCalendarUnitDay fromDate:fromDate toDate:now options:0];
  return difference.day;
}

@end
