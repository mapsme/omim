#import "Common.h"
#import "MWMAlertViewController.h"
#import "MWMDownloadTransitMapAlert.h"
#import "MapsAppDelegate.h"

static NSString * const kAlertControllerNibIdentifier = @"MWMAlertViewController";

@interface MWMAlertViewController () <UIGestureRecognizerDelegate>

@property (weak, nonatomic, readwrite) UIViewController * _Nullable ownerViewController;

@end

@implementation MWMAlertViewController

- (instancetype _Nullable)initWithViewController:(UIViewController * _Nonnull)viewController
{
  self = [super initWithNibName:kAlertControllerNibIdentifier bundle:nil];
  if (self)
    self.ownerViewController = viewController;
  return self;
}

- (void)willRotateToInterfaceOrientation:(UIInterfaceOrientation)toInterfaceOrientation duration:(NSTimeInterval)duration
{
  MWMAlert * alert = self.view.subviews.firstObject;
  [alert rotate:toInterfaceOrientation duration:duration];
}

#pragma mark - Actions

- (void)presentRateAlert
{
  [self displayAlert:MWMAlert.rateAlert];
}

- (void)presentLocationAlert
{
  NSString * title = L(@"location_is_disabled_long_text");
  NSString * cancel = L(@"cancel");
  NSString * openSettings = L(@"settings");
  if (isIOS7)
  {
    UIAlertView * alertView = [[UIAlertView alloc] initWithTitle:title message:nil delegate:nil cancelButtonTitle:cancel otherButtonTitles:nil];
    [alertView show];
    return;
  }
  UIAlertController * alertController = [UIAlertController alertControllerWithTitle:title message:nil preferredStyle:UIAlertControllerStyleAlert];
  UIAlertAction * cancelAction = [UIAlertAction actionWithTitle:cancel style:UIAlertActionStyleCancel handler:nil];
  UIAlertAction * openSettingsAction = [UIAlertAction actionWithTitle:openSettings style:UIAlertActionStyleDefault handler:^(UIAlertAction * action)
  {
    [self openSettings];
  }];
  [alertController addAction:cancelAction];
  [alertController addAction:openSettingsAction];
  [self.ownerViewController presentViewController:alertController animated:YES completion:nil];
//  dispatch_async(dispatch_get_main_queue(), ^
//  {
//    // @TODO Remove dispatch on LocationManager -> MWMLocationManager
//    // Test case when location is denied by user on app launch/relaunch
//    [self displayAlert:MWMAlert.locationAlert];
//  });
}

- (void)presentPoint2PointAlertWithOkBlock:(TMWMVoidBlock _Nonnull)okBlock needToRebuild:(BOOL)needToRebuild
{
  [self displayAlert:[MWMAlert point2PointAlertWithOkBlock:okBlock needToRebuild:needToRebuild]];
}

- (void)presentFacebookAlert
{
  [self displayAlert:MWMAlert.facebookAlert];
}

- (void)presentLocationServiceNotSupportedAlert
{
  [self displayAlert:MWMAlert.locationServiceNotSupportedAlert];
}

- (void)presentNoConnectionAlert
{
  [self displayAlert:[MWMAlert noConnectionAlert]];
}

- (void)presentMigrationProhibitedAlert
{
  [self displayAlert:[MWMAlert migrationProhibitedAlert]];
}

- (void)presentUnsavedEditsAlertWithOkBlock:(TMWMVoidBlock _Nonnull)okBlock
{
  [self displayAlert:[MWMAlert unsavedEditsAlertWithOkBlock:okBlock]];
}

- (void)presentNoWiFiAlertWithName:(NSString * _Nonnull)name okBlock:(TMWMVoidBlock _Nonnull)okBlock
{
  [self displayAlert:[MWMAlert noWiFiAlertWithName:name okBlock:okBlock]];
}

- (void)presentPedestrianToastAlert:(BOOL)isFirstLaunch
{
  [self displayAlert:[MWMAlert pedestrianToastShareAlert:isFirstLaunch]];
}

- (void)presentIncorrectFeauturePositionAlert
{
  [self displayAlert:[MWMAlert incorrectFeauturePositionAlert]];
}

- (void)presentInternalErrorAlert
{
  [self displayAlert:[MWMAlert internalErrorAlert]];
}

