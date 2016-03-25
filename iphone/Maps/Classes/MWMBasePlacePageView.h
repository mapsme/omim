@class MWMPlacePageEntity, MWMDirectionView, MWMPlacePageTypeDescriptionView;

@interface MWMBasePlacePageView : SolidTouchView

@property (weak, nonatomic) IBOutlet UILabel * _Nullable titleLabel;
@property (weak, nonatomic) IBOutlet UILabel * _Nullable typeLabel;
@property (weak, nonatomic) IBOutlet UILabel * _Nullable addressLabel;
@property (weak, nonatomic) IBOutlet UILabel * _Nullable distanceLabel;
@property (weak, nonatomic) IBOutlet UIImageView * _Nullable directionArrow;
@property (weak, nonatomic) IBOutlet UITableView * _Nullable featureTable;
@property (weak, nonatomic) IBOutlet UIView * _Nullable separatorView;
@property (weak, nonatomic) IBOutlet UIButton * _Nullable directionButton;

- (void)configureWithEntity:(MWMPlacePageEntity *)entity;
- (void)addBookmark;
- (void)removeBookmark;
- (void)reloadBookmarkCell;
- (void)updateAndLayoutMyPositionSpeedAndAltitude:(NSString *)text;

@end
