#import <Foundation/Foundation.h>

@interface AppInfo : NSObject

+ (instancetype _Nullable)sharedInfo;
- (instancetype _Nullable)init __attribute__((unavailable("init is not available")));

@property (nonatomic, readonly) NSString * countryCode;
@property (nonatomic, readonly) NSString * uniqueId;
@property (nonatomic, readonly) NSString * bundleVersion;
@property (nonatomic, readonly) NSUUID * advertisingId;
@property (nonatomic, readonly) NSString * languageId;

@end
