//
//  MOPUBPlayerManager.h
//  Copyright (c) 2015 MoPub. All rights reserved.
//

#import <Foundation/Foundation.h>

@class MOPUBPlayerViewController;
@class MOPUBNativeVideoAdConfigValues;
@class MPVideoConfig;

@interface MOPUBPlayerManager : NSObject

+ (MOPUBPlayerManager *)sharedInstance;
- (void)disposePlayerViewController;

- (MOPUBPlayerViewController *)playerViewControllerWithVideoConfig:(MPVideoConfig *)videoConfig nativeVideoAdConfig:(MOPUBNativeVideoAdConfigValues *)nativeVideoAdConfig;

@end
