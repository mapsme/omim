#include "map/user_mark.hpp"
#include "platform/location.hpp"
#include "routing/router.hpp"
#include "storage/index.hpp"
#include "storage/storage_defines.hpp"

using namespace storage;

@protocol MWMFrameworkObserver <NSObject>

@end

@protocol MWMFrameworkRouteBuilderObserver <MWMFrameworkObserver>

- (void)processRouteBuilderEvent:(routing::IRouter::ResultCode)code
                       countries:(TCountriesVec const &)absentCountries
                          routes:(TCountriesVec const &)absentRoutes;

@optional

- (void)processRouteBuilderProgress:(CGFloat)progress;

@end

@protocol MWMFrameworkMyPositionObserver <MWMFrameworkObserver>

- (void)processMyPositionStateModeEvent:(location::EMyPositionMode)mode;

@end

@protocol MWMFrameworkUserMarkObserver <MWMFrameworkObserver>

- (void)processUserMarkEvent:(UserMark const *)mark;

@end

@protocol MWMFrameworkStorageObserver <MWMFrameworkObserver>

- (void)processCountryEvent:(TCountryId const &)countryId;

@optional

- (void)processCountry:(TCountryId const &)countryId progress:(TLocalAndRemoteSize const &)progress;

@end

@protocol MWMFrameworkDrapeObserver <MWMFrameworkObserver>

- (void)processViewportCountryEvent:(TCountryId const &)countryId;

@end