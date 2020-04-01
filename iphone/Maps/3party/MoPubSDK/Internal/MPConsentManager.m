//
//  MPConsentManager.m
//  MoPubSDK
//
//  Copyright © 2018 MoPub. All rights reserved.
//

#import <AdSupport/AdSupport.h>
#import "MPAPIEndpoints.h"
#import "MPAdServerURLBuilder.h"
#import "MPConsentAdServerKeys.h"
#import "MPConsentChangedNotification.h"
#import "MPConsentChangedReason.h"
#import "MPConsentError.h"
#import "MPConsentManager.h"
#import "MPConstants.h"
#import "MPHTTPNetworkSession.h"
#import "MPIdentityProvider.h"
#import "MPLogging.h"
#import "MPTimer.h"
#import "MPURLRequest.h"
#import "NSString+MPConsentStatus.h"
#import "MPAdConversionTracker.h"

// NSUserDefault keys
static NSString * const kConsentedIabVendorListStorageKey        = @"com.mopub.mopub-ios-sdk.consented.iab.vendor.list";
static NSString * const kConsentedPrivacyPolicyVersionStorageKey = @"com.mopub.mopub-ios-sdk.consented.privacy.policy.version";
static NSString * const kConsentedVendorListVersionStorageKey    = @"com.mopub.mopub-ios-sdk.consented.vendor.list.version";
static NSString * const kConsentStatusStorageKey                 = @"com.mopub.mopub-ios-sdk.consent.status";
static NSString * const kExtrasStorageKey                        = @"com.mopub.mopub-ios-sdk.extras";
static NSString * const kIabVendorListStorageKey                 = @"com.mopub.mopub-ios-sdk.iab.vendor.list";
static NSString * const kIabVendorListHashStorageKey             = @"com.mopub.mopub-ios-sdk.iab.vendor.list.hash";
static NSString * const kIfaForConsentStorageKey                 = @"com.mopub.mopub-ios-sdk.ifa.for.consent";
static NSString * const kIsDoNotTrackStorageKey                  = @"com.mopub.mopub-ios-sdk.is.do.not.track";
static NSString * const kIsWhitelistedStorageKey                 = @"com.mopub.mopub-ios-sdk.is.whitelisted";
static NSString * const kGDPRAppliesStorageKey                   = @"com.mopub.mopub-ios-sdk.gdpr.applies";
static NSString * const kLastChangedMsStorageKey                 = @"com.mopub.mopub-ios-sdk.last.changed.ms";
static NSString * const kLastChangedReasonStorageKey             = @"com.mopub.mopub-ios-sdk.last.changed.reason";
static NSString * const kLastSynchronizedConsentStatusStorageKey = @"com.mopub.mopub-ios-sdk.last.synchronized.consent.status";
static NSString * const kPrivacyPolicyUrlStorageKey              = @"com.mopub.mopub-ios-sdk.privacy.policy.url";
static NSString * const kPrivacyPolicyVersionStorageKey          = @"com.mopub.mopub-ios-sdk.privacy.policy.version";
static NSString * const kShouldReacquireConsentStorageKey        = @"com.mopub.mopub-ios-sdk.should.reacquire.consent";
static NSString * const kVendorListUrlStorageKey                 = @"com.mopub.mopub-ios-sdk.vendor.list.url";
static NSString * const kVendorListVersionStorageKey             = @"com.mopub.mopub-ios-sdk.vendor.list.version";

// Frequency constants
static NSTimeInterval const kDefaultRefreshInterval = 300; //seconds

// String replacement macros
static NSString * const kMacroReplaceLanguageCode = @"%%LANGUAGE%%";

@interface MPConsentManager() <MPConsentDialogViewControllerDelegate>

/**
 The loaded consent dialog view controller, or nil if the dialog has not been loaded.
 */
@property (nonatomic, strong, nullable) MPConsentDialogViewController * consentDialogViewController;

/**
 Flag indicating the last known "do not track" status. This is primarily used for detecting
 changes in the "do not track" state by the user. This state should only be set by the
 @c setCurrentStatus:reason:shouldBroadcast: method. A value of @c YES indicates
 that we were last known to be in a "do not track" state; otherwise @c NO.
 */
@property (nonatomic, readonly) BOOL isDoNotTrack;

/**
 Timer used to fire the next consent synchronization update. This will be invalidated
 everytime `synchronizeConsentWithCompletion:` is explcitly called. The timer
 frequency is determined by `self.syncFrequency`.
 */
@property (nonatomic, strong, nullable) MPTimer * nextUpdateTimer;

/**
 Queries the raw consent status value that is stored in @c kConsentStatusStorageKey
 */
@property (nonatomic, readonly) MPConsentStatus rawConsentStatus;

/**
 Flag indicating that the server requires reacquisition of consent.
 This flag should be reset once the dialog has been shown to the user
 and the user has consented/not consented, or if the whitelisted publisher
 has explicitly called @c grantConsent or @c revokeConsent.
 @remark Setting this property will not trigger a @c NSUserDefaults.synchronize
 */
@property (nonatomic, assign) BOOL shouldReacquireConsent;

/**
 The maximum frequency in seconds that the SDK is allowed to synchronize consent
 information. Defaults to 300 seconds.
 */
