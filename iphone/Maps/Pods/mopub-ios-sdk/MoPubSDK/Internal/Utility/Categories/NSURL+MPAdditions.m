//
//  NSURL+MPAdditions.m
//
//  Copyright 2018-2019 Twitter, Inc.
//  Licensed under the MoPub SDK License Agreement
//  http://www.mopub.com/legal/sdk-license-agreement/
//

#import "NSURL+MPAdditions.h"

// Share Constants
static NSString * const kMoPubShareScheme = @"mopubshare";
static NSString * const kMoPubShareTweetHost = @"tweet";

// Commands Constants
static NSString * const kMoPubURLScheme = @"mopub";
static NSString * const kMoPubCloseHost = @"close";
static NSString * const kMoPubFinishLoadHost = @"finishLoad";
static NSString * const kMoPubFailLoadHost = @"failLoad";
static NSString * const kMoPubPrecacheCompleteHost = @"precacheComplete";
static NSString * const kMoPubRewardedVideoEndedHost = @"rewardedVideoEnded";

@implementation NSURL (MPAdditions)

- (NSString *)mp_queryParameterForKey:(NSString *)key
{
    NSArray *queryElements = [self.query componentsSeparatedByString:@"&"];
    for (NSString *element in queryElements) {
        NSArray *keyAndValue = [element componentsSeparatedByString:@"="];
        if (keyAndValue.count >= 2 &&
            [[keyAndValue objectAtIndex:0] isEqualToString:key] &&
            [[keyAndValue objectAtIndex:1] length] > 0) {
            return [[keyAndValue objectAtIndex:1] stringByRemovingPercentEncoding];
        }
    }
    return nil;
}

- (NSArray *)mp_queryParametersForKey:(NSString *)key
{
    NSMutableArray *matchingParameters = [NSMutableArray array];
    NSArray *queryElements = [self.query componentsSeparatedByString:@"&"];
    for (NSString *element in queryElements) {
        NSArray *keyAndValue = [element componentsSeparatedByString:@"="];
        if (keyAndValue.count >= 2 &&
            [[keyAndValue objectAtIndex:0] isEqualToString:key] &&
            [[keyAndValue objectAtIndex:1] length] > 0) {
            [matchingParameters addObject:[[keyAndValue objectAtIndex:1] stringByRemovingPercentEncoding]];
        }
    }
    return [NSArray arrayWithArray:matchingParameters];
}

- (NSDictionary *)mp_queryAsDictionary
{
    NSMutableDictionary *queryDict = [NSMutableDictionary dictionary];
    NSArray *queryElements = [self.query componentsSeparatedByString:@"&"];
    for (NSString *element in queryElements) {
        NSArray *keyVal = [element componentsSeparatedByString:@"="];
        if (keyVal.count >= 2) {
            NSString *key = [keyVal objectAtIndex:0];
            NSString *value = [keyVal objectAtIndex:1];
            [queryDict setObject:[value stringByRemovingPercentEncoding] forKey:key];
        }
    }
    return queryDict;
}

- (BOOL)mp_isSafeForLoadingWithoutUserAction
{
    return [[self scheme].lowercaseString isEqualToString:@"http"] ||
        [[self scheme].lowercaseString isEqualToString:@"https"] ||
        [[self scheme].lowercaseString isEqualToString:@"about"];
}

- (BOOL)mp_isMoPubScheme
{
    return [[self scheme] isEqualToString:kMoPubURLScheme];
}

- (MPMoPubShareHostCommand)mp_MoPubShareHostCommand
{
    NSString *host = [self host];
    if (![self mp_isMoPubShareScheme]) {
        return MPMoPubShareHostCommandUnrecognized;
    } else if ([host isEqualToString:kMoPubShareTweetHost]) {
        return MPMoPubShareHostCommandTweet;
    } else {
        return MPMoPubShareHostCommandUnrecognized;
    }
}

- (MPMoPubHostCommand)mp_mopubHostCommand
{
    NSString *host = [self host];
    if (![self mp_isMoPubScheme]) {
        return MPMoPubHostCommandUnrecognized;
    } else if ([host isEqualToString:kMoPubCloseHost]) {
        return MPMoPubHostCommandClose;
    } else if ([host isEqualToString:kMoPubFinishLoadHost]) {
        return MPMoPubHostCommandFinishLoad;
    } else if ([host isEqualToString:kMoPubFailLoadHost]) {
        return MPMoPubHostCommandFailLoad;
    } else if ([host isEqualToString:kMoPubPrecacheCompleteHost]) {
        return MPMoPubHostCommandPrecacheComplete;
    } else if ([host isEqualToString:kMoPubRewardedVideoEndedHost]) {
        return MPMoPubHostCommandRewardedVideoEnded;
    } else {
        return MPMoPubHostCommandUnrecognized;
    }
}

- (BOOL)mp_isMoPubShareScheme
{
    return [[self scheme] isEqualToString:kMoPubShareScheme];
}

@end
