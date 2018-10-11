#pragma once

#import <Foundation/Foundation.h>

#include "std/string.hpp"
#include "std/target_os.hpp"

namespace downloader { class IHttpThreadCallback; }

#ifdef OMIM_OS_IPHONE
#import "../iphone/Maps/Classes/DownloadIndicatorProtocol.h"
#endif

@interface HttpThread : NSObject<NSURLSessionDataDelegate>
{
  downloader::IHttpThreadCallback * m_callback;
  NSURLSessionDataTask * m_dataTask;
  int64_t m_begRange, m_endRange;
  int64_t m_downloadedBytes;
  int64_t m_expectedSize;
}

- (id) initWith:(string const &)url callback:(downloader::IHttpThreadCallback &)cb begRange:(int64_t)beg
       endRange:(int64_t)end expectedSize:(int64_t)size postBody:(string const &)pb;

- (void) cancel;

#ifdef OMIM_OS_IPHONE
+ (void)setDownloadIndicatorProtocol:(id<DownloadIndicatorProtocol>)indicator;
#endif

@end
