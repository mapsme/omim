typedef void (^CompletionHandler)(UIBackgroundFetchResult);

@interface LocalNotificationManager : NSObject

+ (instancetype _Nullable)sharedManager;

- (void)showDownloadMapNotificationIfNeeded:(CompletionHandler)completionHandler;
- (void)processNotification:(UILocalNotification *)notification onLaunch:(BOOL)onLaunch;

@end
