#import "MWMActivityViewController.h"
#import "MWMEditorViralActivityItem.h"
#import "MWMShareActivityItem.h"

@interface MWMActivityViewController ()

@property (weak, nonatomic) UIViewController * ownerViewController;
@property (weak, nonatomic) UIView * anchorView;

@end

@implementation MWMActivityViewController

- (instancetype)initWithActivityItem:(id<UIActivityItemSource>)activityItem
{
  return [self initWithActivityItems:@[activityItem]];
}

- (instancetype)initWithActivityItems:(NSArray *)activityItems
{
  self = [super initWithActivityItems:activityItems applicationActivities:nil];
  if (self)
    self.excludedActivityTypes = @[UIActivityTypePrint, UIActivityTypeAssignToContact, UIActivityTypeSaveToCameraRoll,
                                 UIActivityTypeAirDrop, UIActivityTypeAddToReadingList, UIActivityTypePostToFlickr,
                                 UIActivityTypePostToVimeo];
  return self;
}

+ (instancetype)shareControllerForMyPosition:(CLLocationCoordinate2D const &)location
{
  MWMShareActivityItem * item = [[MWMShareActivityItem alloc] initForMyPositionAtLocation:location];
  return [[self alloc] initWithActivityItem:item];
}

+ (instancetype)shareControllerForPlacePageObject:(id<MWMPlacePageObject>)object;
{
  MWMShareActivityItem * item = [[MWMShareActivityItem alloc] initForPlacePageObject:object];
  return [[self alloc] initWithActivityItem:item];
}

+ (instancetype)shareControllerForEditorViral
{
  MWMEditorViralActivityItem * item = [[MWMEditorViralActivityItem alloc] init];
  UIImage * image = [UIImage imageNamed:@"img_sharing_editor"];
  MWMActivityViewController * vc = [[self alloc] initWithActivityItems:@[item, image]];
  if ([vc respondsToSelector:@selector(popoverPresentationController)])
    vc.popoverPresentationController.permittedArrowDirections = UIPopoverArrowDirectionDown;
  return vc;
}

- (void)presentInParentViewController:(UIViewController *)parentVC anchorView:(UIView *)anchorView
{
  self.ownerViewController = parentVC;
  self.anchorView = anchorView;
  if ([self respondsToSelector:@selector(popoverPresentationController)])
  {
    self.popoverPresentationController.sourceView = anchorView;
    self.popoverPresentationController.sourceRect = anchorView.bounds;
  }
  [parentVC presentViewController:self animated:YES completion:nil];
}

@end
