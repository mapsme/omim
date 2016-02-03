@interface ViewController : UIViewController

- (void)mwm_refreshUI;

@property (nonatomic, readonly) BOOL hasNavigationBar;

- (void)refresh;

- (void)showAlert:(NSString *)alert withButtonTitle:(NSString *)buttonTitle;

@end
