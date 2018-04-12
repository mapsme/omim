typedef NS_ENUM(NSInteger, MWMSocialTokenType) {
  MWMSocialTokenTypeGoogle,
  MWMSocialTokenTypeFacebook,
  MWMSocialTokenTypePhone
};

typedef NS_ENUM(NSInteger, MWMAuthorizationSource) {
  MWMAuthorizationSourceUGC,
  MWMAuthorizationSourceBookmarks
};

typedef NS_ENUM(NSInteger, MWMAuthorizationError) {
  MWMAuthorizationErrorCancelled,
  MWMAuthorizationErrorPassportError
};

typedef void (^MWMAuthorizationCompleteBlock)(BOOL);

@interface MWMAuthorizationViewModel : NSObject

+ (NSURL *)phoneAuthURL;
+ (void)checkAuthenticationWithSource:(MWMAuthorizationSource)source
                           onComplete:(MWMAuthorizationCompleteBlock _Nonnull)onComplete;
+ (BOOL)hasSocialToken;
+ (BOOL)isAuthenticated;
+ (void)authenticateWithToken:(NSString * _Nonnull)token
                         type:(MWMSocialTokenType)type
                       source:(MWMAuthorizationSource)source
                   onComplete:(MWMAuthorizationCompleteBlock _Nonnull)onComplete;

@end
