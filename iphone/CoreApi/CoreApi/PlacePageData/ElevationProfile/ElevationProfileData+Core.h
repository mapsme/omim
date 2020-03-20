#import "ElevationProfileData.h"

#include "map/elevation_info.hpp"

NS_ASSUME_NONNULL_BEGIN

@interface ElevationProfileData (Core)

- (instancetype)initWithElevationInfo:(ElevationInfo const &)elevationInfo activePoint:(double)activePoint;

@end

NS_ASSUME_NONNULL_END
