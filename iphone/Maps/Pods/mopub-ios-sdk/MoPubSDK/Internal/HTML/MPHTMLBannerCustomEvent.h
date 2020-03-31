//
//  MPHTMLBannerCustomEvent.h
//
//  Copyright 2018-2019 Twitter, Inc.
//  Licensed under the MoPub SDK License Agreement
//  http://www.mopub.com/legal/sdk-license-agreement/
//

#import "MPBannerCustomEvent.h"
#import "MPAdWebViewAgent.h"
#import "MPPrivateBannerCustomEventDelegate.h"

@interface MPHTMLBannerCustomEvent : MPBannerCustomEvent <MPAdWebViewAgentDelegate>

@property (nonatomic, weak) id<MPPrivateBannerCustomEventDelegate> delegate;

@end
