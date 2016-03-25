#import "LocationManager.h"
#import "MapsAppDelegate.h"
#import "MapViewController.h"
#import "MWMAlertViewController.h"
#import "MWMDefaultAlert.h"
#import "MWMPlacePageViewManager.h"
#import "Statistics.h"
#import "UIButton+RuntimeAttributes.h"
#import "UILabel+RuntimeAttributes.h"

#include "Framework.h"

static CGFloat const kDividerTopConstant = -8.;
static NSString * kStatisticsEvent = @"Default Alert";

@interface MWMDefaultAlert ()

@property (weak, nonatomic) IBOutlet UILabel * _Nullable messageLabel;
@property (weak, nonatomic) IBOutlet UIButton * _Nullable rightButton;
@property (weak, nonatomic) IBOutlet UIButton * _Nullable leftButton;
@property (weak, nonatomic) IBOutlet UILabel * _Nullable titleLabel;
@property (weak, nonatomic) IBOutlet NSLayoutConstraint * _Nullable rightButtonWidth;
@property (copy, nonatomic) TMWMVoidBlock _Nullable rightButtonAction;
@property (copy, nonatomic) TMWMVoidBlock _Nullable leftButtonAction;
@property (weak, nonatomic) IBOutlet NSLayoutConstraint * _Nullable dividerTop;

@end

static NSString * const kDefaultAlertNibName = @"MWMDefaultAlert";

@implementation MWMDefaultAlert

+ (instancetype _Nullable)routeFileNotExistAlert
{
  kStatisticsEvent = @"Route File Not Exist Alert";
  return [self defaultAlertWithTitle:@"dialog_routing_download_files"
                             message:@"dialog_routing_download_and_update_all"
                    rightButtonTitle:@"ok"
                     leftButtonTitle:nil
                   rightButtonAction:nil];
}

+ (instancetype _Nullable)routeNotFoundAlert
{
  kStatisticsEvent = @"Route File Not Exist Alert";
  NSString * message =
      [NSString stringWithFormat:@"%@\n\n%@", L(@"dialog_routing_cant_build_route"),
                                 L(@"dialog_routing_change_start_or_end")];
  return [self defaultAlertWithTitle:@"dialog_routing_unable_locate_route"
                             message:message
                    rightButtonTitle:@"ok"
                     leftButtonTitle:nil
                   rightButtonAction:nil];
}

+ (instancetype _Nullable)locationServiceNotSupportedAlert
{
  kStatisticsEvent = @"Location Service Not Supported Alert";
  return [self defaultAlertWithTitle:@"device_doesnot_support_location_services"
                             message:nil
                    rightButtonTitle:@"ok"
                     leftButtonTitle:nil
                   rightButtonAction:nil];
}

+ (instancetype _Nullable)noConnectionAlert
{
  kStatisticsEvent = @"No Connection Alert";
  MWMDefaultAlert * alert = [self defaultAlertWithTitle:@"common_check_internet_connection_dialog"
                                                message:nil
                                       rightButtonTitle:@"ok"
                                        leftButtonTitle:nil
                                      rightButtonAction:nil];
  [alert setNeedsCloseAlertAfterEnterBackground];
  return alert;
}

+ (instancetype _Nullable)migrationProhibitedAlert
{
  kStatisticsEvent = @"Migration Prohibited Alert";
  MWMDefaultAlert * alert = [self defaultAlertWithTitle:@"no_migration_during_navigation"
                                                message:nil
                                       rightButtonTitle:@"ok"
                                        leftButtonTitle:nil
                                      rightButtonAction:nil];
  [alert setNeedsCloseAlertAfterEnterBackground];
  return alert;
}

+ (instancetype _Nullable)unsavedEditsAlertWithOkBlock:(TMWMVoidBlock)okBlock
{
  kStatisticsEvent = @"Editor unsaved changes on delete";
  return [self defaultAlertWithTitle:@"please_note"
                             message:@"downloader_delete_map_dialog"
                    rightButtonTitle:@"delete"
                     leftButtonTitle:@"cancel"
                   rightButtonAction:okBlock];
}

+ (instancetype _Nullable)noWiFiAlertWithName:(NSString *)name okBlock:(TMWMVoidBlock)okBlock
{
  kStatisticsEvent = @"No WiFi Alert";
  NSString * title = [NSString stringWithFormat:L(@"common_no_wifi_dialog"), name];
  MWMDefaultAlert * alert = [self defaultAlertWithTitle:title
                                                message:nil
                                       rightButtonTitle:@"use_cellular_data"
                                        leftButtonTitle:@"cancel"
                                      rightButtonAction:okBlock];
  [alert setNeedsCloseAlertAfterEnterBackground];
  return alert;
}

