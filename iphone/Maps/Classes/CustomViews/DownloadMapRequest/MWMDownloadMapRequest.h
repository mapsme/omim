#import "MWMAlertViewController.h"

typedef NS_ENUM(NSUInteger, MWMDownloadMapRequestState)
{
  MWMDownloadMapRequestStateDownload,
  MWMDownloadMapRequestStateRequestLocation,
  MWMDownloadMapRequestStateRequestUnknownLocation
};

@protocol MWMDownloadMapRequestProtocol <NSObject>

@property (nonatomic, readonly) MWMAlertViewController * _Nonnull alertController;

- (void)stateUpdated:(enum MWMDownloadMapRequestState)state;
- (void)selectMapsAction;

@end

@interface MWMDownloadMapRequest : NSObject

- (instancetype _Nullable)init __attribute__((unavailable("init is not available")));
- (instancetype _Nullable)initWithParentView:(UIView * _Nonnull)parentView
                                  delegate:(id <MWMDownloadMapRequestProtocol> _Nonnull)delegate;

- (void)showRequest;

- (void)downloadProgress:(CGFloat)progress;
- (void)setDownloadFailed;

@end