@property (nonatomic, assign, readwrite) NSTimeInterval syncFrequency;

@end

@implementation MPConsentManager

#pragma mark - Initialization

+ (MPConsentManager *)sharedManager {
    static MPConsentManager * sharedMyManager = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        sharedMyManager = [[self alloc] init];
    });
    return sharedMyManager;
}

- (instancetype)init {
    if (self = [super init]) {
        // Register application foreground and background listeners
        [NSNotificationCenter.defaultCenter addObserver:self selector:@selector(onApplicationWillEnterForeground:) name:UIApplicationWillEnterForegroundNotification object:nil];

        // Initialize internal state
        _consentDialogViewController = nil;
        _syncFrequency = kDefaultRefreshInterval;

        // Initializing the timer must be done last since it depends on the
        // value of _syncFrequency
        _nextUpdateTimer = [self newNextUpdateTimer];
    }

    return self;
}

- (void)dealloc {
    [NSNotificationCenter.defaultCenter removeObserver:self];

    // Tear down the next update timer.
    [_nextUpdateTimer invalidate];
    _nextUpdateTimer = nil;
}

#pragma mark - Properties

- (BOOL)canCollectPersonalInfo {
    // We can collection information under the following conditions:
    // 1. GDPR does not apply to the user
    // or
    // 2. GDPR does apply and the user explicitly granted consent.
    return self.isGDPRApplicable == MPBoolNo || (self.isGDPRApplicable == MPBoolYes && self.currentStatus == MPConsentStatusConsented);
}

- (NSString *)currentLanguageCode {
    // Need to strip out any region code if it's in there.
    // In the event we cannot determine the language code, we will default to English.
    NSString * code = [self removeRegionFromLanguageCode:NSLocale.preferredLanguages.firstObject];
    return code != nil ? code : @"en";
}

- (BOOL)isConsentNeeded {
    return self.shouldReacquireConsent || (self.currentStatus == MPConsentStatusUnknown && self.isGDPRApplicable == MPBoolYes);
}

/**
 Flag indicating the last known "do not track" status. This is primarily used for detecting
 changes in the "do not track" state by the user. This state should only be set by the
 @c setCurrentStatus:reason:shouldBroadcast: method.
 @return @c YES if we were in a "do not track" state; otherwise @c NO.
 */
- (BOOL)isDoNotTrack {
    return [NSUserDefaults.standardUserDefaults boolForKey:kIsDoNotTrackStorageKey];
}

- (MPConsentStatus)rawConsentStatus {
    return (MPConsentStatus)[NSUserDefaults.standardUserDefaults integerForKey:kConsentStatusStorageKey];
}

- (BOOL)shouldReacquireConsent {
    return [NSUserDefaults.standardUserDefaults boolForKey:kShouldReacquireConsentStorageKey];
}

- (void)setShouldReacquireConsent:(BOOL)shouldReacquireConsent {
    [NSUserDefaults.standardUserDefaults setBool:shouldReacquireConsent forKey:kShouldReacquireConsentStorageKey];
}

#pragma mark - ISO Language Code

/**
 Strip out any region code if there is one in the ISO langauge code.
 For example: en-US --> en and zh-Hans-HK --> zh
 @param isoLanguageCode ISO-639 compliant language code
 @return The ISO language code stripped of the region if successful; @c nil otherwise.
 */
- (NSString * _Nullable)removeRegionFromLanguageCode:(NSString * _Nullable)isoLanguageCode {
    return [isoLanguageCode componentsSeparatedByString:@"-"].firstObject;
}

#pragma mark - Publisher Consent Granting and Revoking

- (void)grantConsent {
    MPLogInfo(@"Grant consent was called with publisher whitelist status of: %@whitelisted", self.isWhitelisted ? @"" : @"not ");

    // Reset the reacquire consent flag since the user has taken action.
    self.shouldReacquireConsent = NO;

    MPConsentStatus grantStatus = self.isWhitelisted ? MPConsentStatusConsented : MPConsentStatusPotentialWhitelist;
    NSString * grantReason = (grantStatus == MPConsentStatusConsented ? kConsentedChangedReasonWhitelistGranted : kConsentedChangedReasonPotentialWhitelist);

    // Grant consent and if the state has transitioned, immediately synchronize
    // with the server as this is an externally induced state change.
    if ([self setCurrentStatus:grantStatus reason:grantReason shouldBroadcast:YES]) {
        [self synchronizeConsentWithCompletion:^(NSError * _Nullable error) {
            if (error) {
                MPLogError(@"Consent synchronization failed: %@", error.localizedDescription);
            }
            else {
                MPLogInfo(@"Consent synchronization completed");
            }
        }];
    }
}

- (void)revokeConsent {
    // Reset the reacquire consent flag since the user has taken action.
    self.shouldReacquireConsent = NO;

    // Revoke consent and if the state has transitioned, immediately synchronize
    // with the server as this is an externally induced state change.
    if ([self setCurrentStatus:MPConsentStatusDenied reason:kConsentedChangedReasonPublisherDenied shouldBroadcast:YES]) {
        [self synchronizeConsentWithCompletion:^(NSError * _Nullable error) {
            if (error) {
                MPLogError(@"Consent synchronization failed: %@", error.localizedDescription);
            }
            else {
                MPLogInfo(@"Consent synchronization completed");
            }
        }];
    }
}

