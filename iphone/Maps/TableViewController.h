@interface TableViewController : UITableViewController

- (void)mwm_refreshUI;

@property (nonatomic, readonly) BOOL hasNavigationBar;

- (void)refresh;

@end
