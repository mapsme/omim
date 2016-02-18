#import "Common.h"
#import "EAGLView.h"
#import "MWMBottomMenuView.h"
#import "MWMBottomMenuViewController.h"
#import "MWMButton.h"
#import "MapsAppDelegate.h"
#import "UIButton+RuntimeAttributes.h"
#import "UIColor+MapsMeColor.h"
#import "UIFont+MapsMeFonts.h"
#import "UIView+RuntimeAttributes.h"

#include "Framework.h"

@interface MWMBottomMenuView ()

@property(weak, nonatomic) IBOutlet UIView * mainButtons;
@property(weak, nonatomic) IBOutlet UIView * separator;
@property(weak, nonatomic) IBOutlet UICollectionView * additionalButtons;

@property (weak, nonatomic) IBOutlet NSLayoutConstraint * mainButtonWidth;
@property (weak, nonatomic) IBOutlet NSLayoutConstraint * separatorWidth;
@property (weak, nonatomic) IBOutlet NSLayoutConstraint * additionalButtonsWidth;
@property (weak, nonatomic) IBOutlet NSLayoutConstraint * additionalButtonsHeight;

@property(weak, nonatomic) IBOutlet UIView * downloadBadge;

@property(weak, nonatomic) IBOutlet MWMButton * locationButton;
@property(weak, nonatomic) IBOutlet MWMButton * p2pButton;
@property(weak, nonatomic) IBOutlet MWMButton * searchButton;
@property(weak, nonatomic) IBOutlet MWMButton * bookmarksButton;
@property(weak, nonatomic) IBOutlet MWMButton * menuButton;

@property(weak, nonatomic) IBOutlet UIButton * goButton;

@property(weak, nonatomic) IBOutlet UILabel * streetLabel;

@property(nonatomic) CGFloat layoutDuration;

@property (weak, nonatomic) IBOutlet MWMBottomMenuViewController * owner;

@end

@implementation MWMBottomMenuView

- (void)awakeFromNib
{
  [super awakeFromNib];
  self.additionalButtons.hidden = YES;
  self.downloadBadge.hidden = YES;
  self.goButton.hidden = YES;
  self.streetLabel.hidden = YES;
  self.restoreState = MWMBottomMenuStateInactive;
  [self.goButton setBackgroundColor:[UIColor linkBlue] forState:UIControlStateNormal];
  [self.goButton setBackgroundColor:[UIColor linkBlueDark] forState:UIControlStateHighlighted];
}

- (void)layoutSubviews
{
  [self refreshOnOrientationChange];
  if (self.layoutDuration > 0.0)
  {
    CGFloat const duration = self.layoutDuration;
    self.layoutDuration = 0.0;
    [self layoutIfNeeded];
    [UIView animateWithDuration:duration
                     animations:^
    {
      [self layoutGeometry];
      [self layoutIfNeeded];
    }];
  }
  else
  {
    [self layoutGeometry];
  }
  [UIView animateWithDuration:kDefaultAnimationDuration animations:^{ [self updateAlphaAndColor]; }
                   completion:^(BOOL finished) { [self updateVisibility]; }];
  ((EAGLView *)self.superview).widgetsManager.bottomBound = self.mainButtons.height;
  [super layoutSubviews];
}

- (void)updateAlphaAndColor
{
  switch (self.state)
  {
  case MWMBottomMenuStateHidden:
    break;
  case MWMBottomMenuStateInactive:
    self.backgroundColor = [UIColor menuBackground];
    self.bookmarksButton.alpha = 1.0;
    self.downloadBadge.alpha = 1.0;
    self.goButton.alpha = 0.0;
    self.p2pButton.alpha = 1.0;
    self.searchButton.alpha = 1.0;
    self.streetLabel.alpha = 0.0;
    break;
  case MWMBottomMenuStateActive:
    self.backgroundColor = [UIColor white];
    self.bookmarksButton.alpha = 1.0;
    self.downloadBadge.alpha = 0.0;
    self.goButton.alpha = 0.0;
    self.p2pButton.alpha = 1.0;
    self.searchButton.alpha = 1.0;
    self.streetLabel.alpha = 0.0;
    break;
  case MWMBottomMenuStateCompact:
    if (!IPAD)
    {
      self.bookmarksButton.alpha = 0.0;
      self.p2pButton.alpha = 0.0;
      self.searchButton.alpha = 0.0;
    }
    self.downloadBadge.alpha = 0.0;
    self.goButton.alpha = 0.0;
    self.streetLabel.alpha = 0.0;
    break;
  case MWMBottomMenuStatePlanning:
  case MWMBottomMenuStateGo:
    self.bookmarksButton.alpha = 0.0;
    self.goButton.alpha = 1.0;
    self.p2pButton.alpha = 0.0;
    self.searchButton.alpha = 0.0;
    self.streetLabel.alpha = 0.0;
    break;
  case MWMBottomMenuStateText:
    self.bookmarksButton.alpha = 0.0;
    self.goButton.alpha = 0.0;
    self.p2pButton.alpha = 0.0;
    self.searchButton.alpha = 0.0;
    self.streetLabel.alpha = 1.0;
    break;
  }
}

