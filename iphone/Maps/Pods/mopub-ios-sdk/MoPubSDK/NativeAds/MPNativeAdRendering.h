//
//  MPNativeAdRendering.h
//
//  Copyright 2018-2019 Twitter, Inc.
//  Licensed under the MoPub SDK License Agreement
//  http://www.mopub.com/legal/sdk-license-agreement/
//

#import <Foundation/Foundation.h>

#import "MPNativeAd.h"

@class MPNativeAdRenderingImageLoader;

/**
 * The MPNativeAdRendering protocol provides methods for displaying ad content in
 * custom view classes.
 *
 * It can be used for both static native ads and native video ads. If you are serving
 * native video ads, you need to implement nativeVideoView.
 */

@protocol MPNativeAdRendering <NSObject>

@optional

/**
 * Return the UILabel that your view is using for the main text.
 *
 * @return a UILabel that is used for the main text.
 */
- (UILabel *)nativeMainTextLabel;

/**
 * Return the UILabel that your view is using for the title text.
 *
 * @return a UILabel that is used for the title text.
 */
- (UILabel *)nativeTitleTextLabel;

/**
 * Return the UIImageView that your view is using for the icon image.
 *
 * @return a UIImageView that is used for the icon image.
 */
- (UIImageView *)nativeIconImageView;

/**
 * Return the UIImageView that your view is using for the main image.
 *
 * @return a UIImageView that is used for the main image.
 */
- (UIImageView *)nativeMainImageView;

/**
 * Return the UIView that your view is using for the video.
 * You only need to implement this when you are serving video ads.
 *
 * @return a UIView that is used to hold the video.
 */

- (UIView *)nativeVideoView;

/**
 * Returns the UILabel that your view is using for the call to action (cta) text.
 *
 * @return a UILabel that is used for the cta text.
 */
- (UILabel *)nativeCallToActionTextLabel;

/**
 * Returns the UIImageView that your view is using for the privacy information icon.
 *
 * @return a UIImageView that is used for the privacy information icon.
 */
- (UIImageView *)nativePrivacyInformationIconImageView;

/**
 * This method is called if the ad contains a star rating.
 *
 * Implement this method if you expect and wish to display a star rating.
 *
 * @param starRating An NSNumber that is a float in the range of 0.0f and 5.0f.
 */
- (void)layoutStarRating:(NSNumber *)starRating;

/**
 * This method allows you to insert your custom native ad elements into your view.
 *
 * This method will be called when your ad view is added to the view hierarchy.
 *
 * @param customProperties Dictionary that contains custom native ad elements.
 * @param imageLoader Use this object to load your custom images by calling `loadImageForURL:intoImageView:`.
 */
- (void)layoutCustomAssetsWithProperties:(NSDictionary *)customProperties imageLoader:(MPNativeAdRenderingImageLoader *)imageLoader;

/**
 * Specifies a nib object containing a view that should be used to render ads.
 *
 * If you want to use a nib object to render ads, you must implement this method.
 *
 * @return an initialized UINib object. This is not allowed to be `nil`.
 */
+ (UINib *)nibForAd;

@end
