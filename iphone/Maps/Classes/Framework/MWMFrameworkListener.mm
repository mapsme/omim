#import "MapsAppDelegate.h"
#import "MWMFrameworkListener.h"
#import "MWMFrameworkObserverWrapper.h"

#include "Framework.h"

namespace
{

using TWrapper = MWMFrameworkObserverWrapper;
using TWrapperSet = NSMutableSet<TWrapper *>;

using TObserver = id<MWMFrameworkObserver>;
using TRouteBuildingObserver = id<MWMFrameworkRouteBuilderObserver>;
using TMyPositionObserver = id<MWMFrameworkMyPositionObserver>;
using TUsermarkObserver = id<MWMFrameworkUserMarkObserver>;
using TStorageObserver = id<MWMFrameworkStorageObserver>;

Protocol * pRouteBuildingObserver = @protocol(MWMFrameworkRouteBuilderObserver);
Protocol * pMyPositionObserver = @protocol(MWMFrameworkMyPositionObserver);
Protocol * pUserMarkObserver = @protocol(MWMFrameworkUserMarkObserver);
Protocol * pStorageObserver = @protocol(MWMFrameworkStorageObserver);

using TLoopBlock = void (^)(__kindof TObserver observer);

void loopWrappers(TWrapperSet * wrappers, TLoopBlock block)
{
  dispatch_async(dispatch_get_main_queue(), ^
  {
    for (TWrapper * wrapper in [wrappers copy])
    {
      TObserver observer = wrapper.observer;
      if (observer)
        block(observer);
      else
        [wrappers removeObject:wrapper];
    }
  });
}
} // namespace

@interface MWMFrameworkListener ()

@property (nonatomic) TWrapperSet * routeBuildingObservers;
@property (nonatomic) TWrapperSet * myPositionObservers;
@property (nonatomic) TWrapperSet * userMarkObservers;
@property (nonatomic) TWrapperSet * storageObservers;

@property (nonatomic, readwrite) location::EMyPositionMode myPositionMode;

@end

@implementation MWMFrameworkListener
{
  unique_ptr<UserMarkCopy> m_userMark;
}

+ (MWMFrameworkListener *)listener
{
  return MapsAppDelegate.theApp.frameworkListener;
}

- (instancetype)init
{
  self = [super init];
  if (self)
  {
    _routeBuildingObservers = [TWrapperSet set];
    _myPositionObservers = [TWrapperSet set];
    _userMarkObservers = [TWrapperSet set];
    _storageObservers = [TWrapperSet set];

    [self registerRouteBuilderListener];
    [self registerMyPositionListener];
    [self registerUserMarkObserver];
    [self registerStorageObserver];
  }
  return self;
}

- (void)addObserver:(TObserver)observer
{
  if ([observer conformsToProtocol:pRouteBuildingObserver])
    [self.routeBuildingObservers addObject:[[TWrapper alloc] initWithObserver:observer]];
  if ([observer conformsToProtocol:pMyPositionObserver])
    [self.myPositionObservers addObject:[[TWrapper alloc] initWithObserver:observer]];
  if ([observer conformsToProtocol:pUserMarkObserver])
    [self.userMarkObservers addObject:[[TWrapper alloc] initWithObserver:observer]];
  if ([observer conformsToProtocol:pStorageObserver])
    [self.storageObservers addObject:[[TWrapper alloc] initWithObserver:observer]];
}

#pragma mark - MWMFrameworkRouteBuildingObserver

- (void)registerRouteBuilderListener
{
  TWrapperSet * wrappers = self.routeBuildingObservers;
  auto & f = GetFramework();
  f.SetRouteBuildingListener([wrappers](routing::IRouter::ResultCode code, vector<storage::TCountryId> const & absentCountries, vector<storage::TCountryId> const & absentRoutes)
  {
    loopWrappers(wrappers, ^(TRouteBuildingObserver observer)
    {
      [observer processRouteBuilderEvent:code countries:absentCountries routes:absentRoutes];
    });
  });
  f.SetRouteProgressListener([wrappers](float progress)
  {
    loopWrappers(wrappers, ^(TRouteBuildingObserver observer)
    {
      if ([observer respondsToSelector:@selector(processRouteBuilderProgress:)])
        [observer processRouteBuilderProgress:progress];
    });
  });
}

#pragma mark - MWMFrameworkMyPositionObserver

- (void)registerMyPositionListener
{
  TWrapperSet * wrappers = self.myPositionObservers;
  auto & f = GetFramework();
  f.SetMyPositionModeListener([self, wrappers](location::EMyPositionMode mode)
  {
    self.myPositionMode = mode;
    loopWrappers(wrappers, ^(TMyPositionObserver observer)
    {
      [observer processMyPositionStateModeChange:mode];
    });
  });
}

#pragma mark - MWMFrameworkUserMarkObserver

- (void)registerUserMarkObserver
{
  TWrapperSet * wrappers = self.userMarkObservers;
  auto & f = GetFramework();
  f.SetUserMarkActivationListener([self, wrappers](unique_ptr<UserMarkCopy> mark)
  {
    m_userMark = move(mark);
    loopWrappers(wrappers, ^(TUsermarkObserver observer)
    {
      [observer processUserMarkActivation:self->m_userMark->GetUserMark()];
    });
  });
}

#pragma mark - MWMFrameworkStorageObserver

- (void)registerStorageObserver
{
  TWrapperSet * wrappers = self.storageObservers;
  auto & f = GetFramework();
  auto & s = f.Storage();
  s.Subscribe([wrappers](storage::TCountryId const & countryId)
  {
    loopWrappers(wrappers, ^(TStorageObserver observer)
    {
      [observer processCountryEvent:countryId];
    });
  },
  [wrappers](storage::TCountryId const & countryId, storage::LocalAndRemoteSizeT const & progress)
  {
    loopWrappers(wrappers, ^(TStorageObserver observer)
    {
      if ([observer respondsToSelector:@selector(processCountry:progress:)])
        [observer processCountry:countryId progress:progress];
    });
  });
}

#pragma mark - Properties

- (UserMark const *)userMark
{
  return m_userMark ? m_userMark->GetUserMark() : nullptr;
}

- (void)setUserMark:(const UserMark *)userMark
{
  if (userMark)
    m_userMark.reset(new UserMarkCopy(userMark, false));
  else
    m_userMark = nullptr;
}

@end
