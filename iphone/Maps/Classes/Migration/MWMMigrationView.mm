#import "Common.h"
#import "MWMCircularProgress.h"
#import "MWMMigrationView.h"
#import "UIColor+MapsMeColor.h"

@interface MWMMigrationView ()

@property (weak, nonatomic) IBOutlet UIImageView * image;
@property (weak, nonatomic) IBOutlet NSLayoutConstraint * imageMinHeight;
@property (weak, nonatomic) IBOutlet NSLayoutConstraint * imageHeight;

@property (weak, nonatomic) IBOutlet UILabel * title;
@property (weak, nonatomic) IBOutlet NSLayoutConstraint * titleTopOffset;
@property (weak, nonatomic) IBOutlet NSLayoutConstraint * titleImageOffset;

@property (weak, nonatomic) IBOutlet UILabel * text;
@property (weak, nonatomic) IBOutlet UILabel * info;

@property (weak, nonatomic) IBOutlet UIButton * primaryButton;
@property (weak, nonatomic) IBOutlet NSLayoutConstraint * primaryButtonBotomOffset;
@property (weak, nonatomic) IBOutlet UIButton * secondaryButton;
@property (weak, nonatomic) IBOutlet UIView * spinnerView;

@property (nonatomic) MWMCircularProgress * spinner;

@end

@implementation MWMMigrationView

- (void)awakeFromNib
{
  [super awakeFromNib];
  self.state = MWMMigrationViewState::Default;
  self.secondaryButton.titleLabel.textAlignment = NSTextAlignmentCenter;
}

- (void)layoutSubviews
{
  [super layoutSubviews];
  self.title.preferredMaxLayoutWidth = self.title.width;
  self.text.preferredMaxLayoutWidth = self.text.width;
  self.info.preferredMaxLayoutWidth = self.info.width;
  [super layoutSubviews];
}

- (void)setFrame:(CGRect)frame
{
  if (!isIOS7)
    [self updateForSize:frame.size];
  super.frame = frame;
}

- (void)updateForSize:(CGSize)size
{
  BOOL const hideImage = (self.imageHeight.multiplier * size.height <= self.imageMinHeight.constant);
  self.titleImageOffset.priority = hideImage ? UILayoutPriorityDefaultLow : UILayoutPriorityDefaultHigh;
  self.image.hidden = hideImage;
}

- (void)configDefaultState
{
  [self stopSpinner];
  self.info.hidden = YES;
  self.info.textColor = [UIColor blackHintText];
  self.primaryButton.enabled = YES;
  self.primaryButtonBotomOffset.priority = UILayoutPriorityDefaultLow;
  self.secondaryButton.hidden = NO;
  self.secondaryButton.enabled = YES;
}

- (void)startSpinner
{
  self.spinnerView.hidden = NO;
  self.spinner = [[MWMCircularProgress alloc] initWithParentView:self.spinnerView];
  [self.spinner startSpinner:YES];
}

- (void)stopSpinner
{
  self.spinnerView.hidden = YES;
  [self.spinnerView.subviews makeObjectsPerformSelector:@selector(removeFromSuperview)];
  [self.spinner stopSpinner];
  self.spinner = nil;
}

#pragma mark - iOS 7 support methods

- (void)willRotateToInterfaceOrientation:(UIInterfaceOrientation)orientation
{
  UIView * superview = self.superview ? self.superview : UIApplication.sharedApplication.keyWindow;
  BOOL const isLandscape = UIInterfaceOrientationIsLandscape(orientation);
  CGFloat const minDim = MIN(superview.width, superview.height);
  CGFloat const maxDim = MAX(superview.width, superview.height);
  CGFloat const height = isLandscape ? minDim : maxDim;
  CGFloat const width = isLandscape ? maxDim : minDim;
  self.bounds = {{}, {width, height}};
}

- (void)setBounds:(CGRect)bounds
{
  if (isIOS7)
    [self updateForSize:bounds.size];
  super.bounds = bounds;
}

#pragma mark - Properties

- (void)setState:(MWMMigrationViewState)state
{
  [self layoutIfNeeded];
  [self configDefaultState];
  switch (state)
  {
    case MWMMigrationViewState::Default:
      break;
    case MWMMigrationViewState::Processing:
      self.info.hidden = NO;
      self.info.text = L(@"migration_preparing_update");
      [self startSpinner];
      self.primaryButton.enabled = NO;
      self.primaryButtonBotomOffset.priority = UILayoutPriorityDefaultHigh;
      self.secondaryButton.hidden = YES;
      break;
    case MWMMigrationViewState::ErrorNoConnection:
      self.info.hidden = NO;
      self.info.textColor = [UIColor red];
      self.info.text = L(@"migration_no_connection");
      self.primaryButton.enabled = NO;
      self.secondaryButton.enabled = NO;
      break;
    case MWMMigrationViewState::ErrorNoSpace:
      self.info.hidden = NO;
      self.info.textColor = [UIColor red];
      self.info.text = L(@"migration_no_space");
      self.primaryButton.enabled = NO;
      self.secondaryButton.enabled = NO;
      break;
  }
  _state = state;
  [UIView animateWithDuration:kDefaultAnimationDuration animations:^{ [self layoutIfNeeded]; }];
}

@end
