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

#import <Foundation/Foundation.h>

#import <FBSDKCoreKit/FBSDKCopying.h>

#import <FBSDKShareKit/FBSDKShareOpenGraphObject.h>
#import <FBSDKShareKit/FBSDKShareOpenGraphValueContainer.h>

/*!
 @abstract An Open Graph Action for sharing.
 @discussion The property keys MUST have namespaces specified on them, such as `og:image`.
 */
@interface FBSDKShareOpenGraphAction : FBSDKShareOpenGraphValueContainer <FBSDKCopying, NSSecureCoding>

/*!
 @abstract Convenience method to build a new action and set the object for the specified key.
 @param actionType The action type of the receiver
 @param object The Open Graph object represented by this action
 @param key The key for the object
 */
+ (instancetype)actionWithType:(NSString *)actionType object:(FBSDKShareOpenGraphObject *)object key:(NSString *)key;

/*!
 @abstract Convenience method to build a new action and set the object for the specified key.
 @param actionType The action type of the receiver
 @param objectID The ID of an existing Open Graph object
 @param key The key for the object
 */
+ (instancetype)actionWithType:(NSString *)actionType objectID:(NSString *)objectID key:(NSString *)key;

/*!
 @abstract Convenience method to build a new action and set the object for the specified key.
 @param actionType The action type of the receiver
 @param objectURL The URL to a page that defines the Open Graph object with meta tags
 @param key The key for the object
 */
+ (instancetype)actionWithType:(NSString *)actionType objectURL:(NSURL *)objectURL key:(NSString *)key;

/*!
 @abstract Gets the action type.
 @return The action type
 */
@property (nonatomic, copy) NSString *actionType;

/*!
 @abstract Compares the receiver to another Open Graph Action.
 @param action The other action
 @return YES if the receiver's values are equal to the other action's values; otherwise NO
 */
- (BOOL)isEqualToShareOpenGraphAction:(FBSDKShareOpenGraphAction *)action;

@end
