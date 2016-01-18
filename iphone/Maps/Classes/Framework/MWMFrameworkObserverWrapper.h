#import "MWMFrameworkObservers.h"

@interface MWMFrameworkObserverWrapper : NSObject

@property (weak, nonatomic, readonly) id<MWMFrameworkObserver> observer;

- (instancetype)init __attribute__((unavailable("init is not available")));

- (instancetype)initWithObserver:(id<MWMFrameworkObserver>)observer;

@end
