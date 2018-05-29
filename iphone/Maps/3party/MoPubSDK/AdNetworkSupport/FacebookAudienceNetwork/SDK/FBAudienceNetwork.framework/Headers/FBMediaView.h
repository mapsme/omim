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

#import <UIKit/UIKit.h>

#import <FBAudienceNetwork/FBAdDefines.h>
#import <FBAudienceNetwork/FBMediaViewVideoRenderer.h>

NS_ASSUME_NONNULL_BEGIN

@protocol FBMediaViewDelegate;
@class FBNativeAd;

/**
  The FBMediaView loads media content from a given FBNativeAd. This view takes the place of manually loading a cover image.
 */
FB_CLASS_EXPORT
@interface FBMediaView : UIView

/**
  the delegate
 */
@property (nonatomic, weak, nullable) id<FBMediaViewDelegate> delegate;

/**
  This is a method to create a media view using the given native ad.
 - Parameter nativeAd: The native ad to load media content from.
 */
- (instancetype)initWithNativeAd:(FBNativeAd *)nativeAd;

/**
  the native ad, can be set again to reuse this view.
 */
@property (nonatomic, strong) FBNativeAd *nativeAd;

/**
 A custom FBMediaViewVideoRenderer instance, used to override the default user experience of video ads.
 */
@property (nonatomic, strong) FBMediaViewVideoRenderer *videoRenderer;

/**
  The current volume of the media view, ranging from 0.0 through 1.0.
 */
@property (nonatomic, assign, readonly) float volume FB_DEPRECATED;

/**
  Enables or disables autoplay for some types of media. Defaults to YES.
 */
@property (nonatomic, assign, getter=isAutoplayEnabled) BOOL autoplayEnabled;

/**
 The aspect ratio of the media view visual content. Returns a positive CGFloat, or 0.0 if no ad is currently loaded.
 */
@property (nonatomic, assign, readonly) CGFloat aspectRatio;

/**
 Changes the width of the FBMediaView's frame based on the current height, respecting aspectRatio.
 */
- (void)applyNaturalWidth;

/**
 Changes the height of the FBMediaView's frame based on the current width, respecting aspectRatio.
 */
- (void)applyNaturalHeight;

// Setting autoplayEnabled in the SDK is deprecated. Migrate to using server-side control when available.
- (void)setAutoplayEnabled:(BOOL)autoplayEnabled FB_DEPRECATED;

@end

/**
  The methods declared by the FBMediaViewDelegate protocol allow the adopting delegate to respond to messages from the FBMediaView class and thus respond to operations such as whether the media content has been loaded.
 */
@protocol FBMediaViewDelegate <NSObject>

@optional

/**
  Sent when an FBMediaView has been successfully loaded.

 - Parameter mediaView: An FBMediaView object sending the message.
 */
- (void)mediaViewDidLoad:(FBMediaView *)mediaView;

/**
  Sent just before an FBMediaView will enter the fullscreen layout.

 - Parameter mediaView: An FBMediaView object sending the message.
 */
- (void)mediaViewWillEnterFullscreen:(FBMediaView *)mediaView;

/**
  Sent after an FBMediaView has exited the fullscreen layout.

 - Parameter mediaView: An FBMediaView object sending the message.
 */
- (void)mediaViewDidExitFullscreen:(FBMediaView *)mediaView;

/**
  Sent when an FBMediaView has changed the playback volume of a video ad.

 - Parameter mediaView: An FBMediaView object sending the message.
 - Parameter volume: The current ad video volume (after the volume change).
 */
- (void)mediaView:(FBMediaView *)mediaView videoVolumeDidChange:(float)volume;

/**
  Sent after a video ad in an FBMediaView enters a paused state.

 - Parameter mediaView: An FBMediaView object sending the message.
 */
- (void)mediaViewVideoDidPause:(FBMediaView *)mediaView;

/**
  Sent after a video ad in an FBMediaView enters a playing state.

 - Parameter mediaView: An FBMediaView object sending the message.
 */
- (void)mediaViewVideoDidPlay:(FBMediaView *)mediaView;

/**
  Sent when a video ad in an FBMediaView reaches the end of playback.

 - Parameter mediaView: An FBMediaView object sending the message.
 */
- (void)mediaViewVideoDidComplete:(FBMediaView *)mediaView;

@end

NS_ASSUME_NONNULL_END
