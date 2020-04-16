#import <Foundation/Foundation.h>

#import "MWMTypes.h"

typedef NS_ENUM(NSInteger, MWMSocialTokenType) {
  MWMSocialTokenTypeGoogle,
  MWMSocialTokenTypeFacebook,
  MWMSocialTokenTypePhone,
  MWMSocialTokenTypeApple
} NS_SWIFT_NAME(SocialTokenType);

typedef NS_ENUM(NSInteger, MWMAuthorizationSource) {
  MWMAuthorizationSourceUGC,
  MWMAuthorizationSourceBookmarks
} NS_SWIFT_NAME(AuthorizationSource);

NS_ASSUME_NONNULL_BEGIN

NS_SWIFT_NAME(User)
@interface MWMUser : NSObject

+ (nullable NSURL *)phoneAuthURL;
+ (BOOL)isAuthenticated;
+ (NSString *)termsOfUseLink;
+ (NSString *)privacyPolicyLink;
+ (void)logOut;

+ (void)authenticateWithToken:(NSString *)token
                         type:(MWMSocialTokenType)type
              privacyAccepted:(BOOL)privacyAccepted
                termsAccepted:(BOOL)termsAccepted
                promoAccepted:(BOOL)promoAccepted
                    firstName:(NSString *)firstName
                     lastName:(NSString *)lastName
                       source:(MWMAuthorizationSource)source
                   onComplete:(MWMBoolBlock)onComplete;

@end

NS_ASSUME_NONNULL_END
