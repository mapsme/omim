#import "MWMNavigationViewProtocol.h"

@interface MWMNavigationView : SolidTouchView

@property (nonatomic) CGFloat topBound;
@property (nonatomic) CGFloat leftBound;
@property (nonatomic, readonly) CGFloat visibleHeight;
@property (nonatomic, readonly) CGFloat defaultHeight;
@property (nonatomic, readonly) BOOL isVisible;
@property (nonatomic) UIView * statusbarBackground;
@property (weak, nonatomic) id<MWMNavigationViewProtocol> delegate;
@property (weak, nonatomic, readonly) IBOutlet UIView * contentView;

- (void)addToView:(UIView *)superview;
- (void)remove;
- (CGRect)defaultFrame;

@end
