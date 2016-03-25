#import "MWMPageController.h"
#import "MWMWhatsNewNightModeController.h"

@interface MWMWhatsNewNightModeController ()

@property (weak, nonatomic) IBOutlet UIView * _Nullable containerView;
@property (weak, nonatomic) IBOutlet UIImageView * _Nullable image;
@property (weak, nonatomic) IBOutlet UILabel * _Nullable alertTitle;
@property (weak, nonatomic) IBOutlet UILabel * _Nullable alertText;
@property (weak, nonatomic) IBOutlet UIButton * _Nullable nextPageButton;
@property (weak, nonatomic) IBOutlet NSLayoutConstraint * _Nullable containerWidth;
@property (weak, nonatomic) IBOutlet NSLayoutConstraint * _Nullable containerHeight;

@property (weak, nonatomic) IBOutlet NSLayoutConstraint * _Nullable imageMinHeight;
@property (weak, nonatomic) IBOutlet NSLayoutConstraint * _Nullable imageHeight;

@property (weak, nonatomic) IBOutlet NSLayoutConstraint * _Nullable titleTopOffset;
@property (weak, nonatomic) IBOutlet NSLayoutConstraint * _Nullable titleImageOffset;

@end

namespace
{
NSArray<TMWMWelcomeConfigBlock> * pagesConfigBlocks = @[
  [^(MWMWhatsNewNightModeController * controller)
  {
    controller.image.image = [UIImage imageNamed:@"img_nightmode"];
    controller.alertTitle.text = L(@"whats_new_night_caption");
    controller.alertText.text = L(@"whats_new_night_body");
    [controller.nextPageButton setTitle:L(@"done") forState:UIControlStateNormal];
    [controller.nextPageButton addTarget:controller.pageController
                                  action:@selector(close)
                        forControlEvents:UIControlEventTouchUpInside];
  } copy]
];
} // namespace

@implementation MWMWhatsNewNightModeController

+ (NSString *)udWelcomeWasShownKey
{
  return @"WhatsNewWithNightModeWasShown";
}

+ (NSArray<TMWMWelcomeConfigBlock> *)pagesConfig
{
  return pagesConfigBlocks;
}

#pragma mark - Properties

- (void)setSize:(CGSize)size
{
  super.size = size;
  CGSize const newSize = super.size;
  CGFloat const width = newSize.width;
  CGFloat const height = newSize.height;
  BOOL const hideImage = (self.imageHeight.multiplier * height <= self.imageMinHeight.constant);
  self.titleImageOffset.priority = hideImage ? UILayoutPriorityDefaultLow : UILayoutPriorityDefaultHigh;
  self.image.hidden = hideImage;
  self.containerWidth.constant = width;
  self.containerHeight.constant = height;
}

@end
