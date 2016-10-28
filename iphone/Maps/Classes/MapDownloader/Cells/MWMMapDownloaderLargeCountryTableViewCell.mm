#import "Common.h"
#import "MWMMapDownloaderLargeCountryTableViewCell.h"

@interface MWMMapDownloaderLargeCountryTableViewCell ()

@property (weak, nonatomic) IBOutlet UILabel * mapsCount;

@end

@implementation MWMMapDownloaderLargeCountryTableViewCell

+ (CGFloat)estimatedHeight
{
  return 62.0;
}

#pragma mark - Config

- (void)config:(storage::NodeAttrs const &)nodeAttrs
{
  [super config:nodeAttrs];
  BOOL const haveLocalMaps = (nodeAttrs.m_downloadingMwmCounter != 0);
  NSString * ofMaps = haveLocalMaps ? [NSString stringWithFormat:L(@"downloader_of"), nodeAttrs.m_downloadingMwmCounter, nodeAttrs.m_mwmCounter] : @(nodeAttrs.m_mwmCounter).stringValue;
  self.mapsCount.text = [NSString stringWithFormat:@"%@: %@", L(@"downloader_status_maps"), ofMaps];
}

@end
