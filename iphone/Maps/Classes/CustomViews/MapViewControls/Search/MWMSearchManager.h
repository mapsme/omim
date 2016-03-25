#import "MWMAlertViewController.h"
#import "MWMSearchTextField.h"
#import "MWMSearchView.h"

typedef NS_ENUM(NSUInteger, MWMSearchManagerState)
{
  MWMSearchManagerStateHidden,
  MWMSearchManagerStateDefault,
  MWMSearchManagerStateTableSearch,
  MWMSearchManagerStateMapSearch
};

@protocol MWMSearchManagerProtocol <NSObject>

@property (nonatomic, readonly) MWMAlertViewController * _Nonnull alertController;

- (void)searchViewDidEnterState:(MWMSearchManagerState)state;
- (void)actionDownloadMaps;

@end

@protocol MWMRoutingProtocol;

@interface MWMSearchManager : NSObject

@property (weak, nonatomic) id <MWMSearchManagerProtocol, MWMRoutingProtocol> _Nullable delegate;
@property (weak, nonatomic) IBOutlet MWMSearchTextField * _Nullable searchTextField;

@property (nonatomic) MWMSearchManagerState state;

@property (nonatomic, readonly) UIView * _Nonnull view;

- (instancetype _Nullable)init __attribute__((unavailable("init is not available")));
- (instancetype _Nullable)initWithParentView:(UIView * _Nonnull)view
                                   delegate:(id<MWMSearchManagerProtocol, MWMSearchViewProtocol, MWMRoutingProtocol> _Nonnull)delegate;

- (void)mwm_refreshUI;

#pragma mark - Layout

- (void)willRotateToInterfaceOrientation:(UIInterfaceOrientation)toInterfaceOrientation
                                duration:(NSTimeInterval)duration;
- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:(id<UIViewControllerTransitionCoordinator> _Nonnull)coordinator;

@end
