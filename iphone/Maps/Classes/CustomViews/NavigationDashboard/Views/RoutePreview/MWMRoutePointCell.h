@class MWMRoutePointCell;

@protocol MWMRoutePointCellDelegate <NSObject>

@required
- (void)didPan:(UIPanGestureRecognizer *)pan cell:(MWMRoutePointCell *)cell;
- (void)startEditingCell:(MWMRoutePointCell *)cell;

@end

@interface MWMRoutePointCell : UICollectionViewCell

@property (weak, nonatomic) IBOutlet UITextField * _Nullable title;
@property (weak, nonatomic) IBOutlet UILabel * _Nullable number;
@property (weak, nonatomic) id<MWMRoutePointCellDelegate> _Nullable delegate;

@end
