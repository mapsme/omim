#import "EditMapSettingsVC.h"
#import "UIKitCategories.h"

#include "../../../3party/Alohalytics/src/http_client.h"

extern std::string const OSM_SERVER_URL;
NSString * kOSMLoginKey = @"OSMLoginKey";
NSString * kOSMPasswordKey = @"OSMPasswordKey";

@interface EditMapSettingsVC()
{
}
@end

@implementation EditMapSettingsVC

- (void)viewDidLoad
{
  [super viewDidLoad];
  self.title = L(@"edit_map");
  _webView.backgroundColor = [UIColor clearColor];
  [_webView loadHTMLString:@"<a href='http://www.openstreetmap.org'>OpenStreetMap</a> is a free, editable map of the whole world that is being built by volunteers and released with an open-content license." baseURL:nil];

  NSUserDefaults * settings = [NSUserDefaults standardUserDefaults];
  _loginTextField.text = [settings objectForKey:kOSMLoginKey];
  _passwordTextField.text = [settings objectForKey:kOSMPasswordKey];
  [_loginTextField becomeFirstResponder];
}

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation
{
  return YES;
}

-(IBAction)onSaveButtonClicked
{
  // Special case to clean up login and password in the settings.
  NSString * message;
  NSUserDefaults * settings = [NSUserDefaults standardUserDefaults];
  if (_loginTextField.text.length == 0)
  {
    [settings removeObjectForKey:kOSMLoginKey];
    [settings removeObjectForKey:kOSMPasswordKey];
    [settings synchronize];
    message = @"Cleaned up stored login and password.";
  }
  else
  {
    // Make simple api request to check if authentification works.
    alohalytics::HTTPClientPlatformWrapper request(OSM_SERVER_URL + "/api/0.6/permissions");
    request.set_user_and_password([_loginTextField.text UTF8String], [_passwordTextField.text UTF8String]);
    message = @"Can't connect to OpenStreetMap server.";
    if (request.RunHTTPRequest())
    {
      if (200 != request.error_code())
        message = [NSString stringWithUTF8String:request.server_response().c_str()];
      else if (std::string::npos == request.server_response().find("allow_write_api"))
        message = @"Please re-check your account details as they are not accepted by the server.";
      else
      {
        message = @"Your login is accepted by OpenStreetMap server.";
        [settings setObject:_loginTextField.text forKey:kOSMLoginKey];
        [settings setObject:_passwordTextField.text forKey:kOSMPasswordKey];
        [settings synchronize];
      }
    }
  }
  [self.webView loadHTMLString:message baseURL:nil];
}

-(IBAction)onRegisterButtonClicked
{
  [[UIApplication sharedApplication] openURL:[NSURL URLWithString:@"https://www.openstreetmap.org/user/new"]];
}

@end
