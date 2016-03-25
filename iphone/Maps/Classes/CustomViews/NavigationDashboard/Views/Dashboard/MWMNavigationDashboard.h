#import "MWMNavigationView.h"

@class MWMNavigationDashboardEntity;

@interface MWMNavigationDashboard : MWMNavigationView

@property (weak, nonatomic) IBOutlet UIImageView * _Nullable direction;
@property (weak, nonatomic) IBOutlet UILabel * _Nullable distanceToNextAction;
@property (weak, nonatomic) IBOutlet UILabel * _Nullable distanceToNextActionUnits;
@property (weak, nonatomic) IBOutlet UILabel * _Nullable distanceLeft;
@property (weak, nonatomic) IBOutlet UILabel * _Nullable eta;
@property (weak, nonatomic) IBOutlet UILabel * _Nullable arrivalsTimeLabel;
@property (weak, nonatomic) IBOutlet UILabel * _Nullable roundRoadLabel;
@property (weak, nonatomic) IBOutlet UILabel * _Nullable streetLabel;
@property (weak, nonatomic) IBOutlet UIButton * _Nullable soundButton;
@property (weak, nonatomic) IBOutlet UISlider * _Nullable progress;

- (void)configureWithEntity:(MWMNavigationDashboardEntity *)entity;

@end
