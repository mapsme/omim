#import "MWMAuthorizationCommon.h"
#import "MWMAuthorizationOSMLoginViewController.h"
#import "MWMCircularProgress.h"
#import "UIColor+MapsMeColor.h"
#import "UITextField+RuntimeAttributes.h"

#include "private.h"
#include "editor/server_api.hpp"
#include "platform/platform.hpp"

typedef NS_OPTIONS(NSUInteger, MWMFieldCorrect)
{
  MWMFieldCorrectNO       = 0,
  MWMFieldCorrectLogin    = 1 << 0,
  MWMFieldCorrectPassword = 1 << 1,
  MWMFieldCorrectAll = MWMFieldCorrectLogin | MWMFieldCorrectPassword
};

using namespace osm;

@interface MWMAuthorizationOSMLoginViewController () <UITextFieldDelegate>

@property (weak, nonatomic) IBOutlet UITextField * loginTextField;
@property (weak, nonatomic) IBOutlet UITextField * passwordTextField;
@property (weak, nonatomic) IBOutlet UIButton * loginButton;
@property (weak, nonatomic) IBOutlet UIButton * forgotButton;
@property (weak, nonatomic) IBOutlet UIView * spinnerView;

@property (nonatomic) MWMFieldCorrect isCorrect;

@property (nonatomic) MWMCircularProgress * spinner;

@end

@implementation MWMAuthorizationOSMLoginViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
  self.title = L(@"osm_login");
  self.isCorrect = MWMFieldCorrectNO;
  [self checkConnection];
  [self stopSpinner];
}

- (BOOL)shouldAutorotate
{
  return NO;
}

- (void)checkConnection
{
  self.forgotButton.enabled = Platform::IsConnected();
}

- (void)setLoginButtonEnabled:(BOOL)enabled
{
  self.loginButton.enabled = enabled;
  CALayer * layer = self.loginButton.layer;
  layer.borderColor = (enabled ? [UIColor buttonEnabledBlueText] : [UIColor clearColor]).CGColor;
}

#pragma mark - UITextFieldDelegate

- (BOOL)textField:(UITextField *)textField shouldChangeCharactersInRange:(NSRange)range replacementString:(NSString *)string
{
  NSString * newString =
      [textField.text stringByReplacingCharactersInRange:range withString:string];
  BOOL const isValid = [textField.validator validateString:newString];

  if ([textField isEqual:self.loginTextField])
  {
    if (isValid)
      self.isCorrect |= MWMFieldCorrectLogin;
    else
      self.isCorrect &= ~MWMFieldCorrectLogin;
  }
  else if ([textField isEqual:self.passwordTextField])
  {
    if (isValid)
      self.isCorrect |= MWMFieldCorrectPassword;
    else
      self.isCorrect &= ~MWMFieldCorrectPassword;
  }

  return YES;
}

- (BOOL)textFieldShouldReturn:(UITextField *)textField
{
  if ([textField isEqual:self.loginTextField])
  {
    [self.passwordTextField becomeFirstResponder];
  }
  else if ([textField isEqual:self.passwordTextField])
  {
    [textField resignFirstResponder];
    [self login];
  }
  return YES;
}

- (void)startSpinner
{
  self.spinnerView.hidden = NO;
  self.spinner = [[MWMCircularProgress alloc] initWithParentView:self.spinnerView];
  [self.spinner startSpinner];
  self.loginTextField.enabled = NO;
  self.passwordTextField.enabled = NO;
  self.forgotButton.enabled = NO;
  [self.loginButton setTitle:@"" forState:UIControlStateNormal];
}

- (void)stopSpinner
{
  self.spinnerView.hidden = YES;
  [self.spinnerView.subviews makeObjectsPerformSelector:@selector(removeFromSuperview)];
  [self.spinner stopSpinner];
  self.spinner = nil;
  self.loginTextField.enabled = YES;
  self.passwordTextField.enabled = YES;
  self.forgotButton.enabled = YES;
  [self.loginButton setTitle:L(@"login") forState:UIControlStateNormal];
}

#pragma mark - Actions

- (IBAction)login
{
  if (!self.loginButton.enabled || self.spinner)
    return;
  if (Platform::IsConnected())
  {
    [self startSpinner];
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0), ^
    {
      string const username = self.loginTextField.text.UTF8String;
      string const password = self.passwordTextField.text.UTF8String;
      // TODO(AlexZ): Change to production.
      OsmOAuth auth = OsmOAuth::IZServerAuth();
      OsmOAuth::AuthResult const result = auth.AuthorizePassword(username, password);
      dispatch_async(dispatch_get_main_queue(), ^
      {
        [self stopSpinner];
        if (result == OsmOAuth::AuthResult::OK)
        {
          MWMAuthorizationStoreCredentials(auth.GetToken());
          [self dismissViewControllerAnimated:YES completion:nil];
        }
        else
        {
          [self showAlert:L(@"invalid_username_or_password") withButtonTitle:L(@"ok")];
        }
      });
    });
  }
  else
  {
    [self showAlert:L(@"no_internet_connection") withButtonTitle:L(@"ok")];
  }
}

- (IBAction)cancel
{
  [self.navigationController popViewControllerAnimated:YES];
}

#pragma mark - Properties

- (void)setIsCorrect:(MWMFieldCorrect)isCorrect
{
  _isCorrect = isCorrect;
  [self setLoginButtonEnabled:isCorrect == MWMFieldCorrectAll];
}

@end
