#import "Common.h"
#import "MWMMapDownloaderTableViewHeader.h"

@interface MWMMapDownloaderTableViewHeader ()

@property (weak, nonatomic) IBOutlet UIButton * button;
@property (weak, nonatomic) IBOutlet UIView * separatorDown;
@property (weak, nonatomic) IBOutlet UIImageView * arrow;

@end

@implementation MWMMapDownloaderTableViewHeader

+ (CGFloat)height
{
  return 28.0;
}

- (IBAction)buttonPressed
{
  self.expanded = !self.expanded;
  [self.delegate expandButtonPressed:self];
}

#pragma mark - Properties

- (void)setExpanded:(BOOL)expanded
{
  _expanded = expanded;
  self.separatorDown.hidden = !self.lastSection && !expanded;
  [UIView animateWithDuration:kDefaultAnimationDuration animations:^
  {
    self.arrow.transform = expanded ? CGAffineTransformMakeRotation(M_PI) : CGAffineTransformIdentity;
  }];
}

@end
