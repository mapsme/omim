#import "MWMMapDownloaderDataSource.h"

@interface MWMMapDownloaderDefaultDataSource : MWMMapDownloaderDataSource

- (instancetype _Nullable)initForRootCountryId:(NSString *)countryId delegate:(id<MWMMapDownloaderProtocol>)delegate;
- (void)reload;

@end
