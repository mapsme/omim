#import "MWMTableViewController.h"

@class MWMPlacePageViewManager;

@interface MWMBookmarkColorViewController : MWMTableViewController

@property (weak, nonatomic) MWMPlacePageViewManager * _Nullable placePageManager;
@property (weak, nonatomic) UINavigationController * _Nullable iPadOwnerNavigationController;

@end
