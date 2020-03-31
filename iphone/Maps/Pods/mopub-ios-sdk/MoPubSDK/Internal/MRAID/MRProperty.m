//
//  MRProperty.m
//
//  Copyright 2018-2019 Twitter, Inc.
//  Licensed under the MoPub SDK License Agreement
//  http://www.mopub.com/legal/sdk-license-agreement/
//

#import "MRProperty.h"
#import "MPConstants.h"
#import "MPCoreInstanceProvider.h"

@implementation MRProperty

- (NSString *)description {
    return @"";
}

- (NSString *)jsonString {
    return @"{}";
}

@end

////////////////////////////////////////////////////////////////////////////////////////////////////

@implementation MRHostSDKVersionProperty : MRProperty

+ (instancetype)defaultProperty
{
    MRHostSDKVersionProperty *property = [[self alloc] init];
    property.version = MP_SDK_VERSION;
    return property;
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"hostSDKVersion: '%@'", self.version];
}

@end

////////////////////////////////////////////////////////////////////////////////////////////////////

@implementation MRPlacementTypeProperty : MRProperty

+ (MRPlacementTypeProperty *)propertyWithType:(MRAdViewPlacementType)type {
    MRPlacementTypeProperty *property = [[self alloc] init];
    property.placementType = type;
    return property;
}

- (NSString *)description {
    NSString *placementTypeString = @"unknown";
    switch (_placementType) {
        case MRAdViewPlacementTypeInline: placementTypeString = @"inline"; break;
        case MRAdViewPlacementTypeInterstitial: placementTypeString = @"interstitial"; break;
        default: break;
    }

    return [NSString stringWithFormat:@"placementType: '%@'", placementTypeString];
}

@end

////////////////////////////////////////////////////////////////////////////////////////////////////

@implementation MRStateProperty

+ (MRStateProperty *)propertyWithState:(MRAdViewState)state {
    MRStateProperty *property = [[self alloc] init];
    property.state = state;
    return property;
}

- (NSString *)description {
    NSString *stateString;
    switch (_state) {
        case MRAdViewStateHidden:
            stateString = @"hidden";
            break;
        case MRAdViewStateDefault:
            stateString = @"default";
            break;
        case MRAdViewStateExpanded:
            stateString = @"expanded";
            break;
        case MRAdViewStateResized:
            stateString = @"resized";
            break;
        default:
            stateString = @"loading";
            break;
    }
    return [NSString stringWithFormat:@"state: '%@'", stateString];
}

@end

////////////////////////////////////////////////////////////////////////////////////////////////////

@implementation MRScreenSizeProperty : MRProperty

+ (MRScreenSizeProperty *)propertyWithSize:(CGSize)size {
    MRScreenSizeProperty *property = [[self alloc] init];
    property.screenSize = size;
    return property;
}

- (NSString *)description {
    return [NSString stringWithFormat:@"screenSize: {width: %f, height: %f}",
            _screenSize.width,
            _screenSize.height];
}

@end

////////////////////////////////////////////////////////////////////////////////////////////////////

@implementation MRSupportsProperty : MRProperty

+ (NSDictionary *)supportedFeatures
{
    return [NSDictionary dictionaryWithObjectsAndKeys:
            [NSNumber numberWithBool:NO], @"sms",
            [NSNumber numberWithBool:NO], @"tel",
            [NSNumber numberWithBool:NO], @"calendar",
            [NSNumber numberWithBool:NO], @"storePicture",
            [NSNumber numberWithBool:YES], @"inlineVideo",
            nil];
}

+ (MRSupportsProperty *)defaultProperty
{
    return [self propertyWithSupportedFeaturesDictionary:[self supportedFeatures]];
}

+ (MRSupportsProperty *)propertyWithSupportedFeaturesDictionary:(NSDictionary *)dictionary
{
    MRSupportsProperty *property = [[self alloc] init];
    property.supportsSms = [[dictionary objectForKey:@"sms"] boolValue];
    property.supportsTel = [[dictionary objectForKey:@"tel"] boolValue];
    property.supportsCalendar = NO;
    property.supportsStorePicture = NO;
    property.supportsInlineVideo = [[dictionary objectForKey:@"inlineVideo"] boolValue];
    return property;
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"supports: {sms: %@, tel: %@, calendar: %@, storePicture: %@, inlineVideo: %@}",
            [self javascriptBooleanStringFromBoolValue:self.supportsSms],
            [self javascriptBooleanStringFromBoolValue:self.supportsTel],
            [self javascriptBooleanStringFromBoolValue:self.supportsCalendar],
            [self javascriptBooleanStringFromBoolValue:self.supportsStorePicture],
            [self javascriptBooleanStringFromBoolValue:self.supportsInlineVideo]];
}

- (NSString *)javascriptBooleanStringFromBoolValue:(BOOL)value
{
    return value ? @"true" : @"false";
}

@end

////////////////////////////////////////////////////////////////////////////////////////////////////

@implementation MRViewableProperty : MRProperty

+ (MRViewableProperty *)propertyWithViewable:(BOOL)viewable {
    MRViewableProperty *property = [[self alloc] init];
    property.isViewable = viewable;
    return property;
}

- (NSString *)description {
    return [NSString stringWithFormat:@"viewable: %@", _isViewable ? @"true" : @"false"];
}

@end
