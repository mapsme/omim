// Copyright 2004-present Facebook. All Rights Reserved.
//
// You are hereby granted a non-exclusive, worldwide, royalty-free license to use,
// copy, modify, and distribute this software in source code or binary form for use
// in connection with the web services and APIs provided by Facebook.
//
// As with any software that integrates with the Facebook platform, your use of
// this software is subject to the Facebook Developer Principles and Policies
// [http://developers.facebook.com/policy/]. This copyright notice shall be
// included in all copies or substantial portions of the software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#import <StoreKit/StoreKit.h>
#import <UIKit/UIKit.h>

#import <FBAudienceNetwork/FBAdDefines.h>
#import <FBAudienceNetwork/FBAdSize.h>

NS_ASSUME_NONNULL_BEGIN

@protocol FBAdViewDelegate;

/**
  A customized UIView to represent a Facebook ad (a.k.a. banner ad).
 */
FB_CLASS_EXPORT
@interface FBAdView : UIView

/**
  This is a method to initialize an FBAdView matching the given placement id.

 - Parameter placementID: The id of the ad placement. You can create your placement id from Facebook developers page.
 - Parameter adSize: The size of the ad; for example, kFBAdSizeHeight50Banner or kFBAdSizeHeight90Banner.
 - Parameter viewController: The view controller that will be used to present the ad and the app store view.
 */
- (instancetype)initWithPlacementID:(NSString *)placementID
                             adSize:(FBAdSize)adSize
                 rootViewController:(nullable UIViewController *)viewController NS_DESIGNATED_INITIALIZER;

/**
  Begins loading the FBAdView content.


 You can implement `adViewDidLoad:` and `adView:didFailWithError:` methods
 of `FBAdViewDelegate` if you would like to be notified as loading succeeds or fails.
 */
- (void)loadAd;

/**
 Begins loading the FBAdView content from a bid payload attained through a server side bid.


 You can implement `adViewDidLoad:` and `adView:didFailWithError:` methods
 of `FBAdViewDelegate` if you would like to be notified as loading succeeds or fails.

 - Parameter bidPayload: The payload of the ad bid. You can get your bid id from Facebook bidder endpoint.
 */
- (void)loadAdWithBidPayload:(NSString *)bidPayload;

/**
  This is a method to disable auto refresh for the FBAdView instance


 By default, we read the refresh interval from the placement setting in your Facebook
 developers page. Once you call this method, the auto refresh will be disabled for this FBAdView
 instance, and you cannot re-enable the refresh for this instance. A new created FBAdView will still
 use the default behavior.

 This method is designed for ad network mediation. We still recommend you to set the placement
 refresh interval as 'None' if you're using one of the ad network mediation.
 */
- (void)disableAutoRefresh;

/**
  Typed access to the id of the ad placement.
 */
@property (nonatomic, copy, readonly) NSString *placementID;
/**
  Typed access to the app's root view controller.
 */
@property (nonatomic, weak, readonly, nullable) UIViewController *rootViewController;
/**
  the delegate
 */
@property (nonatomic, weak, nullable) id<FBAdViewDelegate> delegate;

@end

/**
  The methods declared by the FBAdViewDelegate protocol allow the adopting delegate to respond
 to messages from the FBAdView class and thus respond to operations such as whether the ad has
 been loaded, the person has clicked the ad.
 */
@protocol FBAdViewDelegate <NSObject>

@optional

/**
  Sent after an ad has been clicked by the person.

 - Parameter adView: An FBAdView object sending the message.
 */
- (void)adViewDidClick:(FBAdView *)adView;
/**
  When an ad is clicked, the modal view will be presented. And when the user finishes the
 interaction with the modal view and dismiss it, this message will be sent, returning control
 to the application.

 - Parameter adView: An FBAdView object sending the message.
 */
- (void)adViewDidFinishHandlingClick:(FBAdView *)adView;
/**
  Sent when an ad has been successfully loaded.

 - Parameter adView: An FBAdView object sending the message.
 */
- (void)adViewDidLoad:(FBAdView *)adView;
/**
  Sent after an FBAdView fails to load the ad.

 - Parameter adView: An FBAdView object sending the message.
 - Parameter error: An error object containing details of the error.
 */
- (void)adView:(FBAdView *)adView didFailWithError:(NSError *)error;

/**
  Sent immediately before the impression of an FBAdView object will be logged.

 - Parameter adView: An FBAdView object sending the message.
 */
- (void)adViewWillLogImpression:(FBAdView *)adView;

/**
  Asks the delegate for a view controller to present modal content, such as the in-app
 browser that can appear when an ad is clicked.

 - Returns: A view controller that is used to present modal content.
 */
@property (nonatomic, readonly, strong, nonnull) UIViewController *viewControllerForPresentingModalView;

@end

NS_ASSUME_NONNULL_END