#pragma mark - Consent Dialog

- (BOOL)isConsentDialogLoaded {
    return self.consentDialogViewController != nil;
}

- (void)loadConsentDialogWithCompletion:(void (^)(NSError *error))completion {
    // Helper block to call completion if not nil
    void (^callCompletion)(NSError *error) = ^(NSError *error) {
        if (completion != nil) {
            completion(error);
        }
    };

    // If "Limit Ad Tracking" is on, do not load, nil any view controller that has already loaded, and send error
    if (self.currentStatus == MPConsentStatusDoNotTrack) {
        self.consentDialogViewController = nil;
        NSError *limitAdTrackingError = [NSError errorWithDomain:kConsentErrorDomain
                                                            code:MPConsentErrorCodeLimitAdTrackingEnabled
                                                        userInfo:nil];
        callCompletion(limitAdTrackingError);
        return;
    }

    // If a view controller is already loaded, don't load another.
    if (self.consentDialogViewController) {
        callCompletion(nil);
        return;
    }

    // Weak self reference for blocks
    __weak __typeof__(self) weakSelf = self;

    // Do network request to get HTML
    MPURLRequest *consentDialogRequest = [MPURLRequest requestWithURL:[MPAdServerURLBuilder consentDialogURL]];
    [MPHTTPNetworkSession startTaskWithHttpRequest:consentDialogRequest responseHandler:^(NSData *data, NSURLResponse *response){
        // De-serialize JSON data
        NSError *deserializationError;
        NSDictionary *consentDialogResponse = [NSJSONSerialization JSONObjectWithData:data options:0 error:&deserializationError];
        // Stop if there was a de-serialization error
        if (!consentDialogResponse) {
            callCompletion(deserializationError);
            return;
        }

        // Initialize view controller with response HTML strings
        MPConsentDialogViewController *viewController = [[MPConsentDialogViewController alloc] initWithDialogHTML:consentDialogResponse[kDialogHTMLKey]];
        viewController.delegate = weakSelf;

        // Load consent page data
        [viewController loadConsentPageWithCompletion:^(BOOL success, NSError *error) {
            if (success) {
                // Notify when page data has been loaded and set the property
                weakSelf.consentDialogViewController = viewController;
                callCompletion(nil);
            } else {
                // Notify when there was an error loading page data and set the property to `nil`.
                weakSelf.consentDialogViewController = nil;
                callCompletion(error);
            }
        }];
    } errorHandler:^(NSError *error){
        // Call completion without success if the network request failed
        callCompletion(error);
    }];
}

- (void)showConsentDialogFromViewController:(UIViewController *)viewController completion:(void (^)(void))completion {
    if (self.isConsentDialogLoaded) {
        [viewController presentViewController:self.consentDialogViewController
                                     animated:YES
                                   completion:completion];
    }
}

// MPConsentDialogViewControllerDelegate
- (void)consentDialogViewControllerDidReceiveConsentResponse:(BOOL)response
                                 consentDialogViewController:(MPConsentDialogViewController *)consentDialogViewController {
    // Reset the reacquire consent flag since the user has taken action.
    self.shouldReacquireConsent = NO;

    // Set consent status
    MPConsentStatus status = (response ? MPConsentStatusConsented : MPConsentStatusDenied);
    NSString * changeReason = (response ? kConsentedChangedReasonGranted : kConsentedChangedReasonDenied);
    BOOL didTransition = [self setCurrentStatus:status reason:changeReason shouldBroadcast:YES];

    // Synchronize only if there was a successful state transition.
    // It is possible that the user responded to the consent dialog while
    // in a "do not track" state.
    if (didTransition) {
        [self synchronizeConsentWithCompletion:^(NSError * _Nullable error) {
            if (error) {
                MPLogInfo(@"Error when syncing consent dialog response: %@", error);
                return;
            }

            MPLogInfo(@"Did sync consent dialog response.");
        }];
    }
}

- (void)consentDialogViewControllerWillDisappear:(MPConsentDialogViewController *)consentDialogViewController {
    self.consentDialogViewController = nil;
}

#pragma mark - Foreground / Background Notification Listeners

- (void)onApplicationWillEnterForeground:(NSNotification *)notification {
    // Always check for "do not track" changes and synchronize with the server
    // whenever possible.
    [self checkForDoNotTrackAndTransition];
    // If IDFA changed, status will be set to MPConsentStatusUnknown.
    [self checkForIfaChange];
    [self synchronizeConsentWithCompletion:^(NSError * _Nullable error) {
        if (error) {
            MPLogError(@"Consent synchronization failed: %@", error.localizedDescription);
        }
        else {
            MPLogInfo(@"Consent synchronization completed");
        }
    }];
}

#pragma mark - Broadcasted Notifications

/**
 Broadcasts a @c NSNotification that the consent status has changed.
 @param newStatus The new consent state.
 @param oldStatus The previous consent state.
 @param canCollectPii Flag indicating that collection of PII is allowed.
 */
