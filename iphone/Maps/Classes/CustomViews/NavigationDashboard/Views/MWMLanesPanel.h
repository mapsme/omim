#import "MWMRouteHelperPanel.h"

#include "platform/location.hpp"

@interface MWMLanesPanel : MWMRouteHelperPanel

- (instancetype _Nullable)initWithParentView:(UIView *)parentView;
- (void)configureWithLanes:(vector<location::FollowingInfo::SingleLaneInfoClient> const &)lanes;

@end
