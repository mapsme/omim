#import <UIKit/UIKit.h>

@class MWMiPhonePortraitPlacePage;

@interface MWMPlacePageNavigationBar : UIView

+ (void)dismissNavigationBar;
+ (void)showNavigationBarForPlacePage:(MWMiPhonePortraitPlacePage *)placePage;
+ (void)remove;

- (instancetype _Nullable)init __attribute__((unavailable("call navigationBarWithPlacePage: instead")));
- (instancetype _Nullable)initWithCoder:(NSCoder *)aDecoder __attribute__((unavailable("call navigationBarWithPlacePage: instead")));
- (instancetype _Nullable)initWithFrame:(CGRect)frame __attribute__((unavailable("call navigationBarWithPlacePage: instead")));

@end
