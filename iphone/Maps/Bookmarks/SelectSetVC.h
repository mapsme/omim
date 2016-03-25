#import "MWMTableViewController.h"

@class MWMPlacePageViewManager;

@interface SelectSetVC : MWMTableViewController

- (instancetype _Nullable)initWithPlacePageManager:(MWMPlacePageViewManager *)manager;

@property (weak, nonatomic) UINavigationController * _Nullable iPadOwnerNavigationController;

@end
