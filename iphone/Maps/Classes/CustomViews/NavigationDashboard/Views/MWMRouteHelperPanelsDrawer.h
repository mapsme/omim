#import "MWMRouteHelperPanel.h"

@interface MWMRouteHelperPanelsDrawer : NSObject

@property (weak, nonatomic, readonly) UIView * _Nullable topView;

- (instancetype _Nullable)initWithTopView:(UIView *)view;
- (void)invalidateTopBounds:(NSArray *)panels topView:(UIView *)view;

@end