+ (instancetype _Nullable)endPointNotFoundAlert
{
  kStatisticsEvent = @"End Point Not Found Alert";
  NSString * message =
      [NSString stringWithFormat:@"%@\n\n%@", L(@"dialog_routing_end_not_determined"),
                                 L(@"dialog_routing_select_closer_end")];
  return [self defaultAlertWithTitle:@"dialog_routing_change_end"
                             message:message
                    rightButtonTitle:@"ok"
                     leftButtonTitle:nil
                   rightButtonAction:nil];
}

+ (instancetype _Nullable)startPointNotFoundAlert
{
  kStatisticsEvent = @"Start Point Not Found Alert";
  NSString * message =
      [NSString stringWithFormat:@"%@\n\n%@", L(@"dialog_routing_start_not_determined"),
                                 L(@"dialog_routing_select_closer_start")];
  return [self defaultAlertWithTitle:@"dialog_routing_change_start"
                             message:message
                    rightButtonTitle:@"ok"
                     leftButtonTitle:nil
                   rightButtonAction:nil];
}

+ (instancetype _Nullable)internalRoutingErrorAlert
{
  kStatisticsEvent = @"Internal Routing Error Alert";
  NSString * message =
      [NSString stringWithFormat:@"%@\n\n%@", L(@"dialog_routing_application_error"),
                                 L(@"dialog_routing_try_again")];
  return [self defaultAlertWithTitle:@"dialog_routing_system_error"
                             message:message
                    rightButtonTitle:@"ok"
                     leftButtonTitle:nil
                   rightButtonAction:nil];
}

+ (instancetype _Nullable)incorrectFeauturePositionAlert
{
  kStatisticsEvent = @"Incorrect Feature Possition Alert";
  return [self defaultAlertWithTitle:@"dialog_incorrect_feature_position"
                             message:@"message_invalid_feature_position"
                    rightButtonTitle:@"ok"
                     leftButtonTitle:nil
                   rightButtonAction:nil];
}

+ (instancetype _Nullable)internalErrorAlert
{
  kStatisticsEvent = @"Internal Error Alert";
  return [self defaultAlertWithTitle:@"dialog_routing_system_error"
                             message:nil
                    rightButtonTitle:@"ok"
                     leftButtonTitle:nil
                   rightButtonAction:nil];
}

+ (instancetype _Nullable)invalidUserNameOrPasswordAlert
{
  kStatisticsEvent = @"Invalid User Name or Password Alert";
  return [self defaultAlertWithTitle:@"invalid_username_or_password"
                             message:nil
                    rightButtonTitle:@"ok"
                     leftButtonTitle:nil
                   rightButtonAction:nil];
}

+ (instancetype _Nullable)noCurrentPositionAlert
{
  kStatisticsEvent = @"No Current Position Alert";
  NSString * message =
      [NSString stringWithFormat:@"%@\n\n%@", L(@"common_current_location_unknown_dialog"),
                                 L(@"dialog_routing_location_turn_wifi")];
  return [self defaultAlertWithTitle:@"dialog_routing_check_gps"
                             message:message
                    rightButtonTitle:@"ok"
                     leftButtonTitle:nil
                   rightButtonAction:nil];
}

+ (instancetype _Nullable)disabledLocationAlert
{
  kStatisticsEvent = @"Disabled Location Alert";
  TMWMVoidBlock action = ^{
    GetFramework().SwitchMyPositionNextMode();
  };
  return [MWMDefaultAlert defaultAlertWithTitle:@"dialog_routing_location_turn_on"
                                        message:@"dialog_routing_location_unknown_turn_on"
                               rightButtonTitle:@"turn_on"
                                leftButtonTitle:@"later"
                              rightButtonAction:action];
}

+ (instancetype _Nullable)pointsInDifferentMWMAlert
{
  kStatisticsEvent = @"Points In Different MWM Alert";
  return [self defaultAlertWithTitle:@"routing_failed_cross_mwm_building"
                             message:nil
                    rightButtonTitle:@"ok"
                     leftButtonTitle:nil
                   rightButtonAction:nil];
}

+ (instancetype _Nullable)point2PointAlertWithOkBlock:(TMWMVoidBlock)okBlock needToRebuild:(BOOL)needToRebuild
{
  if (needToRebuild)
  {
    return [self defaultAlertWithTitle:@"p2p_only_from_current"
                               message:@"p2p_reroute_from_current"
                      rightButtonTitle:@"ok"
                       leftButtonTitle:@"cancel"
                     rightButtonAction:okBlock];
  }
  else
  {
    return [self defaultAlertWithTitle:@"p2p_only_from_current"
                               message:nil
                      rightButtonTitle:@"ok"
                       leftButtonTitle:nil
                     rightButtonAction:nil];
  }
}

+ (instancetype _Nullable)disableAutoDownloadAlertWithOkBlock:(TMWMVoidBlock)okBlock
{
  kStatisticsEvent = @"Disable Auto Download Alert";
  MWMDefaultAlert * alert = [self defaultAlertWithTitle:@"disable_auto_download"
                                                message:nil
                                       rightButtonTitle:@"_disable"
                                        leftButtonTitle:@"cancel"
                                      rightButtonAction:okBlock];
  [alert setNeedsCloseAlertAfterEnterBackground];
  return alert;
}

