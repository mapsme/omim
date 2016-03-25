#import "MWMDownloadMapRequest.h"

@interface MWMDownloadMapRequestView : SolidTouchView

@property (nonatomic) enum MWMDownloadMapRequestState state;

- (instancetype _Nullable)initWithFrame:(CGRect)frame __attribute__((unavailable("initWithFrame is not available")));
- (instancetype _Nullable)init __attribute__((unavailable("init is not available")));

@end
