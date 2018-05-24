//
//  GADCustomEventExtras.h
//  Google Mobile Ads SDK
//
//  Copyright 2012 Google Inc. All rights reserved.
//

#import <Foundation/Foundation.h>

#import <GoogleMobileAds/GADAdNetworkExtras.h>
#import <GoogleMobileAds/GoogleMobileAdsDefines.h>

GAD_ASSUME_NONNULL_BEGIN

/// Create an instance of this class to set additional parameters for each custom event object. The
/// additional parameters for a custom event are keyed by the custom event label. These extras are
/// passed to your implementation of GADCustomEventBanner or GADCustomEventInterstitial.
@interface GADCustomEventExtras : NSObject<GADAdNetworkExtras>

/// Set additional parameters for the custom event with label |label|. To remove additional
/// parameters associated with |label|, pass in nil for |extras|.
- (void)setExtras:(NSDictionary *GAD_NULLABLE_TYPE)extras forLabel:(NSString *)label;

/// Retrieve the extras for |label|.
- (NSDictionary *GAD_NULLABLE_TYPE)extrasForLabel:(NSString *)label;

/// Removes all the extras set on this instance.
- (void)removeAllExtras;

/// Returns all the extras set on this instance.
- (NSDictionary *)allExtras;

@end

GAD_ASSUME_NONNULL_END
