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

@class FBSDKShareOpenGraphObject;
@class FBSDKSharePhoto;

/*!
 @abstract Protocol defining operations on open graph actions and objects.
 @discussion The property keys MUST have namespaces specified on them, such as `og:image`.
 */
@protocol FBSDKShareOpenGraphValueContaining <NSObject, NSSecureCoding>

/*!
 @abstract Gets an NSArray out of the receiver.
 @param key The key for the value
 @return The NSArray value or nil
 */
- (NSArray *)arrayForKey:(NSString *)key;

/*!
 @abstract Applies a given block object to the entries of the receiver.
 @param block A block object to operate on entries in the receiver
 */
- (void)enumerateKeysAndObjectsUsingBlock:(void (^)(NSString *key, id object, BOOL *stop))block;

/*!
 @abstract Returns an enumerator object that lets you access each key in the receiver.
 @return An enumerator object that lets you access each key in the receiver
 */
- (NSEnumerator *)keyEnumerator;

/*!
 @abstract Gets an NSNumber out of the receiver.
 @param key The key for the value
 @return The NSNumber value or nil
 */
- (NSNumber *)numberForKey:(NSString *)key;

/*!
 @abstract Returns an enumerator object that lets you access each value in the receiver.
 @return An enumerator object that lets you access each value in the receiver
 */
- (NSEnumerator *)objectEnumerator;

/*!
 @abstract Gets an FBSDKShareOpenGraphObject out of the receiver.
 @param key The key for the value
 @return The FBSDKShareOpenGraphObject value or nil
 */
- (FBSDKShareOpenGraphObject *)objectForKey:(NSString *)key;

/*!
 @abstract Enables subscript access to the values in the receiver.
 @param key The key for the value
 @return The value
 */
- (id)objectForKeyedSubscript:(NSString *)key;

/*!
 @abstract Parses properties out of a dictionary into the receiver.
 @param properties The properties to parse.
 */
- (void)parseProperties:(NSDictionary *)properties;

/*!
 @abstract Gets an FBSDKSharePhoto out of the receiver.
 @param key The key for the value
 @return The FBSDKSharePhoto value or nil
 */
- (FBSDKSharePhoto *)photoForKey:(NSString *)key;

/*!
 @abstract Removes a value from the receiver for the specified key.
 @param key The key for the value
 */
- (void)removeObjectForKey:(NSString *)key;

/*!
 @abstract Sets an NSArray on the receiver.
 @discussion This method will throw if the array contains any values that is not an NSNumber, NSString, NSURL,
 FBSDKSharePhoto or FBSDKShareOpenGraphObject.
 @param array The NSArray value
 @param key The key for the value
 */
- (void)setArray:(NSArray *)array forKey:(NSString *)key;

/*!
 @abstract Sets an NSNumber on the receiver.
 @param number The NSNumber value
 @param key The key for the value
 */
- (void)setNumber:(NSNumber *)number forKey:(NSString *)key;

/*!
 @abstract Sets an FBSDKShareOpenGraphObject on the receiver.
 @param object The FBSDKShareOpenGraphObject value
 @param key The key for the value
 */
- (void)setObject:(FBSDKShareOpenGraphObject *)object forKey:(NSString *)key;

/*!
 @abstract Sets an FBSDKSharePhoto on the receiver.
 @param photo The FBSDKSharePhoto value
 @param key The key for the value
 */
- (void)setPhoto:(FBSDKSharePhoto *)photo forKey:(NSString *)key;

/*!
 @abstract Sets an NSString on the receiver.
 @param string The NSString value
 @param key The key for the value
 */
- (void)setString:(NSString *)string forKey:(NSString *)key;

/*!
 @abstract Sets an NSURL on the receiver.
 @param URL The NSURL value
 @param key The key for the value
 */
- (void)setURL:(NSURL *)URL forKey:(NSString *)key;

/*!
 @abstract Gets an NSString out of the receiver.
 @param key The key for the value
 @return The NSString value or nil
 */
- (NSString *)stringForKey:(NSString *)key;

/*!
 @abstract Gets an NSURL out of the receiver.
 @param key The key for the value
 @return The NSURL value or nil
 */
- (NSURL *)URLForKey:(NSString *)key;

@end

/*!
 @abstract A base class to container Open Graph values.
 */
@interface FBSDKShareOpenGraphValueContainer : NSObject <FBSDKShareOpenGraphValueContaining>

@end