- (void)notifyConsentChangedTo:(MPConsentStatus)newStatus
                 fromOldStatus:(MPConsentStatus)oldStatus
                 canCollectPii:(BOOL)canCollectPii {
    // Build the NSNotification userInfo dictionary.
    NSDictionary * userInfo = @{ kMPConsentChangedInfoNewConsentStatusKey: @(newStatus),
                                 kMPConsentChangedInfoPreviousConsentStatusKey: @(oldStatus),
                                 kMPConsentChangedInfoCanCollectPersonalInfoKey: @(canCollectPii)
                                };

    // Broadcast the consent changed notification on the main thread
    // because there may be a listener that is dealing with UI.
    dispatch_async(dispatch_get_main_queue(), ^{
        [NSNotificationCenter.defaultCenter postNotificationName:kMPConsentChangedNotification object:self userInfo:userInfo];
    });

    [self handlePersonalDataOnStateChangeTo:newStatus fromOldStatus:oldStatus];
}

#pragma mark - Ad Server Communication

/**
 Synchronizes the current consent state with the server. The local consent state
 may change depending upon the response from the server.
 @remark Calling this method will reset the current periodic timer update again after @c self.syncFrequency seconds.
 @param completion Required completion block to listen for the result of the synchronization.
 */
- (void)synchronizeConsentWithCompletion:(void (^ _Nonnull)(NSError * error))completion {
    // Invalidate the next update timer since we are synchronizing right now.
    [self.nextUpdateTimer invalidate];
    self.nextUpdateTimer = nil;

    // If GDPR does not apply to the user, synchronizing with the ad server
    // is no longer required. This call will complete without error and no
    // next update timer will be created.
    if (self.isGDPRApplicable == MPBoolNo) {
        MPLogInfo(@"GDPR not applicable, consent synchronization will complete immediately");
        completion(nil);
        return;
    }

    // In the event that the device is in a "do not track" state and there is
    // no IFA to send one last time for the server to revoke consent, further
    // server synchronization is not necessary until the device transitions
    // out of the "do not track" state.
    // In the case that GDPR applicability is unknown, we should perform a sync
    // to determine the final state.
    if (!MPIdentityProvider.advertisingTrackingEnabled && self.ifaForConsent == nil && self.isGDPRApplicable != MPBoolUnknown) {
        MPLogInfo(@"Currently in a do not track state, consent synchronization will complete immediately");
        completion(nil);
        return;
    }

    // Capture the current status being synchronized with the server
    NSString * synchronizedStatus = [NSString stringFromConsentStatus:self.currentStatus];

    // Generate the request.
    NSURL * syncUrl = [MPAdServerURLBuilder consentSynchronizationUrl];
    MPURLRequest * syncRequest = [MPURLRequest requestWithURL:syncUrl];

    // Send the synchronization request out.
    __weak __typeof__(self) weakSelf = self;
    [MPHTTPNetworkSession startTaskWithHttpRequest:syncRequest responseHandler:^(NSData * _Nonnull data, NSHTTPURLResponse * _Nonnull response) {
        __typeof__(self) strongSelf = weakSelf;

        // Update the last successfully synchronized state.
        // We still update this state even if we failed to parse the response
        // because this is a reflection of what we last sent to the server.
        // If we've made it this far, it means that the `synchronizedStatus` was
        // successfully sent to the server. However, it may be the case that the
        // server sends us back an invalid response.
        [NSUserDefaults.standardUserDefaults setObject:synchronizedStatus forKey:kLastSynchronizedConsentStatusStorageKey];

        // Deserialize the JSON response and attempt to parse it
        NSError * deserializationError = nil;
        NSDictionary * json = [NSJSONSerialization JSONObjectWithData:data options:kNilOptions error:&deserializationError];
        if (deserializationError != nil) {
            // Schedule the next timer and complete with error.
            strongSelf.nextUpdateTimer = [strongSelf newNextUpdateTimer];
            MPLogError(@"%@", deserializationError.localizedDescription);
            completion(deserializationError);
            return;
        }

        // Attempt to parse and update the consent state
        NSError * parseError = nil;
        if ([strongSelf updateConsentStateWithParameters:json]) {
            MPLogTrace(@"Successfully parsed consent synchronization response");
        }
        else {
            parseError = [NSError errorWithDomain:kConsentErrorDomain code:MPConsentErrorCodeFailedToParseSynchronizationResponse userInfo:@{ NSLocalizedDescriptionKey: @"Failed to parse consent synchronization response; one or more required fields are missing" }];
            MPLogError(@"%@", parseError.localizedDescription);
        }

        // Schedule the next timer and complete.
        strongSelf.nextUpdateTimer = [strongSelf newNextUpdateTimer];
        completion(parseError);
    } errorHandler:^(NSError * _Nonnull error) {
        __typeof__(self) strongSelf = weakSelf;

        // Schedule the next timer and complete with error.
        strongSelf.nextUpdateTimer = [strongSelf newNextUpdateTimer];
        MPLogError(@"%@", error.localizedDescription);
        completion(error);
    }];
}

#pragma mark - Next Update Timer

/**
 Creates a new next update timer instance and starts the timer immediately. This timer
 will repeat.
 @return A new timer instance.
 */
- (MPTimer * _Nonnull)newNextUpdateTimer {
    MPTimer * timer = [MPTimer timerWithTimeInterval:self.syncFrequency target:self selector:@selector(onNextUpdateFiredWithTimer) repeats:YES];
    [timer scheduleNow];
    return timer;
}

