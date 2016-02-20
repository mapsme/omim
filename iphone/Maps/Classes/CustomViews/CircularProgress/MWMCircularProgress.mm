#import "MWMCircularProgress.h"
#import "MWMCircularProgressView.h"

@interface MWMCircularProgress ()

@property (nonatomic) IBOutlet MWMCircularProgressView * rootView;
@property (nonatomic) NSNumber * nextProgressToAnimate;

@end

@implementation MWMCircularProgress

- (nonnull instancetype)initWithParentView:(nonnull UIView *)parentView
{
  self = [super init];
  if (self)
  {
    [[NSBundle mainBundle] loadNibNamed:self.class.className owner:self options:nil];
    [parentView.subviews makeObjectsPerformSelector:@selector(removeFromSuperview)];
    [parentView addSubview:self.rootView];
    [self reset];
    self.state = MWMCircularProgressStateNormal;
  }
  return self;
}

- (void)dealloc
{
  [self.rootView removeFromSuperview];
}

- (void)reset
{
  _progress = 0.;
  [self.rootView updatePath:0.];
  self.nextProgressToAnimate = nil;
}

- (void)setImage:(nonnull UIImage *)image forState:(MWMCircularProgressState)state
{
  [self.rootView setImage:image forState:state];
}

- (void)setColor:(nonnull UIColor *)color forState:(MWMCircularProgressState)state
{
  [self.rootView setColor:color forState:state];
}

- (void)setInvertColor:(BOOL)invertColor
{
  self.rootView.isInvertColor = invertColor;
}

#pragma mark - Animation

- (void)animationDidStop:(CABasicAnimation *)anim finished:(BOOL)flag
{
  if (self.nextProgressToAnimate)
  {
    self.progress = self.nextProgressToAnimate.doubleValue;
    self.nextProgressToAnimate = nil;
  }
}

#pragma mark - Actions

- (IBAction)buttonTouchUpInside:(UIButton *)sender
{
  [self.delegate progressButtonPressed:self];
}

#pragma mark - Properties

- (void)setProgress:(CGFloat)progress
{
  if (progress <= _progress)
  {
    self.state = MWMCircularProgressStateSpinner;
    return;
  }
  self.rootView.state = MWMCircularProgressStateProgress;
  if (self.rootView.animating)
  {
    if (progress > self.nextProgressToAnimate.floatValue)
      self.nextProgressToAnimate = @(progress);
  }
  else
  {
    [self.rootView animateFromValue:self->_progress toValue:progress];
    _progress = progress;
  }
}

- (void)setState:(MWMCircularProgressState)state
{
  dispatch_async(dispatch_get_main_queue(), ^
  {
    [self reset];
    self.rootView.state = state;
  });
}

- (MWMCircularProgressState)state
{
  return self.rootView.state;
}

@end
