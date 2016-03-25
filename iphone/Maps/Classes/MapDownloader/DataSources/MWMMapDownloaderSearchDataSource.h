#import "MWMMapDownloaderDataSource.h"

#include "storage/downloader_search_params.hpp"

@interface MWMMapDownloaderSearchDataSource : MWMMapDownloaderDataSource

- (instancetype _Nullable)initWithSearchResults:(DownloaderSearchResults const &)results delegate:(id<MWMMapDownloaderProtocol>)delegate;

@end
