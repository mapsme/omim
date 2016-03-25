@interface MWMAPIBar : NSObject

@property (nonatomic) BOOL isVisible;

- (instancetype _Nullable)initWithController:(UIViewController * _Nonnull)controller;

- (void)back;

@end
