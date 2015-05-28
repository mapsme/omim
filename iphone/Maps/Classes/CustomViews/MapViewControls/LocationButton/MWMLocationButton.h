#import <Foundation/Foundation.h>

#include "platform/location.hpp"

@interface MWMLocationButton : NSObject

@property (nonatomic) BOOL hidden;

- (instancetype)init __attribute__((unavailable("init is not available")));
- (instancetype)initWithParentView:(UIView *)view;
- (void)setMyPositionMode:(location::EMyPositionMode)mode;

@end
