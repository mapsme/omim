#import "MWMRouterRecommendation.h"

#include "routing/router.hpp"
#include "routing/routing_callbacks.hpp"

#include "storage/storage.hpp"
#include "storage/storage_defines.hpp"

#include "platform/downloader_defines.hpp"

using namespace storage;

@protocol MWMFrameworkObserver<NSObject>

@end

@protocol MWMFrameworkRouteBuilderObserver<MWMFrameworkObserver>

- (void)processRouteBuilderEvent:(routing::RouterResultCode)code
                       countries:(storage::CountriesSet const &)absentCountries;

@optional

- (void)processRouteBuilderProgress:(CGFloat)progress;
- (void)processRouteRecommendation:(MWMRouterRecommendation)recommendation;
- (void)speedCameraShowedUpOnRoute:(double)speedLimit;
- (void)speedCameraLeftVisibleArea;

@end

@protocol MWMFrameworkStorageObserver<MWMFrameworkObserver>

- (void)processCountryEvent:(storage::CountryId const &)countryId;

@optional

- (void)processCountry:(storage::CountryId const &)countryId
              progress:(downloader::Progress const &)progress;

@end

@protocol MWMFrameworkDrapeObserver<MWMFrameworkObserver>

@optional

- (void)processViewportCountryEvent:(storage::CountryId const &)countryId;
- (void)processViewportChangedEvent;

@end
