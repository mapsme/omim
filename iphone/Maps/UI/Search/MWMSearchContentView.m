#import "MWMSearchContentView.h"

@interface MWMSearchContentView ()

@property (strong, nonatomic) IBOutlet SolidTouchView *solidTouchView;

@end

@implementation MWMSearchContentView

- (void)layoutSubviews
{
  [self.subviews enumerateObjectsUsingBlock:^(UIView * view, NSUInteger idx, BOOL * stop)
  {
    view.frame = self.bounds;
  }];
  [super layoutSubviews];
}

- (void)mwm_refreshUI
{
  self.solidTouchView.backgroundColor = UIColor.white;
}

@end
