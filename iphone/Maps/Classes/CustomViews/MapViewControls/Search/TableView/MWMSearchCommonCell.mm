#import "Common.h"
#import "LocationManager.h"
#import "MapsAppDelegate.h"
#import "MWMSearchCommonCell.h"
#import "UIColor+MapsMeColor.h"
#import "UIFont+MapsMeFonts.h"

#include "Framework.h"

#include "geometry/mercator.hpp"
#include "platform/measurement_utils.hpp"

@interface MWMSearchCommonCell ()

@property (weak, nonatomic) IBOutlet UILabel * typeLabel;
@property (weak, nonatomic) IBOutlet UIView * infoView;
@property (weak, nonatomic) IBOutlet UILabel * infoLabel;
@property (weak, nonatomic) IBOutlet UIView * infoRatingView;
@property (nonatomic) IBOutletCollection(UIImageView) NSArray * infoRatingStars;
@property (weak, nonatomic) IBOutlet UILabel * locationLabel;
@property (weak, nonatomic) IBOutlet UILabel * distanceLabel;
@property (weak, nonatomic) IBOutlet UIView * closedView;

@end

@implementation MWMSearchCommonCell

- (void)config:(search::Result &)result forHeight:(BOOL)forHeight
{
  [super config:result];
  self.typeLabel.text = @(result.GetFeatureType().c_str()).capitalizedString;
  search::AddressInfo const info = GetFramework().GetSearchResultAddress(result);
  self.locationLabel.text = @(info.FormatAddress(search::AddressInfo::SEARCH_RESULT).c_str());
  [self.locationLabel sizeToFit];

  if (!forHeight)
  {
    NSUInteger const starsCount = result.GetStarsCount();
    NSString * cuisine = @(result.GetCuisine().c_str());
    if (starsCount > 0)
      [self setInfoRating:starsCount];
    else if (cuisine.length > 0)
      [self setInfoText:cuisine.capitalizedString];
    else
      [self clearInfo];

    switch (result.IsOpenNow())
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
      double lat, lon;
      LocationManager * locationManager = MapsAppDelegate.theApp.m_locationManager;
      if ([locationManager getLat:lat Lon:lon])
      {
        m2::PointD const mercLoc = MercatorBounds::FromLatLon(lat, lon);
        double const dist = MercatorBounds::DistanceOnEarth(mercLoc, result.GetFeatureCenter());
        MeasurementUtils::FormatDistance(dist, distanceStr);
      }
      self.distanceLabel.text = @(distanceStr.c_str());
    }
  }
  if (isIOS7)
    [self layoutIfNeeded];
}

- (void)setInfoText:(NSString *)infoText
{
  self.infoView.hidden = NO;
  self.infoLabel.hidden = NO;
  self.infoRatingView.hidden = YES;
  self.infoLabel.text = infoText;
}

- (void)setInfoRating:(NSUInteger)infoRating
{
  self.infoView.hidden = NO;
  self.infoRatingView.hidden = NO;
  self.infoLabel.hidden = YES;
  [self.infoRatingStars enumerateObjectsUsingBlock:^(UIImageView * star, NSUInteger idx, BOOL *stop)
  {
    star.highlighted = star.tag <= infoRating;
  }];
}

- (void)clearInfo
{
  self.infoView.hidden = YES;
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