- (void)updateVisibility
{
  switch (self.state)
  {
  case MWMBottomMenuStateHidden:
    break;
  case MWMBottomMenuStateInactive:
    self.additionalButtons.hidden = YES;
    self.goButton.hidden = YES;
    self.separator.hidden = YES;
    self.streetLabel.hidden = YES;
    break;
  case MWMBottomMenuStateActive:
    self.downloadBadge.hidden = YES;
    self.goButton.hidden = YES;
    self.streetLabel.hidden = YES;
    break;
  case MWMBottomMenuStateCompact:
    if (!IPAD)
    {
      self.bookmarksButton.hidden = YES;
      self.p2pButton.hidden = YES;
      self.searchButton.hidden = YES;
    }
    self.downloadBadge.hidden = YES;
    self.goButton.hidden = YES;
    self.streetLabel.hidden = YES;
    break;
  case MWMBottomMenuStatePlanning:
  case MWMBottomMenuStateGo:
    self.bookmarksButton.hidden = YES;
    self.p2pButton.hidden = YES;
    self.searchButton.hidden = YES;
    self.streetLabel.hidden = YES;
    break;
  case MWMBottomMenuStateText:
    self.bookmarksButton.hidden = YES;
    self.goButton.hidden = YES;
    self.p2pButton.hidden = YES;
    self.searchButton.hidden = YES;
    self.streetLabel.hidden = NO;
    break;
  }
}

- (void)layoutGeometry
{
  switch (self.state)
  {
  case MWMBottomMenuStateHidden:
    self.minY = self.superview.height;
    return;
  case MWMBottomMenuStateInactive:
  case MWMBottomMenuStateCompact:
  case MWMBottomMenuStatePlanning:
  case MWMBottomMenuStateGo:
  case MWMBottomMenuStateText:
    self.additionalButtonsHeight.constant = 0.0;
    self.separator.height = 0.0;
    break;
  case MWMBottomMenuStateActive:
    {
      BOOL const isLandscape = self.width > self.layoutThreshold;
      if (isLandscape)
      {
        self.additionalButtonsHeight.constant = 64.0;
      }
      else
      {
        NSUInteger const additionalButtonsCount = [self.additionalButtons numberOfItemsInSection:0];
        CGFloat const buttonHeight = 52.0;
        self.additionalButtonsHeight.constant = additionalButtonsCount * buttonHeight;
      }
    }
    break;
  }
  CGFloat const width = MIN(self.superview.width - self.leftBound, self.superview.width);
  CGFloat const height = self.mainButtons.height + self.separator.height + self.additionalButtonsHeight.constant;
  self.frame = {{self.superview.width - width, self.superview.height - height}, {width, height}};
  self.mainButtonWidth.constant = self.separatorWidth.constant = self.additionalButtonsWidth.constant = width;
}

- (void)updateMenuButtonFromState:(MWMBottomMenuState)fromState toState:(MWMBottomMenuState)toState
{
  if (fromState == MWMBottomMenuStateActive || toState == MWMBottomMenuStateActive)
    [self morphMenuButtonTemplate:@"ic_menu_" toState:toState];
  else if (fromState == MWMBottomMenuStateCompact || toState == MWMBottomMenuStateCompact)
    [self morphMenuButtonTemplate:@"ic_menu_rotate_" toState:toState];
  [self refreshMenuButtonState];
}

- (void)morphMenuButtonTemplate:(NSString *)morphTemplate toState:(MWMBottomMenuState)toState
{
  BOOL const direct = toState == MWMBottomMenuStateActive || toState == MWMBottomMenuStateCompact;
  UIButton * btn = self.menuButton;
  NSUInteger const morphImagesCount = 6;
  NSUInteger const startValue = direct ? 1 : morphImagesCount;
  NSUInteger const endValue = direct ? morphImagesCount + 1 : 0;
  NSInteger const stepValue = direct ? 1 : -1;
  NSMutableArray * morphImages = [NSMutableArray arrayWithCapacity:morphImagesCount];
  for (NSUInteger i = startValue, j = 0; i != endValue; i += stepValue, j++)
    morphImages[j] = [UIImage imageNamed:[NSString stringWithFormat:@"%@%@_%@", morphTemplate, @(i).stringValue,
                                          [UIColor isNightMode] ? @"dark" : @"light"]];
  btn.imageView.animationImages = morphImages;
  btn.imageView.animationRepeatCount = 1;
  btn.imageView.image = morphImages.lastObject;
  [btn.imageView startAnimating];
}