- (void)onNextUpdateFiredWithTimer {
    // Synchronize with the server because it's time.
    [self synchronizeConsentWithCompletion:^(NSError * _Nullable error) {
        if (error) {
            MPLogError(@"Consent synchronization failed: %@", error.localizedDescription);
        }
        else {
            MPLogInfo(@"Consent synchronization completed");
        }
    }];
}

#pragma mark - Internal State Synchronization

/**
 Checks if there is a transition from a do not track state to a tracking state, or a tracking
 state to a do not track state. If detected, the appropriate consent status state change will
 occur locally and trigger the @c kMPConsentChangedNotification.
 @remark This is a local update only and will require a seperate Ad Server synchronization.
 @return @c YES if a transition occurred; @c NO otherwise.
 */
- (BOOL)checkForDoNotTrackAndTransition {
    BOOL didTransition = NO;

    // Transitioned from an "allowed to track" to "do not track" state.
    BOOL trackingAllowed = ASIdentifierManager.sharedManager.advertisingTrackingEnabled;
    MPConsentStatus status = self.currentStatus;
    if (status != MPConsentStatusDoNotTrack && !trackingAllowed) {
        didTransition = [self setCurrentStatus:MPConsentStatusDoNotTrack reason:kConsentedChangedReasonDoNotTrackEnabled shouldBroadcast:YES];
    }
    // Transitioned from a "do not track" state to an "allowed to track" state.
    // If the previously cached state was "deny consent"
    else if (status == MPConsentStatusDoNotTrack && trackingAllowed) {
        MPConsentStatus transitionToState = (self.rawConsentStatus == MPConsentStatusDenied ? MPConsentStatusDenied : MPConsentStatusUnknown);
        NSString * transitionReason = (transitionToState == MPConsentStatusDenied ? kConsentedChangedReasonDoNotTrackDisabled : kConsentedChangedReasonDoNotTrackDisabledNeedConsent);
        didTransition = [self setCurrentStatus:transitionToState reason:transitionReason shouldBroadcast:YES];
    }

    return didTransition;
}

/**
 Checks if there is a transition from a "potential whitelist" state to an "allowed"
 state. If detected, the appropriate consent status state change will occur locally.
 @remark This check should only be performed after the current versions and current
 consent status have been updated, but before broadcasting any consent state change
 notification.
 @return @c YES if a transition occurred; @c NO otherwise.
 */
- (BOOL)checkForWhitelistAllowedAndTransition {
    // Logic only applicable if in the potential whitelist state.
    if (self.currentStatus != MPConsentStatusPotentialWhitelist) {
        return NO;
    }

    BOOL didTransition = NO;
    if (self.isWhitelisted) {
        didTransition = [self setCurrentStatus:MPConsentStatusConsented reason:kConsentedChangedReasonWhitelistGranted shouldBroadcast:NO];
    }

    return didTransition;
}

/**
 Updates the local consent status.
 @param currentStatus The updated status.
 @param reasonForChange Reason for the change in status. This should map to an entry in @c MPConsentChangedReason.h
 @param shouldBroadcast Flag indicating if the change in status broadcasted.
 @return @c YES if the consent status was successfully changed; @c NO otherwise.
 */
- (BOOL)setCurrentStatus:(MPConsentStatus)currentStatus
                  reason:(NSString * _Nonnull)reasonForChange
         shouldBroadcast:(BOOL)shouldBroadcast {
    // Compare the current consent status with the proposed status.
    // Nothing needs to be done if we're not changing state.
    MPConsentStatus oldStatus = self.currentStatus;
    if (oldStatus == currentStatus) {
        MPLogWarn(@"Attempted to set consent status to same value");
        return NO;
    }

    // Disallow setting consent status if we are currently in a "do not track" state
    // and will not transition out of it.
    BOOL trackingEnabledOnDevice = MPIdentityProvider.advertisingTrackingEnabled;
    if (oldStatus == MPConsentStatusDoNotTrack && !trackingEnabledOnDevice) {
        MPLogWarn(@"Attempted to set consent status while in a do not track state");
        return NO;
    }

    // Save IFA for this particular case so it can be used to remove personal data later.
    if (oldStatus != MPConsentStatusConsented && currentStatus == MPConsentStatusConsented) {
        [self storeIfa];
    }

    // Set the time stamp for this consent status update.
    NSUserDefaults * defaults = NSUserDefaults.standardUserDefaults;
    NSTimeInterval timestampInMilliseconds = [NSDate date].timeIntervalSince1970 * 1000.0;
    [defaults setDouble:timestampInMilliseconds forKey:kLastChangedMsStorageKey];

    // Explicitly disallow the setting of `MPConsentStatusDoNotTrack` since we
    // need to preserve the previously cached status in the event that
    // advertiser tracking is allowed.
    if (currentStatus != MPConsentStatusDoNotTrack) {
        [defaults setObject:reasonForChange forKey:kLastChangedReasonStorageKey];
        [defaults setInteger:currentStatus forKey:kConsentStatusStorageKey];
    }

    // "do not track" state is maintained by a seperate storage field.
    [defaults setBool:(currentStatus == MPConsentStatusDoNotTrack) forKey:kIsDoNotTrackStorageKey];

    // If the state has been updated to "consented", copy the current
    // privacy policy version, vendor list version, and IAB vendor list
    // to the equivalent consented fields.
    if (currentStatus == MPConsentStatusConsented) {
        [defaults setObject:self.iabVendorList forKey:kConsentedIabVendorListStorageKey];
        [defaults setObject:self.privacyPolicyVersion forKey:kConsentedPrivacyPolicyVersionStorageKey];
        [defaults setObject:self.vendorListVersion forKey:kConsentedVendorListVersionStorageKey];
    }
    // If the state has transitioned out of the "consented" state, remove any previously
    // consented versions as they no longer apply.
    else if (oldStatus == MPConsentStatusConsented) {
        [defaults setObject:nil forKey:kConsentedIabVendorListStorageKey];
        [defaults setObject:nil forKey:kConsentedPrivacyPolicyVersionStorageKey];
        [defaults setObject:nil forKey:kConsentedVendorListVersionStorageKey];
    }

    if (shouldBroadcast) {
        [self notifyConsentChangedTo:self.currentStatus fromOldStatus:oldStatus canCollectPii:self.canCollectPersonalInfo];
    }

    MPLogInfo(@"Consent state changed to %@: %@", [NSString stringFromConsentStatus:currentStatus], reasonForChange);

    return YES;
}

