#import "MWMAlert.h"

#include "routing/router.hpp"
#include "storage/storage.hpp"

@interface MWMAlertViewController : UIViewController

@property (weak, nonatomic, readonly) UIViewController * _Nullable ownerViewController;

- (instancetype _Nullable)initWithViewController:(UIViewController * _Nonnull)viewController;
- (void)presentAlert:(routing::IRouter::ResultCode)type;
- (void)presentRoutingMigrationAlertWithOkBlock:(TMWMVoidBlock _Nonnull)okBlock;
- (void)presentDownloaderAlertWithCountries:(storage::TCountriesVec const &)countries
                                       code:(routing::IRouter::ResultCode)code
                                    okBlock:(TMWMVoidBlock _Nonnull)okBlock;
- (void)presentRateAlert;
- (void)presentFacebookAlert;
- (void)presentPoint2PointAlertWithOkBlock:(TMWMVoidBlock _Nonnull)okBlock needToRebuild:(BOOL)needToRebuild;
- (void)presentRoutingDisclaimerAlert;
- (void)presentDisabledLocationAlert;
- (void)presentLocationAlert;
- (void)presentLocationServiceNotSupportedAlert;
- (void)presentNoConnectionAlert;
- (void)presentMigrationProhibitedAlert;
- (void)presentUnsavedEditsAlertWithOkBlock:(TMWMVoidBlock _Nonnull)okBlock;
- (void)presentNoWiFiAlertWithName:(NSString * _Nonnull)name okBlock:(TMWMVoidBlock _Nonnull)okBlock;
- (void)presentPedestrianToastAlert:(BOOL)isFirstLaunch;
- (void)presentIncorrectFeauturePositionAlert;
- (void)presentInternalErrorAlert;
- (void)presentInvalidUserNameOrPasswordAlert;
- (void)presentDisableAutoDownloadAlertWithOkBlock:(TMWMVoidBlock _Nonnull)okBlock;
- (void)presentDownloaderNoConnectionAlertWithOkBlock:(TMWMVoidBlock _Nonnull)okBlock cancelBlock:(TMWMVoidBlock _Nonnull)cancelBlock;
- (void)presentDownloaderNotEnoughSpaceAlert;
- (void)presentDownloaderInternalErrorAlertWithOkBlock:(TMWMVoidBlock _Nonnull)okBlock cancelBlock:(TMWMVoidBlock _Nonnull)cancelBlock;
- (void)presentDownloaderNeedUpdateAlertWithOkBlock:(TMWMVoidBlock _Nonnull)okBlock;
- (void)presentEditorViralAlert;
- (void)presentOsmAuthAlert;
- (void)closeAlertWithCompletion:(TMWMVoidBlock _Nonnull)completion;

- (instancetype _Nullable)init __attribute__((unavailable("call -initWithViewController: instead!")));
+ (instancetype _Nullable)new __attribute__((unavailable("call -initWithViewController: instead!")));
- (instancetype _Nullable)initWithCoder:(NSCoder * _Nonnull)aDecoder __attribute__((unavailable("call -initWithViewController: instead!")));
- (instancetype _Nullable)initWithNibName:(NSString * _Nullable)nibNameOrNil bundle:(NSBundle * _Nullable)nibBundleOrNil __attribute__((unavailable("call -initWithViewController: instead!")));
@end
