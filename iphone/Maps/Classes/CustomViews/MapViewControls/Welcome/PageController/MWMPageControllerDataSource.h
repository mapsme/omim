#import "MWMWelcomeController.h"

NS_CLASS_AVAILABLE_IOS(8_0) @interface MWMPageControllerDataSource : NSObject <UIPageViewControllerDataSource>

- (instancetype _Nullable)initWithPageController:(MWMPageController *)pageController welcomeClass:(Class<MWMWelcomeControllerProtocol>)welcomeClass;
- (MWMWelcomeController *)firstWelcomeController;

@end
