#import <Foundation/Foundation.h>

#include <geometry/point2d.hpp>

@interface RouteState : NSObject

@property (nonatomic, readonly) BOOL hasActualRoute;
@property (nonatomic) m2::PointD endPoint;

+ (instancetype _Nullable)savedState;

+ (void)save;
+ (void)remove;

@end
