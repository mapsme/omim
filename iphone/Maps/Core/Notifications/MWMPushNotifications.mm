#import "MWMPushNotifications.h"
#import <Crashlytics/Crashlytics.h>
#import <Pushwoosh/PushNotificationManager.h>
#import <UserNotifications/UserNotifications.h>
#import "MWMCommon.h"
#import "Statistics.h"

#include "platform/marketing_service.hpp"
#include "platform/platform.hpp"

#import "3party/Alohalytics/src/alohalytics_objc.h"

#include "base/logging.hpp"

// If you have a "missing header error" here, then please run configure.sh script in the root repo
// folder.
#import "private.h"

#include "std/string.hpp"

namespace
{
NSString * const kPushDeviceTokenLogEvent = @"iOSPushDeviceToken";
}  // namespace

@implementation MWMPushNotifications

+ (void)setup:(NSDictionary *)launchOptions
{
  PushNotificationManager * pushManager = [PushNotificationManager pushManager];

  [pushManager handlePushReceived:launchOptions];

  // make sure we count app open in Pushwoosh stats
  [pushManager sendAppOpen];

  // register for push notifications!
  [pushManager registerForPushNotifications];
}

+ (void)application:(UIApplication *)application
    didRegisterForRemoteNotificationsWithDeviceToken:(NSData *)deviceToken
{
  LOG(LINFO, ("PN Original token: ", [self stringWithDeviceToken:deviceToken].UTF8String));
  PushNotificationManager * pushManager = [PushNotificationManager pushManager];
  [pushManager handlePushRegistration:deviceToken];
  LOG(LINFO, ("PN Pushwoosh token: ", [pushManager getPushToken].UTF8String));
  [Alohalytics logEvent:kPushDeviceTokenLogEvent withValue:pushManager.getHWID];
}

+ (void)application:(UIApplication *)application
    didFailToRegisterForRemoteNotificationsWithError:(NSError *)error
{
  LOG(LINFO, ("PN Fail to Register: ", error));
  [[PushNotificationManager pushManager] handlePushRegistrationFailure:error];
  [[Crashlytics sharedInstance] recordError:error];
}

+ (NSString *)stringWithDeviceToken:(NSData *)deviceToken {
  return [[[NSString stringWithFormat:@"%@",deviceToken]
           stringByTrimmingCharactersInSet:[NSCharacterSet characterSetWithCharactersInString:@"<>"]] stringByReplacingOccurrencesOfString:@" " withString:@""];
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
  if (app.applicationState != UIApplicationStateInactive)
    return NO;

  NSString * openLink = userInfo[@"openURL"];
  if (!openLink)
    return NO;

  NSURL * url = [NSURL URLWithString:openLink];
  [app openURL:url options:@{} completionHandler:nil];
  return YES;
}

+ (void)userNotificationCenter:(UNUserNotificationCenter *)center
       willPresentNotification:(UNNotification *)notification
         withCompletionHandler:(void (^)(UNNotificationPresentationOptions options))completionHandler
{
  [[PushNotificationManager pushManager].notificationCenterDelegate userNotificationCenter:center
                                                                   willPresentNotification:notification
                                                                     withCompletionHandler:completionHandler];
}

+ (void)userNotificationCenter:(UNUserNotificationCenter *)center
didReceiveNotificationResponse:(UNNotificationResponse *)response
         withCompletionHandler:(void(^)(void))completionHandler
{
  [[PushNotificationManager pushManager].notificationCenterDelegate userNotificationCenter:center
                                                            didReceiveNotificationResponse:response
                                                                     withCompletionHandler:completionHandler];
}

+ (NSString *)formattedTimestamp {
  return @(GetPlatform().GetMarketingService().GetPushWooshTimestamp().c_str());
}

@end
