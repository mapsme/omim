#import <Foundation/Foundation.h>

@interface HttpSessionManager : NSObject

- (instancetype) initWithConfiguration:(NSURLSessionConfiguration *)configuration;

- (NSURLSessionDataTask *) dataTaskWithRequest:(NSURLRequest *)request delegate:(id<NSURLSessionDataDelegate>)delegate completionHandler:(void (^)(NSData * data, NSURLResponse * response, NSError * error))completionHandler;

@end
