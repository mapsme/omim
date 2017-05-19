#import "MWMPushNotifications.h"
#import "MWMCommon.h"
#import <Pushwoosh/PushNotificationManager.h>
#import <UserNotifications/UserNotifications.h>
#import "Statistics.h"

#import "3party/Alohalytics/src/alohalytics_objc.h"

// If you have a "missing header error" here, then please run configure.sh script in the root repo
// folder.
#import "private.h"

#include "base/logging.hpp"

#include "std/string.hpp"

namespace
{
NSString * const kPushDeviceTokenLogEvent = @"iOSPushDeviceToken";
}  // namespace

@implementation MWMPushNotifications

+ (void)setup:(NSDictionary *)launchOptions
{
  PushNotificationManager * pushManager = [PushNotificationManager pushManager];

  if (!isIOSVersionLessThan(10))
    [UNUserNotificationCenter currentNotificationCenter].delegate = pushManager.notificationCenterDelegate;

  [pushManager handlePushReceived:launchOptions];

  // make sure we count app open in Pushwoosh stats
  [pushManager sendAppOpen];

  // register for push notifications!
  [pushManager registerForPushNotifications];
}

+ (void)application:(UIApplication *)application
    didRegisterForRemoteNotificationsWithDeviceToken:(NSData *)deviceToken
{
  PushNotificationManager * pushManager = [PushNotificationManager pushManager];
  [pushManager handlePushRegistration:deviceToken];
  [Alohalytics logEvent:kPushDeviceTokenLogEvent withValue:pushManager.getHWID];
}

+ (void)application:(UIApplication *)application
    didFailToRegisterForRemoteNotificationsWithError:(NSError *)error
{
  [[PushNotificationManager pushManager] handlePushRegistrationFailure:error];
}

+ (void)application:(UIApplication *)application
    didReceiveRemoteNotification:(NSDictionary *)userInfo
          fetchCompletionHandler:(void (^)(UIBackgroundFetchResult))completionHandler
{
  [Statistics logEvent:kStatEventName(kStatApplication, kStatPushReceived) withParameters:userInfo];
  if (![self handleURLPush:userInfo])
    [[PushNotificationManager pushManager] handlePushReceived:userInfo];
  completionHandler(UIBackgroundFetchResultNoData);
}

+ (BOOL)handleURLPush:(NSDictionary *)userInfo
{
  auto app = UIApplication.sharedApplication;
  LOG(LWARNING, ("Handle url push"));
  [userInfo enumerateKeysAndObjectsUsingBlock:^(NSString *  _Nonnull key, id  _Nonnull obj, BOOL * _Nonnull stop) {
    LOG(LWARNING, ("Key in push's user info:", key.UTF8String));
  }];
  if (app.applicationState != UIApplicationStateInactive)
    return NO;
  NSString * openLink = userInfo[@"openURL"];
  LOG(LWARNING, ("Push's url:", openLink.UTF8String));
  if (!openLink)
    return NO;
  NSURL * url = [NSURL URLWithString:openLink];
  [app openURL:url];
  return YES;
}

+ (NSString *)pushToken { return [[PushNotificationManager pushManager] getPushToken]; }
@end