- (void)presentInvalidUserNameOrPasswordAlert
{
  [self displayAlert:[MWMAlert invalidUserNameOrPasswordAlert]];
}

- (void)presentRoutingMigrationAlertWithOkBlock:(TMWMVoidBlock _Nonnull)okBlock
{
  [self displayAlert:[MWMAlert routingMigrationAlertWithOkBlock:okBlock]];
}

- (void)presentDownloaderAlertWithCountries:(storage::TCountriesVec const &)countries
                                       code:(routing::IRouter::ResultCode)code
                                    okBlock:(TMWMVoidBlock _Nonnull)okBlock
{
  [self displayAlert:[MWMAlert downloaderAlertWithAbsentCountries:countries code:code okBlock:okBlock]];
}

- (void)presentRoutingDisclaimerAlert
{
  [self displayAlert:[MWMAlert routingDisclaimerAlertWithInitialOrientation:self.ownerViewController.interfaceOrientation]];
}

- (void)presentDisabledLocationAlert
{
  [self displayAlert:MWMAlert.disabledLocationAlert];
}

- (void)presentAlert:(routing::IRouter::ResultCode)type
{
  [self displayAlert:[MWMAlert alert:type]];
}

- (void)displayAlert:(MWMAlert *)alert
{
  alert.alertController = self;
  [self.ownerViewController addChildViewController:self];
  self.view.alpha = 0.;
  alert.alpha = 0.;
  if (!isIOS7)
  {
    CGFloat const scale = 1.1;
    alert.transform = CGAffineTransformMakeScale(scale, scale);
  }
  [UIView animateWithDuration:kDefaultAnimationDuration animations:^
  {
    self.view.alpha = 1.;
    alert.alpha = 1.;
    if (!isIOS7)
      alert.transform = CGAffineTransformIdentity;
  }];
  [MapsAppDelegate.theApp.window endEditing:YES];
}

- (void)presentDisableAutoDownloadAlertWithOkBlock:(TMWMVoidBlock _Nonnull)okBlock
{
  [self displayAlert:[MWMAlert disableAutoDownloadAlertWithOkBlock:okBlock]];
}

- (void)presentDownloaderNoConnectionAlertWithOkBlock:(TMWMVoidBlock _Nonnull)okBlock cancelBlock:(TMWMVoidBlock _Nonnull)cancelBlock
{
  [self displayAlert:[MWMAlert downloaderNoConnectionAlertWithOkBlock:okBlock cancelBlock:cancelBlock]];
}

- (void)presentDownloaderNotEnoughSpaceAlert
{
  [self displayAlert:[MWMAlert downloaderNotEnoughSpaceAlert]];
}

- (void)presentDownloaderInternalErrorAlertWithOkBlock:(TMWMVoidBlock _Nonnull)okBlock cancelBlock:(TMWMVoidBlock _Nonnull)cancelBlock
{
  [self displayAlert:[MWMAlert downloaderInternalErrorAlertWithOkBlock:okBlock cancelBlock:cancelBlock]];
}

- (void)presentDownloaderNeedUpdateAlertWithOkBlock:(TMWMVoidBlock _Nonnull)okBlock
{
  [self displayAlert:[MWMAlert downloaderNeedUpdateAlertWithOkBlock:okBlock]];
}

- (void)presentEditorViralAlert
{
  [self displayAlert:[MWMAlert editorViralAlert]];
}

- (void)presentOsmAuthAlert
{
  [self displayAlert:[MWMAlert osmAuthAlert]];
}

- (void)closeAlertWithCompletion:(TMWMVoidBlock _Nonnull)completion
{
  MWMAlert * alert = self.view.subviews.firstObject;
  [UIView animateWithDuration:kDefaultAnimationDuration animations:^
  {
    alert.alpha = 0.;
    self.view.alpha = 0.;
  }
  completion:^(BOOL finished)
  {
    completion();
    [self.view removeFromSuperview];
    [self removeFromParentViewController];
  }];
}

- (void)openSettings
{
  NSURL * url = [NSURL URLWithString:UIApplicationOpenSettingsURLString];
  UIApplication * a = [UIApplication sharedApplication];
  if ([a canOpenURL:url])
    [a openURL:url];
}

@end
