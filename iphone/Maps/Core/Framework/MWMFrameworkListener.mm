#import "MWMFrameworkListener.h"
#import "MapsAppDelegate.h"

#include "Framework.h"

#include "std/mutex.hpp"

namespace
{
using Observer = id<MWMFrameworkObserver>;
using TRouteBuildingObserver = id<MWMFrameworkRouteBuilderObserver>;
using TStorageObserver = id<MWMFrameworkStorageObserver>;
using TDrapeObserver = id<MWMFrameworkDrapeObserver>;

using Observers = NSHashTable<Observer>;

Protocol * pRouteBuildingObserver = @protocol(MWMFrameworkRouteBuilderObserver);
Protocol * pStorageObserver = @protocol(MWMFrameworkStorageObserver);
Protocol * pDrapeObserver = @protocol(MWMFrameworkDrapeObserver);

using TLoopBlock = void (^)(__kindof Observer observer);

void loopWrappers(Observers * observers, TLoopBlock block)
{
  dispatch_async(dispatch_get_main_queue(), ^{
    for (Observer observer in observers)
    {
      if (observer)
        block(observer);
    }
  });
}
}  // namespace

@interface MWMFrameworkListener ()

@property(nonatomic) Observers * routeBuildingObservers;
@property(nonatomic) Observers * storageObservers;
@property(nonatomic) Observers * drapeObservers;

@end

@implementation MWMFrameworkListener

+ (MWMFrameworkListener *)listener
{
  static MWMFrameworkListener * listener;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    listener = [[super alloc] initListener];
  });
  return listener;
}

+ (void)addObserver:(Observer)observer
{
  dispatch_async(dispatch_get_main_queue(), ^{
    MWMFrameworkListener * listener = [MWMFrameworkListener listener];
    if ([observer conformsToProtocol:pRouteBuildingObserver])
      [listener.routeBuildingObservers addObject:observer];
    if ([observer conformsToProtocol:pStorageObserver])
      [listener.storageObservers addObject:observer];
    if ([observer conformsToProtocol:pDrapeObserver])
      [listener.drapeObservers addObject:observer];
  });
}

+ (void)removeObserver:(Observer)observer
{
  dispatch_async(dispatch_get_main_queue(), ^{
    MWMFrameworkListener * listener = [MWMFrameworkListener listener];
    [listener.routeBuildingObservers removeObject:observer];
    [listener.storageObservers removeObject:observer];
    [listener.drapeObservers removeObject:observer];
  });
}

- (instancetype)initListener
{
  self = [super init];
  if (self)
  {
    _routeBuildingObservers = [Observers weakObjectsHashTable];
    _storageObservers = [Observers weakObjectsHashTable];
    _drapeObservers = [Observers weakObjectsHashTable];

    [self registerRouteBuilderListener];
    [self registerStorageObserver];
    [self registerDrapeObserver];
  }
  return self;
}

#pragma mark - MWMFrameworkRouteBuilderObserver

- (void)registerRouteBuilderListener
{
  using namespace routing;
  using namespace storage;
  Observers * observers = self.routeBuildingObservers;
  auto & f = GetFramework();
  // TODO(ldragunov,rokuz): Thise two routing callbacks are the only framework callbacks which does
  // not guarantee
  // that they are called on a main UI thread context. Discuss it with Lev.
  // Simplest solution is to insert RunOnGuiThread() call in the core where callbacks are called.
  // This will help to avoid unnecessary parameters copying and will make all our framework
  // callbacks
  // consistent: every notification to UI will run on a main UI thread.
  f.GetRoutingManager().SetRouteBuildingListener(
      [observers](IRouter::ResultCode code, TCountriesVec const & absentCountries) {
        loopWrappers(observers, [code, absentCountries](TRouteBuildingObserver observer) {
          [observer processRouteBuilderEvent:code countries:absentCountries];
        });
      });
  f.GetRoutingManager().SetRouteProgressListener([observers](float progress) {
    loopWrappers(observers, [progress](TRouteBuildingObserver observer) {
      if ([observer respondsToSelector:@selector(processRouteBuilderProgress:)])
        [observer processRouteBuilderProgress:progress];
    });
  });
}

#pragma mark - MWMFrameworkStorageObserver

- (void)registerStorageObserver
{
  Observers * observers = self.storageObservers;
  auto & s = GetFramework().GetStorage();
  s.Subscribe(
      [observers](TCountryId const & countryId) {
        for (TStorageObserver observer in observers)
          [observer processCountryEvent:countryId];
      },
      [observers](TCountryId const & countryId, MapFilesDownloader::TProgress const & progress) {
        for (TStorageObserver observer in observers)
        {
          if ([observer respondsToSelector:@selector(processCountry:progress:)])
            [observer processCountry:countryId progress:progress];
        }
      });
}

#pragma mark - MWMFrameworkDrapeObserver

- (void)registerDrapeObserver
{
  Observers * observers = self.drapeObservers;
  auto & f = GetFramework();
  f.SetCurrentCountryChangedListener([observers](TCountryId const & countryId) {
    for (TDrapeObserver observer in observers)
      [observer processViewportCountryEvent:countryId];
  });
}

@end
