#import "MWMPageController.h"
#import "MWMWhatsNewDownloaderEditorController.h"

@interface MWMWhatsNewDownloaderEditorController ()

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
  [^(MWMWhatsNewDownloaderEditorController * controller) {
    controller.image.image = [UIImage imageNamed:@"img_whatsnew_migration"];
    controller.alertTitle.text = L(@"whatsnew_smallmwm_header");
    controller.alertText.text = L(@"whatsnew_smallmwm_message");
    [controller.nextPageButton setTitle:L(@"whats_new_next_button") forState:UIControlStateNormal];
    [controller.nextPageButton addTarget:controller.pageController
                                  action:@selector(nextPage)
                        forControlEvents:UIControlEventTouchUpInside];
  } copy],
  [^(MWMWhatsNewDownloaderEditorController * controller) {
    controller.image.image = [UIImage imageNamed:@"img_whatsnew_editdata"];
    controller.alertTitle.text = L(@"whatsnew_editor_title");
    controller.alertText.text = [NSString stringWithFormat:@"%@\n\n%@", L(@"whatsnew_editor_message_1"), L(@"whatsnew_editor_message_2")];
    [controller.nextPageButton setTitle:L(@"whats_new_next_button") forState:UIControlStateNormal];
    [controller.nextPageButton addTarget:controller.pageController
                                  action:@selector(nextPage)
                        forControlEvents:UIControlEventTouchUpInside];
  } copy],
  [^(MWMWhatsNewDownloaderEditorController * controller) {
    controller.image.image = [UIImage imageNamed:@"img_whatsnew_update_search"];
    controller.alertTitle.text = L(@"whatsnew_search_header");
    controller.alertText.text = L(@"whatsnew_search_message");
    [controller.nextPageButton setTitle:L(@"done") forState:UIControlStateNormal];
    [controller.nextPageButton addTarget:controller.pageController
                                  action:@selector(close)
                        forControlEvents:UIControlEventTouchUpInside];
  } copy]
];
}  // namespace

@implementation MWMWhatsNewDownloaderEditorController

+ (NSString *)udWelcomeWasShownKey
{
  return @"WhatsNewWithDownloaderEditorSearchWasShown";
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
