#import "MWMButton.h"

@interface MWMCircularProgressView : UIView

@property (nonatomic, readonly) BOOL animating;

@property (nonatomic) MWMCircularProgressState state;
@property (nonatomic) BOOL isInvertColor;

- (instancetype _Nullable)initWithFrame:(CGRect)frame __attribute__((unavailable("initWithFrame is not available")));
- (instancetype _Nullable)init __attribute__((unavailable("init is not available")));

- (void)setImage:(UIImage * _Nonnull)image forState:(MWMCircularProgressState)state;
- (void)setColor:(UIColor * _Nonnull)color forState:(MWMCircularProgressState)state;
- (void)setColoring:(MWMButtonColoring)coloring forState:(MWMCircularProgressState)state;

- (void)animateFromValue:(CGFloat)fromValue toValue:(CGFloat)toValue;

- (void)updatePath:(CGFloat)progress;

@end
