//
//  GADCustomEventInterstitialDelegate.h
//  Google Mobile Ads SDK
//
//  Copyright 2012 Google Inc. All rights reserved.
//

#import <Foundation/Foundation.h>

#import <GoogleMobileAds/GoogleMobileAdsDefines.h>

GAD_ASSUME_NONNULL_BEGIN

@protocol GADCustomEventInterstitial;

/// Call back to this delegate in your custom event. You must call
/// customEventInterstitialDidReceiveAd: when there is an ad to show, or
/// customEventInterstitial:didFailAd: when there is no ad to show. Otherwise, if enough time passed
/// (several seconds) after the SDK called the requestInterstitialAdWithParameter: method of your
/// custom event, the mediation SDK will consider the request timed out, and move on to the next ad
/// network.
@protocol GADCustomEventInterstitialDelegate<NSObject>

/// Your Custom Event object must call this when it receives or creates an interstitial ad.
- (void)customEventInterstitialDidReceiveAd:(id<GADCustomEventInterstitial>)customEvent;

/// Your Custom Event object must call this when it fails to receive or create the ad. Pass along
/// any error object sent from the ad network's SDK, or an NSError describing the error. Pass nil if
/// not available.
- (void)customEventInterstitial:(id<GADCustomEventInterstitial>)customEvent
                      didFailAd:(NSError *GAD_NULLABLE_TYPE)error;

/// Your Custom Event object should call this when the user touches or "clicks" the ad to initiate
/// an action. When the SDK receives this callback, it reports the click back to the mediation
/// server.
- (void)customEventInterstitialWasClicked:(id<GADCustomEventInterstitial>)customEvent;

// When you call any of the following methods, the call will be propagated back to the
// GADInterstitialDelegate that you implemented and passed to GADInterstitial.

/// Your Custom Event should call this when the interstitial is being displayed.
- (void)customEventInterstitialWillPresent:(id<GADCustomEventInterstitial>)customEvent;

/// Your Custom Event should call this when the interstitial is about to be dismissed.
- (void)customEventInterstitialWillDismiss:(id<GADCustomEventInterstitial>)customEvent;

/// Your Custom Event should call this when the interstitial has been dismissed.
- (void)customEventInterstitialDidDismiss:(id<GADCustomEventInterstitial>)customEvent;

/// Your Custom Event should call this method when a user action will result in app switching.
- (void)customEventInterstitialWillLeaveApplication:(id<GADCustomEventInterstitial>)customEvent;

#pragma mark Deprecated

/// Deprecated. Use customEventInterstitialDidReceiveAd:.
- (void)customEventInterstitial:(id<GADCustomEventInterstitial>)customEvent
                   didReceiveAd:(NSObject *)ad
    GAD_DEPRECATED_MSG_ATTRIBUTE("Use customEventInterstitialDidReceiveAd:.");

@end

GAD_ASSUME_NONNULL_END
