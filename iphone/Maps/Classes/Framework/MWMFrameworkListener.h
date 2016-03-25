#import "MWMFrameworkObservers.h"

#include "platform/location.hpp"

@interface MWMFrameworkListener : NSObject

+ (MWMFrameworkListener *)listener;
+ (void)addObserver:(id<MWMFrameworkObserver>)observer;
+ (void)removeObserver:(id<MWMFrameworkObserver>)observer;

- (instancetype _Nullable)init __attribute__((unavailable("call +listener instead")));
- (instancetype _Nullable)copy __attribute__((unavailable("call +listener instead")));
- (instancetype _Nullable)copyWithZone:(NSZone *)zone __attribute__((unavailable("call +listener instead")));
+ (instancetype _Nullable)alloc __attribute__((unavailable("call +listener instead")));
+ (instancetype _Nullable)allocWithZone:(struct _NSZone *)zone __attribute__((unavailable("call +listener instead")));
+ (instancetype _Nullable)new __attribute__((unavailable("call +listener instead")));

@end
