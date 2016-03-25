#import "Common.h"
#import "MWMDropDown.h"

namespace
{

CGFloat const kLeadingOffset = 16.;
CGFloat const kBottomOffset = 3.;
CGFloat const kTopOffset = 25.;

} // namespace

#pragma mark - _MWMDropDownView (private)

@interface _MWMDropDownView : UIView

@property (weak, nonatomic) IBOutlet UILabel * _Nullable message;

@end

@implementation _MWMDropDownView

- (void)layoutSubviews
{
  CGFloat const superviewWidth = self.superview.width;
  self.message.width = superviewWidth - 2 * kLeadingOffset;
  [self.message sizeToFit];
  self.message.midX = self.superview.midX;
  self.size = {superviewWidth, kTopOffset + kBottomOffset + self.message.height};
  [super layoutSubviews];
}

@end

#pragma mark - MWMDropDown implementation

@interface MWMDropDown ()

@property (nonatomic) IBOutlet _MWMDropDownView * dropDown;
@property (weak, nonatomic) UIView * _Nullable superview;

@end

@implementation MWMDropDown

#pragma mark - Public

- (instancetype _Nullable)initWithSuperview:(UIView *)view
{
  self = [super init];
  if (self)
  {
    [[NSBundle mainBundle] loadNibNamed:[MWMDropDown className] owner:self options:nil];
    self.superview = view;
  }
  return self;
}

- (void)showWithMessage:(NSString *)message
{
  [self.dropDown removeFromSuperview];
  self.dropDown.message.text = message;
  self.dropDown.alpha = 0.;
  [self.dropDown setNeedsLayout];
  [self.superview addSubview:self.dropDown];
  self.dropDown.origin = {0., -self.dropDown.height};
  [self display];
}

- (void)dismiss
{
  [UIView animateWithDuration:kDefaultAnimationDuration animations:^
  {
    self.dropDown.alpha = 0.;
    self.dropDown.origin = {0., -self.dropDown.height};
  }
  completion:^(BOOL finished)
  {
    [self.dropDown removeFromSuperview];
  }];
}

#pragma mark - Private

- (void)display
{
  [UIView animateWithDuration:kDefaultAnimationDuration animations:^
  {
    self.dropDown.alpha = 1.;
    self.dropDown.origin = {};
  }];
}

@end
