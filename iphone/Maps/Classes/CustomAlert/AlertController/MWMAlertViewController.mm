#import "MWMAlertViewController.h"
#import "Common.h"
#import "MWMController.h"
#import "MWMDownloadTransitMapAlert.h"
#import "MWMLocationAlert.h"
#import "MWMLocationNotFoundAlert.h"
#import "MWMMobileInternetAlert.h"
#import "MWMSearchNoResultsAlert.h"
#import "MapViewController.h"
#import "MapsAppDelegate.h"

static NSString * const kAlertControllerNibIdentifier = @"MWMAlertViewController";

@interface MWMAlertViewController ()<UIGestureRecognizerDelegate>

@property(weak, nonatomic, readwrite) UIViewController * ownerViewController;

@end

@implementation MWMAlertViewController

+ (nonnull MWMAlertViewController *)activeAlertController
{
  UIViewController * tvc = topViewController();
  ASSERT([tvc conformsToProtocol:@protocol(MWMController)], ());
  UIViewController<MWMController> * mwmController =
      static_cast<UIViewController<MWMController> *>(tvc);
  return mwmController.alertController;
}

- (nonnull instancetype)initWithViewController:(nonnull UIViewController *)viewController
{
  self = [super initWithNibName:kAlertControllerNibIdentifier bundle:nil];
  if (self)
    self.ownerViewController = viewController;
  return self;
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:(id<UIViewControllerTransitionCoordinator>)coordinator
{
  auto const orient = size.width > size.height ? UIInterfaceOrientationLandscapeLeft : UIInterfaceOrientationPortrait;
  [coordinator animateAlongsideTransition:^(id<UIViewControllerTransitionCoordinatorContext> context) {
    for (MWMAlert * alert in self.view.subviews)
      [alert rotate:orient duration:context.transitionDuration];
  } completion:^(id<UIViewControllerTransitionCoordinatorContext> context) {}];
}

#pragma mark - Actions

- (void)presentRateAlert { [self displayAlert:MWMAlert.rateAlert]; }
- (void)presentLocationAlert
{
  if (![MapViewController controller].pageViewController)
    [self displayAlert:[MWMAlert locationAlert]];
}
- (void)presentPoint2PointAlertWithOkBlock:(nonnull TMWMVoidBlock)okBlock
                             needToRebuild:(BOOL)needToRebuild
{
  [self displayAlert:[MWMAlert point2PointAlertWithOkBlock:okBlock needToRebuild:needToRebuild]];
}

- (void)presentFacebookAlert { [self displayAlert:MWMAlert.facebookAlert]; }
- (void)presentLocationServiceNotSupportedAlert
{
  [self displayAlert:MWMAlert.locationServiceNotSupportedAlert];
}

- (void)presentLocationNotFoundAlertWithOkBlock:(nonnull TMWMVoidBlock)okBlock
{
  [self displayAlert:[MWMLocationNotFoundAlert alertWithOkBlock:okBlock]];
}

- (void)presentNoConnectionAlert { [self displayAlert:[MWMAlert noConnectionAlert]]; }
- (void)presentMigrationProhibitedAlert { [self displayAlert:[MWMAlert migrationProhibitedAlert]]; }
- (void)presentDeleteMapProhibitedAlert { [self displayAlert:[MWMAlert deleteMapProhibitedAlert]]; }
- (void)presentUnsavedEditsAlertWithOkBlock:(nonnull TMWMVoidBlock)okBlock
{
  [self displayAlert:[MWMAlert unsavedEditsAlertWithOkBlock:okBlock]];
}

- (void)presentNoWiFiAlertWithOkBlock:(nullable TMWMVoidBlock)okBlock
{
  [self displayAlert:[MWMAlert noWiFiAlertWithOkBlock:okBlock]];
}

- (void)presentIncorrectFeauturePositionAlert
{
  [self displayAlert:[MWMAlert incorrectFeauturePositionAlert]];
}

- (void)presentInternalErrorAlert { [self displayAlert:[MWMAlert internalErrorAlert]]; }
- (void)presentNotEnoughSpaceAlert { [self displayAlert:[MWMAlert notEnoughSpaceAlert]]; }
- (void)presentInvalidUserNameOrPasswordAlert
{
  [self displayAlert:[MWMAlert invalidUserNameOrPasswordAlert]];
}

- (void)presentRoutingMigrationAlertWithOkBlock:(TMWMVoidBlock)okBlock
{
  [self displayAlert:[MWMAlert routingMigrationAlertWithOkBlock:okBlock]];
}

- (void)presentDownloaderAlertWithCountries:(storage::TCountriesVec const &)countries
                                       code:(routing::IRouter::ResultCode)code
                                cancelBlock:(TMWMVoidBlock)cancelBlock
                              downloadBlock:(TMWMDownloadBlock)downloadBlock
                      downloadCompleteBlock:(TMWMVoidBlock)downloadCompleteBlock
{
  [self displayAlert:[MWMAlert downloaderAlertWithAbsentCountries:countries
                                                             code:code
                                                      cancelBlock:cancelBlock
                                                    downloadBlock:downloadBlock
                                            downloadCompleteBlock:downloadCompleteBlock]];
}

- (void)presentRoutingDisclaimerAlertWithOkBlock:(TMWMVoidBlock)block
{
  [self displayAlert:[MWMAlert routingDisclaimerAlertWithOkBlock:block]];
}

- (void)presentDisabledLocationAlert { [self displayAlert:MWMAlert.disabledLocationAlert]; }
- (void)presentAlert:(routing::IRouter::ResultCode)type
{
  [self displayAlert:[MWMAlert alert:type]];
}

- (void)presentDisableAutoDownloadAlertWithOkBlock:(nonnull TMWMVoidBlock)okBlock
{
  [self displayAlert:[MWMAlert disableAutoDownloadAlertWithOkBlock:okBlock]];
}

- (void)presentDownloaderNoConnectionAlertWithOkBlock:(nonnull TMWMVoidBlock)okBlock
                                          cancelBlock:(nonnull TMWMVoidBlock)cancelBlock
{
  [self displayAlert:[MWMAlert downloaderNoConnectionAlertWithOkBlock:okBlock
                                                          cancelBlock:cancelBlock]];
}

- (void)presentDownloaderNotEnoughSpaceAlert
{
  [self displayAlert:[MWMAlert downloaderNotEnoughSpaceAlert]];
}

- (void)presentDownloaderInternalErrorAlertWithOkBlock:(nonnull TMWMVoidBlock)okBlock
                                           cancelBlock:(nonnull TMWMVoidBlock)cancelBlock
{
  [self displayAlert:[MWMAlert downloaderInternalErrorAlertWithOkBlock:okBlock
                                                           cancelBlock:cancelBlock]];
}

- (void)presentDownloaderNeedUpdateAlertWithOkBlock:(nonnull TMWMVoidBlock)okBlock
{
  [self displayAlert:[MWMAlert downloaderNeedUpdateAlertWithOkBlock:okBlock]];
}

- (void)presentPlaceDoesntExistAlertWithBlock:(MWMStringBlock)block
{
  [self displayAlert:[MWMAlert placeDoesntExistAlertWithBlock:block]];
}

- (void)presentResetChangesAlertWithBlock:(TMWMVoidBlock)block
{
  [self displayAlert:[MWMAlert resetChangesAlertWithBlock:block]];
}

- (void)presentDeleteFeatureAlertWithBlock:(TMWMVoidBlock)block
{
  [self displayAlert:[MWMAlert deleteFeatureAlertWithBlock:block]];
}

- (void)presentPersonalInfoWarningAlertWithBlock:(nonnull TMWMVoidBlock)block
{
  [self displayAlert:[MWMAlert personalInfoWarningAlertWithBlock:block]];
}

- (void)presentTrackWarningAlertWithCancelBlock:(nonnull TMWMVoidBlock)block
{
  [self displayAlert:[MWMAlert trackWarningAlertWithCancelBlock:block]];
}

- (void)presentSearchNoResultsAlert
{
  Class alertClass = [MWMSearchNoResultsAlert class];
  NSArray<__kindof MWMAlert *> * subviews = self.view.subviews;
  MWMSearchNoResultsAlert * alert = nil;
  for (MWMAlert * view in subviews)
  {
    if (![view isKindOfClass:alertClass])
      continue;
    alert = static_cast<MWMSearchNoResultsAlert *>(view);
    alert.alpha = 1;
    [self.view bringSubviewToFront:alert];
    break;
  }
  if (!alert)
  {
    alert = [MWMSearchNoResultsAlert alert];
    [self displayAlert:alert];
  }
  [alert update];
}

- (void)presentMobileInternetAlertWithBlock:(nonnull TMWMVoidBlock)block
{
  [self displayAlert:[MWMMobileInternetAlert alertWithBlock:block]];
}
- (void)presentEditorViralAlert { [self displayAlert:[MWMAlert editorViralAlert]]; }
- (void)presentOsmAuthAlert { [self displayAlert:[MWMAlert osmAuthAlert]]; }
- (void)displayAlert:(MWMAlert *)alert
{
  // TODO(igrechuhin): Remove this check on location manager refactoring.
  // Workaround for current location manager duplicate error alerts.
  if ([alert isKindOfClass:[MWMLocationAlert class]])
  {
    for (MWMAlert * view in self.view.subviews)
    {
      if ([view isKindOfClass:[MWMLocationAlert class]])
        return;
    }
  }
  [UIView animateWithDuration:kDefaultAnimationDuration
                        delay:0
                      options:UIViewAnimationOptionBeginFromCurrentState
                   animations:^{
                     for (MWMAlert * view in self.view.subviews)
                     {
                       if (view != alert)
                         view.alpha = 0.0;
                     }
                   }
                   completion:nil];

  [self removeFromParentViewController];
  alert.alertController = self;
  [self.ownerViewController addChildViewController:self];
  alert.alpha = 0.;
  CGFloat const scale = 1.1;
  alert.transform = CGAffineTransformMakeScale(scale, scale);
  [UIView animateWithDuration:kDefaultAnimationDuration
                   animations:^{
                     self.view.alpha = 1.;
                     alert.alpha = 1.;
                     alert.transform = CGAffineTransformIdentity;
                   }];
  [MapsAppDelegate.theApp.window endEditing:YES];
}

- (void)closeAlert:(nullable TMWMVoidBlock)completion
{
  NSArray * subviews = self.view.subviews;
  MWMAlert * closeAlert = subviews.lastObject;
  MWMAlert * showAlert = (subviews.count >= 2 ? subviews[subviews.count - 2] : nil);
  [UIView animateWithDuration:kDefaultAnimationDuration
      delay:0
      options:UIViewAnimationOptionBeginFromCurrentState
      animations:^{
        closeAlert.alpha = 0.;
        if (showAlert)
          showAlert.alpha = 1.;
        else
          self.view.alpha = 0.;
      }
      completion:^(BOOL finished) {
        [closeAlert removeFromSuperview];
        if (!showAlert)
        {
          [self.view removeFromSuperview];
          [self removeFromParentViewController];
        }
        if (completion)
          completion();
      }];
}

@end
