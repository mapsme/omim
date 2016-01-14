#import "MWMOpeningHoursAddScheduleTableViewCell.h"
#import "MWMOpeningHoursCommon.h"
#import "MWMOpeningHoursEditorViewController.h"

@interface MWMOpeningHoursAddScheduleTableViewCell ()

@property (weak, nonatomic) IBOutlet UIButton * addScheduleButton;

@end

@implementation MWMOpeningHoursAddScheduleTableViewCell

+ (CGFloat)height
{
  return 84.0;
}

- (void)refresh
{
  NSString * title = [NSString stringWithFormat:@"%@ %@", L(@"add_schedule_for"),
                      stringFromOpeningDays([self.model unhandledDays])];
  [self.addScheduleButton setTitle:title forState:UIControlStateNormal];
}

#pragma mark - Actions

- (IBAction)addScheduleTap
{
  [self.model addSchedule];
}

#pragma mark - Properties

- (void)setModel:(MWMOpeningHoursModel *)model
{
  _model = model;
  [self refresh];
}

@end
