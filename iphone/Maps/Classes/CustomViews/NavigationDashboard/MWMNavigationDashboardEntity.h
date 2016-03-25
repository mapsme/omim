#include "platform/location.hpp"

@interface MWMNavigationDashboardEntity : NSObject

@property (nonatomic, readonly) NSString * targetDistance;
@property (nonatomic, readonly) NSString * targetUnits;
@property (nonatomic, readonly) NSString * distanceToTurn;
@property (nonatomic, readonly) NSString * turnUnits;
@property (nonatomic, readonly) NSString * streetName;
@property (nonatomic, readonly) UIImage * turnImage;
@property (nonatomic, readonly) UIImage * nextTurnImage;
@property (nonatomic, readonly) NSUInteger roundExitNumber;
@property (nonatomic, readonly) NSUInteger timeToTarget;
@property (nonatomic, readonly) CGFloat progress;
//@property (nonatomic, readonly) vector<location::FollowingInfo::SingleLaneInfoClient> lanes;
@property (nonatomic, readonly) BOOL isPedestrian;

- (void)updateWithFollowingInfo:(location::FollowingInfo const &)info;

+ (instancetype _Nullable)new __attribute__((unavailable("init is not available")));

@end