+ (instancetype _Nullable)downloaderNoConnectionAlertWithOkBlock:(TMWMVoidBlock)okBlock cancelBlock:(TMWMVoidBlock)cancelBlock
{
  kStatisticsEvent = @"Downloader No Connection Alert";
  MWMDefaultAlert * alert = [self defaultAlertWithTitle:@"downloader_status_failed"
                                                message:@"common_check_internet_connection_dialog"
                                       rightButtonTitle:@"downloader_retry"
                                        leftButtonTitle:@"cancel"
                                      rightButtonAction:okBlock];
  alert.leftButtonAction = cancelBlock;
  [alert setNeedsCloseAlertAfterEnterBackground];
  return alert;
}

+ (instancetype _Nullable)downloaderNotEnoughSpaceAlert
{
  kStatisticsEvent = @"Downloader Not Enough Space Alert";
  MWMDefaultAlert * alert = [self defaultAlertWithTitle:@"downloader_no_space_title"
                                                message:@"downloader_no_space_message"
                                       rightButtonTitle:@"close"
                                        leftButtonTitle:nil
                                      rightButtonAction:nil];
  [alert setNeedsCloseAlertAfterEnterBackground];
  return alert;
}

+ (instancetype _Nullable)downloaderInternalErrorAlertWithOkBlock:(TMWMVoidBlock)okBlock cancelBlock:(TMWMVoidBlock)cancelBlock
{
  kStatisticsEvent = @"Downloader Internal Error Alert";
  MWMDefaultAlert * alert = [self defaultAlertWithTitle:@"migration_download_error_dialog"
                                                message:nil
                                       rightButtonTitle:@"downloader_retry"
                                        leftButtonTitle:@"cancel"
                                      rightButtonAction:okBlock];
  alert.leftButtonAction = cancelBlock;
  [alert setNeedsCloseAlertAfterEnterBackground];
  return alert;
}

+ (instancetype _Nullable)downloaderNeedUpdateAlertWithOkBlock:(TMWMVoidBlock)okBlock
{
  kStatisticsEvent = @"Downloader Need Update Alert";
  MWMDefaultAlert * alert = [self defaultAlertWithTitle:@"downloader_need_update_title"
                                                message:@"downloader_need_update_message"
                                       rightButtonTitle:@"downloader_status_outdated"
                                        leftButtonTitle:@"not_now"
                                      rightButtonAction:okBlock];
  [alert setNeedsCloseAlertAfterEnterBackground];
  return alert;
}

+ (instancetype _Nullable)routingMigrationAlertWithOkBlock:(TMWMVoidBlock)okBlock
{
  kStatisticsEvent = @"Routing Need Migration Alert";
  MWMDefaultAlert * alert = [self defaultAlertWithTitle:@"downloader_update_maps"
                                                message:@"downloader_mwm_migration_dialog"
                                       rightButtonTitle:@"ok"
                                        leftButtonTitle:@"cancel"
                                      rightButtonAction:okBlock];
  return alert;
}

+ (instancetype _Nullable)defaultAlertWithTitle:(NSString * _Nonnull)title
                              message:(NSString * _Nullable)message
                     rightButtonTitle:(NSString * _Nonnull)rightButtonTitle
                      leftButtonTitle:(NSString * _Nullable)leftButtonTitle
                    rightButtonAction:(TMWMVoidBlock _Nullable)action
{
  [Statistics logEvent:kStatisticsEvent withParameters:@{kStatAction : kStatOpen}];
  MWMDefaultAlert * alert = [
      [[NSBundle mainBundle] loadNibNamed:kDefaultAlertNibName owner:self options:nil] firstObject];
  alert.titleLabel.localizedText = title;
  alert.messageLabel.localizedText = message;
  if (!message)
  {
    alert.dividerTop.constant = kDividerTopConstant;
    [alert layoutIfNeeded];
  }
  alert.rightButton.localizedText = rightButtonTitle;
  alert.rightButtonAction = action;
  if (leftButtonTitle)
  {
    alert.leftButton.localizedText = leftButtonTitle;
  }
  else
  {
    alert.leftButton.hidden = YES;
    alert.rightButtonWidth.constant = [alert.subviews.firstObject width];
  }
  return alert;
}

#pragma mark - Actions

- (IBAction)rightButtonTap
{
  [Statistics logEvent:kStatisticsEvent withParameters:@{kStatAction : kStatApply}];
  if (self.rightButtonAction)
    self.rightButtonAction();
  [self close];
}

- (IBAction)leftButtonTap
{
  [Statistics logEvent:kStatisticsEvent withParameters:@{kStatAction : kStatClose}];
  if (self.leftButtonAction)
    self.leftButtonAction();
  [self close];
}

@end
