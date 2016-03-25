#import "MWMViewController.h"

@class MWMPlacePageViewManager;

@interface MWMBookmarkDescriptionViewController : MWMViewController

- (instancetype _Nullable)initWithPlacePageManager:(MWMPlacePageViewManager *)manager;

@property (weak, nonatomic) UINavigationController * _Nullable iPadOwnerNavigationController;

@end
