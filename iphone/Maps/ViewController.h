@interface ViewController : UIViewController

@property (nonatomic, readonly) BOOL hasNavigationBar;

- (void)mwm_refreshUI;
- (void)showAlert:(NSString *)alert withButtonTitle:(NSString *)buttonTitle;

@end
