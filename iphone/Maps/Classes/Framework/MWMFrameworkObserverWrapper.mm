#import "MWMFrameworkObserverWrapper.h"

@implementation MWMFrameworkObserverWrapper

- (instancetype)initWithObserver:(id<MWMFrameworkObserver>)observer
{
  self = [super init];
  if (self)
    _observer = observer;
  return self;
}

@end