/**
 Updates the local consent state atomically.
 @param newState Parameters are stored as @c NSString key-value pairs.
 @return @c YES if the parameters were successfully parsed; @c NO otherwise.
 */
- (BOOL)updateConsentStateWithParameters:(NSDictionary * _Nonnull)newState {
    MPLogTrace(@"Attempting to update consent with new state:\n%@", newState);

    // Validate required parameters
    NSString * isWhitelistedValue = newState[kIsWhitelistedKey];
    NSString * isGDPRRegionValue = newState[kIsGDPRRegionKey];
    NSString * currentIabVendorListHash = newState[kIabVendorListHashKey];
    NSString * vendorListUrl = newState[kVendorListUrlKey];
    NSString * vendorListVersion = newState[kVendorListVersionKey];
    NSString * privacyPolicyUrl = newState[kPrivacyPolicyUrlKey];
    NSString * privacyPolicyVersion = newState[kPrivacyPolicyVersionKey];
    if (isWhitelistedValue == nil || isGDPRRegionValue == nil ||
        currentIabVendorListHash == nil ||
        vendorListUrl == nil || vendorListVersion == nil ||
        privacyPolicyUrl == nil || privacyPolicyVersion == nil) {
        MPLogError(@"Failed to parse new state. Missing required fields.");
        return NO;
    }

    // Extract the old field values for comparison.
    MPConsentStatus oldStatus = self.currentStatus;
    MPBool oldGDPRApplicableStatus = self.isGDPRApplicable;

    // Update the required fields.
    NSUserDefaults * defaults = NSUserDefaults.standardUserDefaults;
    [defaults setBool:[isWhitelistedValue boolValue] forKey:kIsWhitelistedStorageKey];

    [defaults setObject:currentIabVendorListHash forKey:kIabVendorListHashStorageKey];
    [defaults setObject:vendorListUrl forKey:kVendorListUrlStorageKey];
    [defaults setObject:vendorListVersion forKey:kVendorListVersionStorageKey];
    [defaults setObject:privacyPolicyUrl forKey:kPrivacyPolicyUrlStorageKey];
    [defaults setObject:privacyPolicyVersion forKey:kPrivacyPolicyVersionStorageKey];

    // A user is considered GDPR applicable if they first launched the app
    // within a GDPR region.
    if (self.isGDPRApplicable == MPBoolUnknown) {
        MPBool gdprApplies = [isGDPRRegionValue boolValue] ? MPBoolYes : MPBoolNo;
        [defaults setInteger:gdprApplies forKey:kGDPRAppliesStorageKey];
    }

    // Optionally force state to explicit no or unknown.
    BOOL shouldForceExplicitNo = [newState[kForceExplicitNoKey] boolValue];
    BOOL shouldInvalidateConsent = [newState[kInvalidateConsentKey] boolValue];
    BOOL shouldReacquireConsent = [newState[kReacquireConsentKey] boolValue];
    NSString * consentChangeReason = newState[kConsentChangedReasonKey];
    [self forceStatusShouldForceExplicitNo:shouldForceExplicitNo
                   shouldInvalidateConsent:shouldInvalidateConsent
                    shouldReacquireConsent:shouldReacquireConsent
                       consentChangeReason:consentChangeReason
                           shouldBroadcast:NO];

    // Optionally update the current IAB vendor list
    NSString * currentIabVendorList = newState[kIabVendorListKey];
    if (currentIabVendorList != nil) {
        [defaults setObject:currentIabVendorList forKey:kIabVendorListStorageKey];

        // In the event that the IAB has changed the structure of the vendor list format,
        // but the vendors in the list remain the same, we should update the consented
        // IAB vendor list to the current one if the privacy policy version and vendor list
        // version match their consented counterparts.
        if ([self.consentedPrivacyPolicyVersion isEqualToString:privacyPolicyVersion] &&
            [self.consentedVendorListVersion isEqualToString:vendorListVersion]) {
            [defaults setObject:currentIabVendorList forKey:kConsentedIabVendorListStorageKey];
        }
    }

    // Optionally update the synchronization frequency
    NSString * syncFrequencyValue = newState[kSyncFrequencyKey];
    if (syncFrequencyValue != nil) {
        NSTimeInterval frequency = [syncFrequencyValue doubleValue];
        self.syncFrequency = (frequency > 0 ? frequency : kDefaultRefreshInterval);
    }

    // Optionally update the server extras field
    NSString * extras = newState[kExtrasKey];
    if (extras != nil) {
        [defaults setObject:extras forKey:kExtrasStorageKey];
    }

    // State transition check for server finally whitelisting a publisher.
    [self checkForWhitelistAllowedAndTransition];

    // Broadcast the `kMPConsentChangedNotification` if needed.
    if ((oldStatus != self.currentStatus) || (oldGDPRApplicableStatus != self.isGDPRApplicable)) {
        [self notifyConsentChangedTo:self.currentStatus fromOldStatus:oldStatus canCollectPii:self.canCollectPersonalInfo];
    }

    return YES;
}