- (void)refreshMenuButtonState
{
  dispatch_async(dispatch_get_main_queue(), ^
  {
    if (self.menuButton.imageView.isAnimating)
    {
      [self refreshMenuButtonState];
    }
    else
    {
      UIButton * btn = self.menuButton;
      NSString * name = nil;
      switch (self.state)
      {
      case MWMBottomMenuStateHidden:
      case MWMBottomMenuStateInactive:
      case MWMBottomMenuStatePlanning:
      case MWMBottomMenuStateGo:
      case MWMBottomMenuStateText:
        name = @"ic_menu";
        break;
      case MWMBottomMenuStateActive:
        name = @"ic_menu_down";
        break;
      case MWMBottomMenuStateCompact:
        name = @"ic_menu_left";
        break;
      }
      UIImage * image = [UIImage imageNamed:name];
      if (isIOS7)
        image = [image imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
      [btn setImage:image forState:UIControlStateNormal];
    }
  });
}

- (void)refreshOnOrientationChange
{
  [self refreshButtonsColor];
  if (IPAD || self.state != MWMBottomMenuStateCompact)
    return;
  BOOL const isPortrait = self.superview.width < self.superview.height;
  if (isPortrait)
    self.owner.leftBound = 0.0;
}

- (void)refreshLayout
{
  self.layoutDuration = kDefaultAnimationDuration;
  [self setNeedsLayout];
  [self refreshButtonsColor];
  if (self.state == MWMBottomMenuStateInactive)
    [self updateBadge];
}

- (void)refreshButtonsColor
{
  if (!isIOS7)
    return;
  auto const coloring = self.p2pButton.coloring;
  self.p2pButton.coloring = coloring;
  self.bookmarksButton.coloring = coloring;
  self.locationButton.coloring = self.locationButton.coloring;
  self.searchButton.coloring = self.searchButton.coloring;
}

- (void)updateBadge
{
  auto & s = GetFramework().Storage();
  storage::Storage::UpdateInfo updateInfo{};
  s.GetUpdateInfo(updateInfo);
  self.downloadBadge.hidden = (updateInfo.m_numberOfMwmFilesToUpdate == 0);
}

#pragma mark - Properties

- (void)setFrame:(CGRect)frame
{
  CGFloat const minWidth = self.locationButton.width + self.menuButton.width;
  frame.size.width = MAX(minWidth, frame.size.width);
  super.frame = frame;
}

- (void)setState:(MWMBottomMenuState)state
{
  if (_state == state)
    return;
  [self refreshLayout];
  BOOL updateMenuButton = YES;
  switch (state)
  {
    case MWMBottomMenuStateHidden:
      updateMenuButton = NO;
      break;
    case MWMBottomMenuStateInactive:
    {
      if (MapsAppDelegate.theApp.routingPlaneMode == MWMRoutingPlaneModeNone)
        _leftBound = 0.0;
      [self updateBadge];
      self.p2pButton.hidden = self.searchButton.hidden = self.bookmarksButton.hidden = NO;
      self.layoutDuration = (_state == MWMBottomMenuStateCompact && !IPAD) ? 0.0 : kDefaultAnimationDuration;
      updateMenuButton = (_state != MWMBottomMenuStateGo && _state != MWMBottomMenuStatePlanning &&
                          _state != MWMBottomMenuStateText);
      break;
    }
    case MWMBottomMenuStateActive:
      self.restoreState = _state;
      [self updateMenuButtonFromState:_state toState:state];
      self.additionalButtons.hidden = NO;
      self.bookmarksButton.hidden = NO;
      self.p2pButton.hidden = NO;
      self.searchButton.hidden = NO;
      self.separator.hidden = NO;
      break;
    case MWMBottomMenuStateCompact:
      self.layoutDuration = IPAD ? kDefaultAnimationDuration : 0.0;
      break;
    case MWMBottomMenuStatePlanning:
      self.goButton.enabled = NO;
      self.goButton.hidden = NO;
      break;
    case MWMBottomMenuStateGo:
      self.goButton.enabled = YES;
      self.goButton.hidden = NO;
      break;
    case MWMBottomMenuStateText:
      self.streetLabel.font = [UIFont medium16];
      self.streetLabel.hidden = NO;
      self.streetLabel.textColor = [UIColor blackSecondaryText];
      break;
  }
  if (updateMenuButton)
    [self updateMenuButtonFromState:_state toState:state];
  _state = state;
}

- (void)setLeftBound:(CGFloat)leftBound
{
  _leftBound = MAX(leftBound, 0.0);
  self.state = _leftBound > 1.0 ? MWMBottomMenuStateCompact : self.restoreState;
  [self setNeedsLayout];
}

- (void)setSearchIsActive:(BOOL)searchIsActive
{
  _searchIsActive = self.searchButton.selected = searchIsActive;
}

@end
