#import "MWMNoMapsView.h"
#import "Common.h"
#import "MWMKeyboard.h"

@interface MWMNoMapsView ()<MWMKeyboardObserver>

@property(weak, nonatomic) IBOutlet UIImageView * image;
@property(weak, nonatomic) IBOutlet UILabel * title;
@property(weak, nonatomic) IBOutlet UILabel * text;

@property(weak, nonatomic) IBOutlet NSLayoutConstraint * containerWidth;
@property(weak, nonatomic) IBOutlet NSLayoutConstraint * containerHeight;
@property(weak, nonatomic) IBOutlet NSLayoutConstraint * containerTopOffset;
@property(weak, nonatomic) IBOutlet NSLayoutConstraint * containerBottomOffset;
@property(weak, nonatomic) IBOutlet NSLayoutConstraint * imageMinHeight;
@property(weak, nonatomic) IBOutlet NSLayoutConstraint * imageHeight;
@property(weak, nonatomic) IBOutlet NSLayoutConstraint * titleImageOffset;
@property(weak, nonatomic) IBOutlet NSLayoutConstraint * titleTopOffset;
@property(weak, nonatomic) IBOutlet NSLayoutConstraint * textTopOffset;

@end

@implementation MWMNoMapsView

- (void)awakeFromNib
{
  [super awakeFromNib];
  if (!IPAD)
  {
    if (isIOS7)
    {
      self.containerWidth.priority = UILayoutPriorityFittingSizeLevel;
      self.containerHeight.priority = UILayoutPriorityFittingSizeLevel;
    }
    else
    {
      self.containerWidth.active = NO;
      self.containerHeight.active = NO;
    }
  }
  else
  {
    if (isIOS7)
      self.containerTopOffset.priority = UILayoutPriorityFittingSizeLevel;
    else
      self.containerTopOffset.active = NO;
  }
  [MWMKeyboard addObserver:self];
}

- (void)layoutSubviews
{
  [super layoutSubviews];
  [self configForSize:self.frame.size];
}

- (void)configForSize:(CGSize)size
{
  CGSize const iPadSize = {520, 600};
  CGSize const newSize = IPAD ? iPadSize : size;
  CGFloat const width = newSize.width;
  CGFloat const height = newSize.height;
  BOOL const hideImage = (self.imageHeight.multiplier * height <= self.imageMinHeight.constant);
  if (hideImage)
  {
    self.titleImageOffset.priority = UILayoutPriorityDefaultLow;
    self.title.hidden = self.title.minY < self.titleTopOffset.constant;
    self.text.hidden = self.text.minY < self.textTopOffset.constant;
  }
  else
  {
    self.titleImageOffset.priority = UILayoutPriorityDefaultHigh;
    self.title.hidden = NO;
    self.text.hidden = NO;
  }
  self.image.hidden = hideImage;
  if (IPAD)
  {
    self.containerWidth.constant = width;
    self.containerHeight.constant = height;
  }
}

#pragma mark - MWMKeyboard

- (void)onKeyboardAnimation
{
  self.containerBottomOffset.constant = [MWMKeyboard keyboardHeight];
  [self.superview layoutIfNeeded];
}

- (void)onKeyboardWillAnimate { [self.superview layoutIfNeeded]; }
@end
