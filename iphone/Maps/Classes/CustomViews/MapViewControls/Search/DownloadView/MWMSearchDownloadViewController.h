#import "MWMViewController.h"

@protocol MWMSearchDownloadProtocol <NSObject>

@property (nonatomic, readonly) MWMAlertViewController * _Nonnull alertController;

- (void)selectMapsAction;

@end

@interface MWMSearchDownloadViewController : MWMViewController

- (instancetype _Nullable)init __attribute__((unavailable("init is not available")));
- (instancetype _Nullable)initWithDelegate:(id<MWMSearchDownloadProtocol> _Nonnull)delegate;

- (void)downloadProgress:(CGFloat)progress;
- (void)setDownloadFailed;

@end
