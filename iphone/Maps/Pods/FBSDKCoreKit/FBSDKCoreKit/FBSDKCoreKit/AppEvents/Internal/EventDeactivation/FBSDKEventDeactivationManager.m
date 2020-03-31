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

#import "FBSDKEventDeactivationManager.h"

static NSString *const DEPRECATED_PARAM_KEY = @"deprecated_param";
static NSString *const DEPRECATED_EVENT_KEY = @"is_deprecated_event";

@interface FBSDKDeactivatedEvent : NSObject

@property (nonatomic, readonly, copy) NSString *eventName;
@property (nonatomic, readonly, copy, nullable) NSSet<NSString *> *deactivatedParams;

-(instancetype)initWithEventName:(NSString *)eventName
               deactivatedParams:(NSSet<NSString *> *)deactivatedParams;

@end

@implementation FBSDKDeactivatedEvent

-(instancetype)initWithEventName:(NSString *)eventName
               deactivatedParams:(NSSet<NSString *> *)deactivatedParams
{
  self = [super init];
  if (self) {
    _eventName = eventName;
    _deactivatedParams = deactivatedParams;
  }

  return self;
}

@end

@implementation FBSDKEventDeactivationManager

static BOOL isEventDeactivationEnabled = NO;

static NSMutableSet<NSString *> *_deactivatedEvents;
static NSMutableArray<FBSDKDeactivatedEvent *>  *_eventsWithDeactivatedParams;

+ (void)enable
{
  isEventDeactivationEnabled = YES;
}

+ (void)updateDeactivatedEvents:(nullable NSDictionary<NSString *, id> *)events
{
  if (!isEventDeactivationEnabled || events.count == 0) {
    return;
  }
  [_deactivatedEvents removeAllObjects];
  [_eventsWithDeactivatedParams removeAllObjects];
  NSMutableArray<FBSDKDeactivatedEvent *> *deactivatedParamsArray = [NSMutableArray array];
  NSMutableSet<NSString *> *deactivatedEventSet = [NSMutableSet set];
  for (NSString *eventName in events.allKeys) {
    NSDictionary<NSString *, id> *eventInfo = events[eventName];
    if (!eventInfo) {
      return;
    }
    if (eventInfo[DEPRECATED_EVENT_KEY]) {
      [deactivatedEventSet addObject:eventName];
    }
    if (eventInfo[DEPRECATED_PARAM_KEY]) {
      FBSDKDeactivatedEvent *eventWithDeactivatedParams = [[FBSDKDeactivatedEvent alloc] initWithEventName:eventName
                                                                                         deactivatedParams:[NSSet setWithArray:eventInfo[DEPRECATED_PARAM_KEY]]];
      [deactivatedParamsArray addObject:eventWithDeactivatedParams];
    }
  }
  _deactivatedEvents = deactivatedEventSet;
  _eventsWithDeactivatedParams = deactivatedParamsArray;
}

+ (void)processEvents:(NSMutableArray<NSDictionary<NSString *, id> *> *)events
{
  if (!isEventDeactivationEnabled) {
    return;
  }
  NSArray<NSDictionary<NSString *, id> *> *eventArray = [events copy];
  for (NSDictionary<NSString *, NSDictionary<NSString *, id> *> *event in eventArray) {
    if ([_deactivatedEvents containsObject:event[@"event"][@"_eventName"]]) {
      [events removeObject:event];
    }
  }
}

+ (nullable NSDictionary<NSString *, id> *)processParameters:(nullable NSDictionary<NSString *, id> *)parameters
                                                   eventName:(NSString *)eventName
{
  if (!isEventDeactivationEnabled || parameters.count == 0 || _eventsWithDeactivatedParams.count == 0) {
     return parameters;
   }
  NSMutableDictionary<NSString *, id> *params = [NSMutableDictionary dictionaryWithDictionary:parameters];
  for (NSString *key in [parameters keyEnumerator]) {
    for (FBSDKDeactivatedEvent *event in _eventsWithDeactivatedParams) {
      if ([event.eventName isEqualToString:eventName] && [event.deactivatedParams containsObject:key]) {
        [params removeObjectForKey:key];
      }
    }
  }
  return [params copy];
}

@end
