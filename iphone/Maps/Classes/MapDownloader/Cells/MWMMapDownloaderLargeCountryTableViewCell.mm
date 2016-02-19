#import "MWMMapDownloaderLargeCountryTableViewCell.h"

@interface MWMMapDownloaderLargeCountryTableViewCell ()

@property (weak, nonatomic) IBOutlet UILabel * mapsCount;

@end

@implementation MWMMapDownloaderLargeCountryTableViewCell

- (void)layoutSubviews
{
  [super layoutSubviews];
  self.mapsCount.preferredMaxLayoutWidth = self.mapsCount.width;
  [super layoutSubviews];
}

- (void)setMapCountText:(NSString *)text
{
  self.mapsCount.text = text;
}

#pragma mark - Properties

- (CGFloat)estimatedHeight
{
  return 62.0;
}

@end
