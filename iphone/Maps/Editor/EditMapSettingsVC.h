#import "ViewController.h"

@interface EditMapSettingsVC : ViewController
{
}

@property (nonatomic, weak) IBOutlet UIWebView *webView;
@property (nonatomic, weak) IBOutlet UITextField *loginTextField;
@property (nonatomic, weak) IBOutlet UITextField *passwordTextField;

-(IBAction)onSaveButtonClicked;
-(IBAction)onRegisterButtonClicked;

@end
