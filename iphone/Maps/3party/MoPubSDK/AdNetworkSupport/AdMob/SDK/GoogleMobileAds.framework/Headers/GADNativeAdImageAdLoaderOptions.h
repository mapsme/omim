//
//  GADNativeAdImageAdLoaderOptions.h
//  Google Mobile Ads SDK
//
//  Copyright 2015 Google Inc. All rights reserved.
//

#import <GoogleMobileAds/GADAdLoader.h>
#import <GoogleMobileAds/GoogleMobileAdsDefines.h>

GAD_ASSUME_NONNULL_BEGIN

/// Native ad image orientation preference.
typedef NS_ENUM(NSInteger, GADNativeAdImageAdLoaderOptionsOrientation) {
  GADNativeAdImageAdLoaderOptionsOrientationAny = 1,       ///< No orientation preference.
  GADNativeAdImageAdLoaderOptionsOrientationPortrait = 2,  ///< Prefer portrait images.
  GADNativeAdImageAdLoaderOptionsOrientationLandscape = 3  ///< Prefer landscape images.
};

/// Ad loader options for native ad image settings.
@interface GADNativeAdImageAdLoaderOptions : GADAdLoaderOptions

/// Indicates if image asset content should be loaded by the SDK. If set to YES, the SDK will not
/// load image asset content and native ad image URLs can be used to fetch content. Defaults to NO,
/// image assets are loaded by the SDK.
@property(nonatomic, assign) BOOL disableImageLoading;

/// Indicates if multiple images should be loaded for each asset. Defaults to NO.
@property(nonatomic, assign) BOOL shouldRequestMultipleImages;

/// Indicates preferred image orientation. Defaults to
/// GADNativeAdImageAdLoaderOptionsOrientationAny.
@property(nonatomic, assign) GADNativeAdImageAdLoaderOptionsOrientation preferredImageOrientation;

@end

GAD_ASSUME_NONNULL_END
