#import "OpeningHours.h"

#import "MWMOpeningHours.h"
#include "3party/opening_hours/opening_hours.hpp"

@interface WorkingDay ()

@property(nonatomic, copy) NSString *workingDays;
@property(nonatomic, copy) NSString *workingTimes;
@property(nonatomic, copy) NSString *breaks;
@property(nonatomic) BOOL isOpen;

@end

@implementation WorkingDay

@end

@interface OpeningHours ()

@property(nonatomic, strong) NSArray<WorkingDay *> *days;
@property(nonatomic) BOOL isClosedNow;

@end

@implementation OpeningHours

- (instancetype)initWithRawString:(NSString *)rawString localization:(id<IOpeningHoursLocalization>)localization {
  self = [super init];
  if (self) {
    osmoh::OpeningHours oh(rawString.UTF8String);
    _isClosedNow = oh.IsClosed(time(nullptr));
    auto days = osmoh::processRawString(rawString, localization);
    NSMutableArray *array = [NSMutableArray arrayWithCapacity:days.size()];
    for (auto day : days) {
      WorkingDay *wd = [[WorkingDay alloc] init];
      wd.isOpen = day.m_isOpen;
      wd.workingDays = day.m_workingDays;
      wd.workingTimes = day.m_isOpen ? day.m_workingTimes : localization.closedString;
      wd.breaks = day.m_breaks;
      [array addObject:wd];
    }
    if (array.count == 0) {
      return nil;
    }
    _days = [array copy];
  }
  return self;
}

@end
