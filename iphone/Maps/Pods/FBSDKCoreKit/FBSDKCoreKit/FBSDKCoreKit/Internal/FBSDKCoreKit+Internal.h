// Copyright (c) 2014-present, Facebook, Inc. All rights reserved.
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

#if SWIFT_PACKAGE
#import "FBSDKCoreKit.h"
#else
#import <FBSDKCoreKit/FBSDKCoreKit.h>
#endif

#if defined FBSDKCOCOAPODS || defined BUCK

#if !TARGET_OS_TV
#import "FBSDKViewHierarchy.h"
#import "FBSDKViewHierarchyMacros.h"
#import "FBSDKCodelessIndexer.h"
#import "FBSDKMetadataIndexer.h"
#import "FBSDKSuggestedEventsIndexer.h"
#import "FBSDKCrypto.h"
#import "FBSDKAudioResourceLoader.h"
#import "FBSDKContainerViewController.h"
#import "FBSDKBridgeAPI.h"
#import "FBSDKMonotonicTime.h"
#import "FBSDKTriStateBOOL.h"
#import "FBSDKCloseIcon.h"
#import "FBSDKColor.h"
#import "FBSDKMaleSilhouetteIcon.h"
#import "FBSDKUIUtility.h"
#import "FBSDKViewImpressionTracker.h"
#import "FBSDKWebDialog.h"
#else
#import "FBSDKDeviceButton+Internal.h"
#import "FBSDKDeviceDialogView.h"
#import "FBSDKSmartDeviceDialogView.h"
#import "FBSDKDeviceViewControllerBase+Internal.h"
#import "FBSDKModalFormPresentationController.h"
#endif

#import "FBSDKAppEvents+Internal.h"
#import "FBSDKAppEventsState.h"
#import "FBSDKAppEventsStateManager.h"
#import "FBSDKAppEventsUtility.h"
#import "FBSDKRestrictiveDataFilterManager.h"
#import "FBSDKTimeSpentData.h"
#import "FBSDKUserDataStore.h"
#import "FBSDKBase64.h"
#import "FBSDKErrorRecoveryAttempter.h"
#import "FBSDKDynamicFrameworkLoader.h"
#import "FBSDKApplicationObserving.h"
#import "FBSDKApplicationDelegate+Internal.h"
#import "FBSDKDeviceRequestsHelper.h"
#import "FBSDKError.h"
#import "FBSDKImageDownloader.h"
#import "FBSDKInternalUtility.h"
#import "FBSDKLogger.h"
#import "FBSDKMath.h"
#import "FBSDKSettings+Internal.h"
#import "FBSDKSwizzler.h"
#import "FBSDKTypeUtility.h"
#import "FBSDKBasicUtility.h"
#import "FBSDKURLSession.h"
#import "FBSDKURLSessionTask.h"
#import "FBSDKCrashHandler.h"
#import "FBSDKLibAnalyzer.h"
#import "FBSDKCrashObserving.h"
#import "FBSDKGraphRequest+Internal.h"
#import "FBSDKGraphRequestConnection+Internal.h"
#import "FBSDKGraphRequestMetadata.h"
#import "FBSDKDialogConfiguration.h"
#import "FBSDKServerConfiguration+Internal.h"
#import "FBSDKServerConfiguration.h"
#import "FBSDKServerConfigurationManager+Internal.h"
#import "FBSDKServerConfigurationManager.h"
#import "FBSDKGateKeeperManager.h"
#import "FBSDKAccessTokenCache.h"
#import "FBSDKAccessTokenCaching.h"
#import "FBSDKKeychainStore.h"
#import "FBSDKKeychainStoreViaBundleID.h"
#import "FBSDKButton+Subclass.h"
#import "FBSDKIcon.h"
#import "FBSDKLogo.h"

#else