- (void)forceStatusShouldForceExplicitNo:(BOOL)shouldForceExplicitNo
                 shouldInvalidateConsent:(BOOL)shouldInvalidateConsent
                  shouldReacquireConsent:(BOOL)shouldReacquireConsent
                     consentChangeReason:(NSString *)consentChangeReason
                         shouldBroadcast:(BOOL)shouldBroadcast {
    if (shouldForceExplicitNo) {
        self.shouldReacquireConsent = NO;
        [self setCurrentStatus:MPConsentStatusDenied reason:consentChangeReason shouldBroadcast:shouldBroadcast];
    }
    else if (shouldInvalidateConsent) {
        self.shouldReacquireConsent = NO;
        [self setCurrentStatus:MPConsentStatusUnknown reason:consentChangeReason shouldBroadcast:shouldBroadcast];
    }
    else if (shouldReacquireConsent) {
        self.shouldReacquireConsent = YES;
    }
}

@end

@implementation MPConsentManager (State)

#pragma mark - Read Only Properties

/**
 Replace all the macros in the URL format.
 @param urlFormat URL format string from ad server
 @param isoLanguageCode ISO language code used for macro replacement
 @return A valid URL if successful; @c nil otherwise
 */
- (NSURL * _Nullable)urlWithFormat:(NSString * _Nullable)urlFormat isoLanguageCode:(NSString * _Nullable)isoLanguageCode {
    NSString * regionFreeLangaugeCode = [self removeRegionFromLanguageCode:isoLanguageCode];
    if (regionFreeLangaugeCode == nil) {
        return nil;
    }

    if (![NSLocale.ISOLanguageCodes containsObject:regionFreeLangaugeCode]) {
        MPLogInfo(@"%@ is not a valid ISO 639-1 language code", regionFreeLangaugeCode);
        return nil;
    }

    // Replace the %%LANGUAGE%% macro in the url.
    NSString * url = [urlFormat stringByReplacingOccurrencesOfString:kMacroReplaceLanguageCode withString:regionFreeLangaugeCode];
    return (url != nil ? [NSURL URLWithString:url] : nil);
}

- (NSString * _Nullable)consentedIabVendorList {
    return [NSUserDefaults.standardUserDefaults stringForKey:kConsentedIabVendorListStorageKey];
}

- (NSString * _Nullable)consentedPrivacyPolicyVersion {
    return [NSUserDefaults.standardUserDefaults stringForKey:kConsentedPrivacyPolicyVersionStorageKey];
}

- (NSString * _Nullable)consentedVendorListVersion {
    return [NSUserDefaults.standardUserDefaults stringForKey:kConsentedVendorListVersionStorageKey];
}

- (MPConsentStatus)currentStatus {
    // Query the "do not track" state first, since "do not track" trumps all other states.
    // The cached consent state may be different than "do not track", and is preserved
    // to retain the previous consent status in the event that advertiser tracking is re-enabled.
    if (self.isDoNotTrack) {
        return MPConsentStatusDoNotTrack;
    }

    // Read the cached consent status. In the event that the key kConsentStatusKey
    // doesn't exist, integerForKey: will return a 0, which will translate into
    // MPConsentStatusUnknown.
    return (MPConsentStatus)[NSUserDefaults.standardUserDefaults integerForKey:kConsentStatusStorageKey];
}

- (NSString * _Nullable)extras {
    return [NSUserDefaults.standardUserDefaults stringForKey:kExtrasStorageKey];
}

- (NSString * _Nullable)iabVendorList {
    return [NSUserDefaults.standardUserDefaults stringForKey:kIabVendorListStorageKey];
}

- (NSString * _Nullable)iabVendorListHash {
    return [NSUserDefaults.standardUserDefaults stringForKey:kIabVendorListHashStorageKey];
}

- (NSString * _Nullable)ifaForConsent {
    return [NSUserDefaults.standardUserDefaults stringForKey:kIfaForConsentStorageKey];
}

- (MPBool)isGDPRApplicable {
    return (MPBool)[NSUserDefaults.standardUserDefaults integerForKey:kGDPRAppliesStorageKey];
}

