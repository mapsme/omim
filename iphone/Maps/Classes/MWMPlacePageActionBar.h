#import <UIKit/UIKit.h>

@class MWMPlacePage;

@interface MWMPlacePageActionBar : SolidTouchView

@property (nonatomic) BOOL isBookmark;
@property (nonatomic) BOOL isPrepareRouteMode;

@property (weak, nonatomic) IBOutlet UIButton * _Nullable shareButton;

+ (MWMPlacePageActionBar *)actionBarForPlacePage:(MWMPlacePage *)placePage;
- (void)configureWithPlacePage:(MWMPlacePage *)placePage;

- (instancetype _Nullable)init __attribute__((unavailable("call actionBarForPlacePage: instead")));
- (instancetype _Nullable)initWithCoder:(NSCoder *)aDecoder __attribute__((unavailable("call actionBarForPlacePage: instead")));
- (instancetype _Nullable)initWithFrame:(CGRect)frame __attribute__((unavailable("call actionBarForPlacePage: instead")));

@end