#if !TARGET_OS_TV
#import "../AppEvents/Internal/ViewHierarchy/FBSDKViewHierarchy.h"
#import "../AppEvents/Internal/ViewHierarchy/FBSDKViewHierarchyMacros.h"
#import "../AppEvents/Internal/Codeless/FBSDKCodelessIndexer.h"
#import "../AppEvents/Internal/AAM/FBSDKMetadataIndexer.h"
#import "../AppEvents/Internal/SuggestedEvents/FBSDKSuggestedEventsIndexer.h"
#import "Cryptography/FBSDKCrypto.h"
#import "FBSDKAudioResourceLoader.h"
#import "FBSDKContainerViewController.h"
#import "BridgeAPI/FBSDKBridgeAPI.h"
#import "FBSDKMonotonicTime.h"
#import "FBSDKTriStateBOOL.h"
#import "UI/FBSDKCloseIcon.h"
#import "UI/FBSDKColor.h"
#import "UI/FBSDKMaleSilhouetteIcon.h"
#import "UI/FBSDKUIUtility.h"
#import "UI/FBSDKViewImpressionTracker.h"
#import "WebDialog/FBSDKWebDialog.h"
#else
#import "Device/FBSDKDeviceButton+Internal.h"
#import "Device/FBSDKDeviceDialogView.h"
#import "Device/FBSDKSmartDeviceDialogView.h"
#import "Device/FBSDKDeviceViewControllerBase+Internal.h"
#import "Device/FBSDKModalFormPresentationController.h"
#endif

#import "../AppEvents/Internal/FBSDKAppEvents+Internal.h"
#import "../AppEvents/Internal/FBSDKAppEventsState.h"
#import "../AppEvents/Internal/FBSDKAppEventsStateManager.h"
#import "../AppEvents/Internal/FBSDKAppEventsUtility.h"
#import "../AppEvents/Internal/RestrictiveDataFilter/FBSDKRestrictiveDataFilterManager.h"
#import "../AppEvents/Internal/FBSDKTimeSpentData.h"
#import "../AppEvents/Internal/FBSDKUserDataStore.h"
#import "Base64/FBSDKBase64.h"
#import "ErrorRecovery/FBSDKErrorRecoveryAttempter.h"
#import "FBSDKDynamicFrameworkLoader.h"
#import "FBSDKApplicationObserving.h"
#import "FBSDKApplicationDelegate+Internal.h"
#import "FBSDKDeviceRequestsHelper.h"
#import "FBSDKError.h"
#import "FBSDKImageDownloader.h"
#import "FBSDKInternalUtility.h"
#import "FBSDKLogger.h"
#import "FBSDKMath.h"
#import "FBSDKSettings+Internal.h"
#import "FBSDKSwizzler.h"
#import "../Basics/Internal/FBSDKTypeUtility.h"
#import "../Basics/Internal/FBSDKBasicUtility.h"
#import "../Basics/Internal/FBSDKURLSession.h"
#import "../Basics/Internal/FBSDKURLSessionTask.h"
#import "../Basics/Instrument/FBSDKCrashHandler.h"
#import "../Basics/Instrument/FBSDKLibAnalyzer.h"
#import "../Basics/Instrument/FBSDKCrashObserving.h"
#import "Network/FBSDKGraphRequest+Internal.h"
#import "Network/FBSDKGraphRequestConnection+Internal.h"
#import "Network/FBSDKGraphRequestMetadata.h"
#import "ServerConfiguration/FBSDKDialogConfiguration.h"
#import "ServerConfiguration/FBSDKServerConfiguration+Internal.h"
#import "ServerConfiguration/FBSDKServerConfiguration.h"
#import "ServerConfiguration/FBSDKServerConfigurationManager+Internal.h"
#import "ServerConfiguration/FBSDKServerConfigurationManager.h"
#import "ServerConfiguration/FBSDKGateKeeperManager.h"
#import "TokenCaching/FBSDKAccessTokenCache.h"
#import "TokenCaching/FBSDKAccessTokenCaching.h"
#import "TokenCaching/FBSDKKeychainStore.h"
#import "TokenCaching/FBSDKKeychainStoreViaBundleID.h"
#import "UI/FBSDKButton+Subclass.h"
#import "UI/FBSDKIcon.h"
#import "UI/FBSDKLogo.h"

#endif
