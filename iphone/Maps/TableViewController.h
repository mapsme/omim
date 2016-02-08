@interface TableViewController : UITableViewController

@property (nonatomic, readonly) BOOL hasNavigationBar;

- (void)mwm_refreshUI;

@end
