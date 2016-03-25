#import <UIKit/UIKit.h>

@class MWMPlacePageViewManager;

@interface MWMDirectionView : UIView

@property (weak, nonatomic) IBOutlet UILabel * _Nullable titleLabel;
@property (weak, nonatomic) IBOutlet UILabel * _Nullable typeLabel;
@property (weak, nonatomic) IBOutlet UILabel * _Nullable distanceLabel;
@property (weak, nonatomic) IBOutlet UIImageView * _Nullable directionArrow;
@property (weak, nonatomic) IBOutlet UIView * _Nullable contentView;

- (instancetype _Nullable)initWithManager:(MWMPlacePageViewManager *)manager;
- (void)setDirectionArrowTransform:(CGAffineTransform)transform;

- (instancetype _Nullable)initWithCoder:(NSCoder *)aDecoder __attribute__((unavailable("initWithCoder is not available")));
- (instancetype _Nullable)initWithFrame:(CGRect)frame __attribute__((unavailable("initWithFrame is not available")));
- (instancetype _Nullable)init __attribute__((unavailable("init is not available")));


@end
