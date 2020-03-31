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

#import "TargetConditionals.h"

#if !TARGET_OS_TV

#import "FBSDKLoginCompletion.h"

@interface FBSDKLoginCompletionParameters ()

@property (nonatomic, copy) NSString *accessTokenString;
@property (nonatomic, copy) NSString *nonceString;

@property (nonatomic, copy) NSSet *permissions;
@property (nonatomic, copy) NSSet *declinedPermissions;
@property (nonatomic, copy) NSSet *expiredPermissions;

@property (nonatomic, copy) NSString *appID;
@property (nonatomic, copy) NSString *userID;

@property (nonatomic, copy) NSError *error;

@property (nonatomic, copy) NSDate *expirationDate;
@property (nonatomic, copy) NSDate *dataAccessExpirationDate;

@property (nonatomic, copy) NSString *challenge;

@property (nonatomic, copy) NSString *graphDomain;

@end

#endif
