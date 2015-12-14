#import "MWMPlacePageButtonCell.h"
#import "Statistics.h"
#import "MWMPlacePage.h"

@interface MWMPlacePageButtonCell ()

@property (weak, nonatomic) MWMPlacePage * placePage;

@end

@implementation MWMPlacePageButtonCell

+ (CGFloat)height
{
  return 44.0;
}

- (void)config:(MWMPlacePage *)placePage
{
  self.placePage = placePage;
}

- (IBAction)editPlaceButtonTouchUpIndide
{
  [[Statistics instance] logEvent:kStatEventName(kStatPlacePage, kStatEdit)];
  [self.placePage editPlace];
}

@end
