#import "MWMRouteHelperPanel.h"

@interface MWMNextTurnPanel : MWMRouteHelperPanel

+ (instancetype _Nullable)turnPanelWithOwnerView:(UIView *)ownerView;
- (void)configureWithImage:(UIImage *)image;

@end
