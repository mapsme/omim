#import "Common.h"
#import "MapsAppDelegate.h"
#import "MWMLocationManager.h"
#import "MWMSearchCommonCell.h"
#import "UIColor+MapsMeColor.h"
#import "UIFont+MapsMeFonts.h"

#include "Framework.h"

#include "geometry/mercator.hpp"
#include "platform/measurement_utils.hpp"

@interface MWMSearchCommonCell ()

@property (weak, nonatomic) IBOutlet UILabel * typeLabel;
@property (weak, nonatomic) IBOutlet UILabel * locationLabel;
@property (weak, nonatomic) IBOutlet UILabel * distanceLabel;
@property (weak, nonatomic) IBOutlet UIView * closedView;

@end

@implementation MWMSearchCommonCell

- (void)config:(search::Result const &)result forHeight:(BOOL)forHeight
{
  [super config:result];
  auto const & pd = result.GetPlaceData();
  self.typeLabel.text = @(pd.GetSubtitle().c_str()).capitalizedString;
  self.locationLabel.text = @(result.GetAddress().c_str());
  [self.locationLabel sizeToFit];

  if (!forHeight)
  {
    switch (pd.IsOpen())
    {
      case osm::Unknown:
      // TODO: Correctly handle Open Now = YES value (show "OPEN" mark).
      case osm::Yes:
        self.closedView.hidden = YES;
        break;
      case osm::No:
        self.closedView.hidden = NO;
        break;
    }
    if (result.HasPoint())
    {
      string distanceStr;
      CLLocation * lastLocation = [MWMLocationManager lastLocation];
      if (lastLocation)
      {
        double const dist = MercatorBounds::DistanceOnEarth(lastLocation.mercator, result.GetFeatureCenter());
        measurement_utils::FormatDistance(dist, distanceStr);
      }
      self.distanceLabel.text = @(distanceStr.c_str());
    }
  }
  if (isIOS7)
    [self layoutIfNeeded];
}

- (void)layoutSubviews
{
  [super layoutSubviews];
  if (isIOS7)
  {
    self.typeLabel.preferredMaxLayoutWidth = floor(self.typeLabel.width);
    self.locationLabel.preferredMaxLayoutWidth = floor(self.locationLabel.width);
    self.distanceLabel.preferredMaxLayoutWidth = floor(self.distanceLabel.width);
    [super layoutSubviews];
  }
}

- (NSDictionary *)selectedTitleAttributes
{
  return @{
    NSForegroundColorAttributeName : UIColor.blackPrimaryText,
    NSFontAttributeName : UIFont.bold17
  };
}

- (NSDictionary *)unselectedTitleAttributes
{
  return @{
    NSForegroundColorAttributeName : UIColor.blackPrimaryText,
    NSFontAttributeName : UIFont.regular17
  };
}

+ (CGFloat)defaultCellHeight
{
  return 80.0;
}

- (CGFloat)cellHeight
{
  return ceil([self.contentView systemLayoutSizeFittingSize:UILayoutFittingCompressedSize].height);
}

@end
