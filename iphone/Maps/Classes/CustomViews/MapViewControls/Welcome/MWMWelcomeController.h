#import "MWMViewController.h"

@class MWMPageController;

@protocol MWMWelcomeControllerProtocol <NSObject>

+ (UIStoryboard *)welcomeStoryboard;
+ (instancetype _Nullable)welcomeController;
+ (NSInteger)pagesCount;
+ (NSString *)udWelcomeWasShownKey;

@end

@interface MWMWelcomeController : MWMViewController <MWMWelcomeControllerProtocol>

@property (nonatomic) NSInteger pageIndex;
@property (weak, nonatomic) MWMPageController * _Nullable pageController;

@property (nonatomic) CGSize size;

@end

using TMWMWelcomeConfigBlock = void (^)(MWMWelcomeController * controller);