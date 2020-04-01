//
//  MPHTMLInterstitialCustomEvent.m
//  MoPub
//
//  Copyright (c) 2013 MoPub. All rights reserved.
//

#import "MPHTMLInterstitialCustomEvent.h"
#import "MPLogging.h"
#import "MPAdConfiguration.h"
#import "MPInstanceProvider.h"

@interface MPHTMLInterstitialCustomEvent ()

@property (nonatomic, strong) MPHTMLInterstitialViewController *interstitial;
@property (nonatomic, assign) BOOL trackedImpression;

@end

@implementation MPHTMLInterstitialCustomEvent

@synthesize interstitial = _interstitial;

- (BOOL)enableAutomaticImpressionAndClickTracking
{
    // An HTML interstitial tracks its own clicks. Turn off automatic tracking to prevent the tap event callback
    // from generating an additional click.
    // However, an HTML interstitial does not track its own impressions so we must manually do it in this class.
    // See interstitialDidAppear:
    return NO;
}

- (void)requestInterstitialWithCustomEventInfo:(NSDictionary *)info
{
    MPLogInfo(@"Loading MoPub HTML interstitial");
    MPAdConfiguration *configuration = [self.delegate configuration];
    MPLogTrace(@"Loading HTML interstitial with source: %@", [configuration adResponseHTMLString]);

    self.interstitial = [[MPInstanceProvider sharedProvider] buildMPHTMLInterstitialViewControllerWithDelegate:self
                                                                                               orientationType:configuration.orientationType];
    [self.interstitial loadConfiguration:configuration];
}

- (void)showInterstitialFromRootViewController:(UIViewController *)rootViewController
{
    [self.interstitial presentInterstitialFromViewController:rootViewController];
}

#pragma mark - MPInterstitialViewControllerDelegate

- (CLLocation *)location
{
    return [self.delegate location];
}

- (NSString *)adUnitId
{
    return [self.delegate adUnitId];
}

- (void)interstitialDidLoadAd:(MPInterstitialViewController *)interstitial
{
    MPLogInfo(@"MoPub HTML interstitial did load");
    [self.delegate interstitialCustomEvent:self didLoadAd:self.interstitial];
}

- (void)interstitialDidFailToLoadAd:(MPInterstitialViewController *)interstitial
{
    MPLogInfo(@"MoPub HTML interstitial did fail");
    [self.delegate interstitialCustomEvent:self didFailToLoadAdWithError:nil];
}

- (void)interstitialWillAppear:(MPInterstitialViewController *)interstitial
{
    MPLogInfo(@"MoPub HTML interstitial will appear");
    [self.delegate interstitialCustomEventWillAppear:self];
}

- (void)interstitialDidAppear:(MPInterstitialViewController *)interstitial
{
    MPLogInfo(@"MoPub HTML interstitial did appear");
    [self.delegate interstitialCustomEventDidAppear:self];

    if (!self.trackedImpression) {
        self.trackedImpression = YES;
        [self.delegate trackImpression];
    }
}

- (void)interstitialWillDisappear:(MPInterstitialViewController *)interstitial
{
    MPLogInfo(@"MoPub HTML interstitial will disappear");
    [self.delegate interstitialCustomEventWillDisappear:self];
}

- (void)interstitialDidDisappear:(MPInterstitialViewController *)interstitial
{
    MPLogInfo(@"MoPub HTML interstitial did disappear");
    [self.delegate interstitialCustomEventDidDisappear:self];

    // Deallocate the interstitial as we don't need it anymore. If we don't deallocate the interstitial after dismissal,
    // then the html in the webview will continue to run which could lead to bugs such as continuing to play the sound of an inline
    // video since the app may hold onto the interstitial ad controller. Moreover, we keep an array of controllers around as well.
    self.interstitial = nil;
}

- (void)interstitialDidReceiveTapEvent:(MPInterstitialViewController *)interstitial
{
    MPLogInfo(@"MoPub HTML interstitial did receive tap event");
    [self.delegate interstitialCustomEventDidReceiveTapEvent:self];
}

- (void)interstitialWillLeaveApplication:(MPInterstitialViewController *)interstitial
{
    MPLogInfo(@"MoPub HTML interstitial will leave application");
    [self.delegate interstitialCustomEventWillLeaveApplication:self];
}

@end
