#import "MWMAlert.h"

@interface MWMDefaultAlert : MWMAlert

+ (instancetype _Nullable)routeNotFoundAlert;
+ (instancetype _Nullable)routeFileNotExistAlert;
+ (instancetype _Nullable)endPointNotFoundAlert;
+ (instancetype _Nullable)startPointNotFoundAlert;
+ (instancetype _Nullable)internalRoutingErrorAlert;
+ (instancetype _Nullable)incorrectFeauturePositionAlert;
+ (instancetype _Nullable)internalErrorAlert;
+ (instancetype _Nullable)invalidUserNameOrPasswordAlert;
+ (instancetype _Nullable)noCurrentPositionAlert;
+ (instancetype _Nullable)pointsInDifferentMWMAlert;
+ (instancetype _Nullable)disabledLocationAlert;
+ (instancetype _Nullable)noWiFiAlertWithName:(NSString *)name okBlock:(TMWMVoidBlock)okBlock;
+ (instancetype _Nullable)noConnectionAlert;
+ (instancetype _Nullable)migrationProhibitedAlert;
+ (instancetype _Nullable)unsavedEditsAlertWithOkBlock:(TMWMVoidBlock)okBlock;
+ (instancetype _Nullable)locationServiceNotSupportedAlert;
+ (instancetype _Nullable)point2PointAlertWithOkBlock:(TMWMVoidBlock)okBlock needToRebuild:(BOOL)needToRebuild;
+ (instancetype _Nullable)disableAutoDownloadAlertWithOkBlock:(TMWMVoidBlock)okBlock;
+ (instancetype _Nullable)downloaderNoConnectionAlertWithOkBlock:(TMWMVoidBlock)okBlock cancelBlock:(TMWMVoidBlock)cancelBlock;
+ (instancetype _Nullable)downloaderNotEnoughSpaceAlert;
+ (instancetype _Nullable)downloaderInternalErrorAlertWithOkBlock:(TMWMVoidBlock)okBlock cancelBlock:(TMWMVoidBlock)cancelBlock;
+ (instancetype _Nullable)downloaderNeedUpdateAlertWithOkBlock:(TMWMVoidBlock)okBlock;
+ (instancetype _Nullable)routingMigrationAlertWithOkBlock:(TMWMVoidBlock)okBlock;

@end
