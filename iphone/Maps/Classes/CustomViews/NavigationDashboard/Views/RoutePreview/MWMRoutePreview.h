#import "MWMNavigationView.h"

@protocol MWMRoutePreviewDataSource <NSObject>

@required
- (NSString *)source;
- (NSString *)destination;

@end

@class MWMNavigationDashboardEntity;
@class MWMRouteTypeButton;
@class MWMNavigationDashboardManager;
@class MWMCircularProgress;

@interface MWMRoutePreview : MWMNavigationView

@property (weak, nonatomic, readonly) IBOutlet UIButton * _Nullable extendButton;
@property (weak, nonatomic) id<MWMRoutePreviewDataSource> _Nullable dataSource;
@property (weak, nonatomic) MWMNavigationDashboardManager * _Nullable dashboardManager;

- (MWMCircularProgress *)pedestrianProgress;
- (MWMCircularProgress *)vehicleProgress;

- (void)configureWithEntity:(MWMNavigationDashboardEntity *)entity;
- (void)statePrepare;
- (void)statePlanning;
- (void)stateError;
- (void)stateReady;
- (void)reloadData;
- (void)selectProgress:(MWMCircularProgress *)progress;

@end
