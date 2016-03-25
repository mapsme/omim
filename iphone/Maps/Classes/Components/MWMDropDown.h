@interface MWMDropDown : NSObject

- (instancetype _Nullable)initWithSuperview:(UIView *)view;
- (void)showWithMessage:(NSString *)message;
- (void)dismiss;

- (instancetype _Nullable)init __attribute__((unavailable("call -initWithSuperview: instead!")));
+ (instancetype _Nullable)new __attribute__((unavailable("call -initWithSuperview: instead!")));
- (instancetype _Nullable)initWithCoder:(NSCoder *)aDecoder __attribute__((unavailable("call -initWithSuperview: instead!")));
- (instancetype _Nullable)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil __attribute__((unavailable("call -initWithSuperview: instead!")));

@end
