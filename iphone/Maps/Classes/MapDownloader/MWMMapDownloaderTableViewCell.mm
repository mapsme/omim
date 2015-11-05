#import "MWMCircularProgress.h"
#import "MWMMapDownloaderTableViewCell.h"

@interface MWMMapDownloaderTableViewCell () <MWMCircularProgressDelegate>

@property (nonatomic) MWMCircularProgress * progressView;

@end

@implementation MWMMapDownloaderTableViewCell

#pragma mark - Properties

- (void)awakeFromNib
{
  [super awakeFromNib];
  self.progressView = [[MWMCircularProgress alloc] initWithParentView:self.stateWrapper delegate:self];
}

- (CGFloat)estimatedHeight
{
  return 52.0;
}

#pragma mark - MWMCircularProgressDelegate

- (void)progressButtonPressed:(nonnull MWMCircularProgress *)progress
{

}

@end
