#import "MWMNavigationDashboardEntity.h"
#import "MWMCoreUnits.h"
#import "MWMLocationManager.h"
#import "MWMSettings.h"
#import "SwiftBridge.h"

#include "platform/measurement_utils.hpp"

@interface MWMNavigationDashboardEntity ()

@property(copy, nonatomic, readwrite) NSAttributedString * estimate;
@property(copy, nonatomic, readwrite) NSAttributedString * estimateDot;
@property(copy, nonatomic, readwrite) NSString * distanceToTurn;
@property(copy, nonatomic, readwrite) NSString * streetName;
@property(copy, nonatomic, readwrite) NSString * targetDistance;
@property(copy, nonatomic, readwrite) NSString * targetUnits;
@property(copy, nonatomic, readwrite) NSString * turnUnits;
@property(nonatomic, readwrite) BOOL isValid;
@property(nonatomic, readwrite) CGFloat progress;
@property(nonatomic, readwrite) CLLocation * pedestrianDirectionPosition;
@property(nonatomic, readwrite) NSUInteger roundExitNumber;
@property(nonatomic, readwrite) NSUInteger timeToTarget;
@property(nonatomic, readwrite) UIImage * nextTurnImage;
@property(nonatomic, readwrite) UIImage * turnImage;

@end

@implementation MWMNavigationDashboardEntity

- (NSString *)speed
{
  CLLocation * lastLocation = [MWMLocationManager lastLocation];
  if (!lastLocation || lastLocation.speed < 0)
    return nil;
  auto const units = coreUnits([MWMSettings measurementUnits]);
  return @(measurement_utils::FormatSpeed(lastLocation.speed, units).c_str());
}

- (NSString *)speedUnits
{
  auto const units = coreUnits([MWMSettings measurementUnits]);
  return @(measurement_utils::FormatSpeedUnits(units).c_str());
}

- (NSString *)eta { return [NSDateComponentsFormatter etaStringFrom:self.timeToTarget]; }
- (NSString *)arrival
{
  auto arrivalDate = [[NSDate date] dateByAddingTimeInterval:self.timeToTarget];
  return [NSDateFormatter localizedStringFromDate:arrivalDate
                                        dateStyle:NSDateFormatterNoStyle
                                        timeStyle:NSDateFormatterShortStyle];
}

- (NSAttributedString *)estimateDot
{
  if (!_estimateDot)
  {
    auto attributes = @{NSForegroundColorAttributeName : [UIColor blackSecondaryText],
                        NSFontAttributeName : [UIFont medium17]};
    _estimateDot = [[NSAttributedString alloc] initWithString:@" • " attributes:attributes];
  }
  return _estimateDot;
}

@end
