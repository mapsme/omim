//
//  GADMediatedUnifiedNativeAd.h
//  Google Mobile Ads SDK
//
//  Copyright 2017 Google Inc. All rights reserved.
//

#import <Foundation/Foundation.h>

#import <GoogleMobileAds/GADNativeAdImage.h>
#import <GoogleMobileAds/GADUnifiedNativeAdAssetIdentifiers.h>
#import <GoogleMobileAds/GoogleMobileAdsDefines.h>

GAD_ASSUME_NONNULL_BEGIN

/// Provides methods used for constructing native ads. The adapter must return an object conforming
/// to this protocol for native ad requests.
@protocol GADMediatedUnifiedNativeAd<NSObject>

/// Headline.
@property(nonatomic, readonly, copy, nullable) NSString *headline;

/// Array of GADNativeAdImage objects.
@property(nonatomic, readonly, nullable) NSArray<GADNativeAdImage *> *images;

/// Description.
@property(nonatomic, readonly, copy, nullable) NSString *body;

/// Icon image.
@property(nonatomic, readonly, nullable) GADNativeAdImage *icon;

/// Text that encourages user to take some action with the ad. For example "Install".
@property(nonatomic, readonly, copy, nullable) NSString *callToAction;

/// App store rating (0 to 5).
@property(nonatomic, readonly, copy, nullable) NSDecimalNumber *starRating;

/// The app store name. For example, "App Store".
@property(nonatomic, readonly, copy, nullable) NSString *store;

/// String representation of the app's price.
@property(nonatomic, readonly, copy, nullable) NSString *price;

/// Identifies the advertiser. For example, the advertiser’s name or visible URL.
@property(nonatomic, readonly, copy, nullable) NSString *advertiser;

/// Returns a dictionary of asset names and object pairs for assets that are not handled by
/// properties of the GADMediatedUnifiedNativeAd.
@property(nonatomic, readonly, copy, nullable) NSDictionary<NSString *, id> *extraAssets;

@optional

/// AdChoices view.
@property(nonatomic, readonly, nullable) UIView *adChoicesView;

/// Media view.
@property(nonatomic, readonly, nullable) UIView *mediaView;

/// Indicates whether the ad has video content.
@property(nonatomic, assign, readonly) BOOL hasVideoContent;

/// Tells the receiver that it has been rendered in |view| with clickable asset views and
/// nonclickable asset views. viewController should be used to present modal views for the ad.
- (void)didRenderInView:(UIView *)view
       clickableAssetViews:
           (NSDictionary<GADUnifiedNativeAssetIdentifier, UIView *> *)clickableAssetViews
    nonclickableAssetViews:
        (NSDictionary<GADUnifiedNativeAssetIdentifier, UIView *> *)nonclickableAssetViews
            viewController:(UIViewController *)viewController;

/// Tells the receiver that an impression is recorded. This method is called only once per mediated
/// native ad.
- (void)didRecordImpression;

/// Tells the receiver that a user click is recorded on the asset named |assetName|. Full screen
/// actions should be presented from viewController. This method is called only if
/// -[GADMAdNetworkAdapter handlesUserClicks] returns NO.
- (void)didRecordClickOnAssetWithName:(GADUnifiedNativeAssetIdentifier)assetName
                                 view:(UIView *)view
                       viewController:(UIViewController *)viewController;

/// Tells the receiver that it has untracked |view|. This method is called when the mediatedNativeAd
/// is no longer rendered in the provided view and the delegate should stop tracking the view's
/// impressions and clicks. The method may also be called with a nil view when the view in which the
/// mediated native ad has rendered is deallocated.
- (void)didUntrackView:(nullable UIView *)view;

@end

GAD_ASSUME_NONNULL_END
