#import "ViewController.h"

@class MWMPageController;

@interface MWMWhatsNewController : ViewController

@property (nonatomic) NSUInteger pageIndex;
@property (weak, nonatomic) MWMPageController * pageController;

- (void)updateForSize:(CGSize)size;

@end