- (BOOL)isWhitelisted {
    return [NSUserDefaults.standardUserDefaults boolForKey:kIsWhitelistedStorageKey];
}

- (NSString * _Nullable)lastChangedReason {
    // Query the "do not track" state first, since "do not track" trumps all other states.
    // The cached consent state may be different than "do not track", and is preserved
    // to retain the previous consent status in the event that advertiser tracking is re-enabled.
    // In this case, the last changed reason in a "do not track" state will always be
    // kConsentedChangedReasonDoNotTrackEnabled
    if (self.isDoNotTrack) {
        return kConsentedChangedReasonDoNotTrackEnabled;
    }

    return [NSUserDefaults.standardUserDefaults stringForKey:kLastChangedReasonStorageKey];
}

- (NSTimeInterval)lastChangedTimestampInMilliseconds {
    return (NSTimeInterval)[NSUserDefaults.standardUserDefaults doubleForKey:kLastChangedMsStorageKey];
}

- (NSString * _Nullable)lastSynchronizedStatus {
    return [NSUserDefaults.standardUserDefaults stringForKey:kLastSynchronizedConsentStatusStorageKey];
}

- (NSURL * _Nullable)privacyPolicyUrl {
    return [self privacyPolicyUrlWithISOLanguageCode:self.currentLanguageCode];
}

- (NSURL * _Nullable)privacyPolicyUrlWithISOLanguageCode:(NSString * _Nonnull)isoLanguageCode {
    NSString * urlFormat = [NSUserDefaults.standardUserDefaults stringForKey:kPrivacyPolicyUrlStorageKey];
    return [self urlWithFormat:urlFormat isoLanguageCode:isoLanguageCode];
}

- (NSString * _Nullable)privacyPolicyVersion {
    return [NSUserDefaults.standardUserDefaults stringForKey:kPrivacyPolicyVersionStorageKey];
}

- (NSURL *)vendorListUrl {
    return [self vendorListUrlWithISOLanguageCode:self.currentLanguageCode];
}

- (NSURL *)vendorListUrlWithISOLanguageCode:(NSString *)isoLanguageCode {
    NSString * urlFormat = [NSUserDefaults.standardUserDefaults stringForKey:kVendorListUrlStorageKey];
    return [self urlWithFormat:urlFormat isoLanguageCode:isoLanguageCode];
}

- (NSString * _Nullable)vendorListVersion {
    return [NSUserDefaults.standardUserDefaults stringForKey:kVendorListVersionStorageKey];
}

@end

@implementation MPConsentManager (PersonalDataHandler)

- (void)handlePersonalDataOnStateChangeTo:(MPConsentStatus)newStatus fromOldStatus:(MPConsentStatus)oldStatus {
    [self updateAppConversionTracking];

    if (oldStatus == MPConsentStatusConsented && newStatus != MPConsentStatusConsented) {
        [self synchronizeConsentWithCompletion:^(NSError * _Nullable error) {
            if (!error) {
                [self removeIfa];
            }
        }];
    }
}

- (void)storeIfa {
    NSString *identifier = [MPIdentityProvider identifierFromASIdentifierManager:NO];
    if (!identifier) {
        return;
    }
    NSString *storedIFA = [NSUserDefaults.standardUserDefaults stringForKey:kIfaForConsentStorageKey];
    if (![identifier isEqualToString:storedIFA]) {
        [NSUserDefaults.standardUserDefaults setObject:identifier forKey:kIfaForConsentStorageKey];
    }
}

/**
 * If IFA is changed and the status is transitioning from MPConsentStatusConsented, remove old IFA from NSUserDefault and change status to unknown.
 */
- (void)checkForIfaChange
{
    NSString *oldIfa = [NSUserDefaults.standardUserDefaults stringForKey:kIfaForConsentStorageKey];
    NSString *newIfa = [MPIdentityProvider identifierFromASIdentifierManager:NO];
    // IFA reset
    if (self.currentStatus == MPConsentStatusConsented && ![oldIfa isEqualToString:newIfa] && newIfa != nil) {
        [NSUserDefaults.standardUserDefaults removeObjectForKey:kLastSynchronizedConsentStatusStorageKey];
        [NSUserDefaults.standardUserDefaults removeObjectForKey:kIfaForConsentStorageKey];
        [self setCurrentStatus:MPConsentStatusUnknown reason:kConsentedChangedReasonIfaChanged shouldBroadcast:YES];
    }
}

- (void)removeIfa {
    [NSUserDefaults.standardUserDefaults removeObjectForKey:kIfaForConsentStorageKey];
}

/**
 * App conversion request will only be fired when MoPub obtains consent.
 */
- (void)updateAppConversionTracking {
    if ([MPConsentManager sharedManager].canCollectPersonalInfo) {
        NSString *appId = [NSUserDefaults.standardUserDefaults stringForKey:MOPUB_CONVERSION_APP_ID_KEY];
        BOOL hasAlreadyCheckedAppConversion = [NSUserDefaults.standardUserDefaults boolForKey:MOPUB_CONVERSION_DEFAULTS_KEY];
        if (!hasAlreadyCheckedAppConversion && appId.length > 0) {
            [[MPAdConversionTracker sharedConversionTracker] reportApplicationOpenForApplicationID:appId];
        }
    }
}

@end
