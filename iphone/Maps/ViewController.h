@interface ViewController : UIViewController

@property (nonatomic, readonly) BOOL hasNavigationBar;

- (void)mwm_refreshUI;

@property (nonatomic, readonly) BOOL hasNavigationBar;

- (void)showAlert:(NSString *)alert withButtonTitle:(NSString *)buttonTitle;

@end
