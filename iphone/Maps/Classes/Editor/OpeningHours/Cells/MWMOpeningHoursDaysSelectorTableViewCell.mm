#import "MWMOpeningHoursDaysSelectorTableViewCell.h"
#import "UIColor+MapsMeColor.h"

#include "3party/opening_hours/opening_hours.hpp"

@interface MWMOpeningHoursDaysSelectorTableViewCell ()

@property (strong, nonatomic) IBOutletCollection(UIButton) NSArray * buttons;
@property (strong, nonatomic) IBOutletCollection(UILabel) NSArray * labels;
@property (strong, nonatomic) IBOutletCollection(UIImageView) NSArray * images;

@property (nonatomic) NSUInteger firstWeekday;

@end

using namespace osmoh;

@implementation MWMOpeningHoursDaysSelectorTableViewCell

+ (CGFloat)heightForWidth:(CGFloat)width
{
  return 76.0;
}

- (void)awakeFromNib
{
  [super awakeFromNib];
  NSCalendar * cal = [NSCalendar currentCalendar];
  cal.locale = [NSLocale currentLocale];
  self.firstWeekday = [cal firstWeekday];
  for (UILabel * label in self.labels)
  {
    switch ([self tag2Weekday:label.tag])
    {
      case Weekday::Sunday:
        label.text = L(@"su");
        break;
      case Weekday::Monday:
        label.text = L(@"mo");
        break;
      case Weekday::Tuesday:
        label.text = L(@"tu");
        break;
      case Weekday::Wednesday:
        label.text = L(@"we");
        break;
      case Weekday::Thursday:
        label.text = L(@"th");
        break;
      case Weekday::Friday:
        label.text = L(@"fr");
        break;
      case Weekday::Saturday:
        label.text = L(@"sa");
        break;
      default:
        NSAssert(false, @"Invalid case");
        break;
    }
  }
}

- (Weekday)tag2Weekday:(NSUInteger)tag
{
  NSUInteger idx = tag + self.firstWeekday;
  if (idx > static_cast<NSUInteger>(Weekday::Saturday))
    idx -= static_cast<NSUInteger>(Weekday::Saturday);
  return static_cast<Weekday>(idx);
}

- (void)makeDay:(NSUInteger)tag selected:(BOOL)selected refresh:(BOOL)refresh
{
  if (refresh)
  {
    Weekday const wd = [self tag2Weekday:tag];
    if (selected)
      [self.section addSelectedDay:wd];
    else
      [self.section removeSelectedDay:wd];
  }
  for (UIButton * btn in self.buttons)
  {
    if (btn.tag == tag)
      btn.selected = selected;
  }
  for (UILabel * label in self.labels)
  {
    if (label.tag == tag)
      label.textColor = (selected ? [UIColor blackPrimaryText] : [UIColor blackHintText]);
  }
  for (UIImageView * image in self.images)
  {
    if (image.tag == tag)
      image.highlighted = selected;
  }
}

- (void)refresh
{
  [super refresh];
  for (UILabel * label in self.labels)
  {
    NSUInteger const tag = label.tag;
    BOOL const selected = [self.section containsSelectedDay:[self tag2Weekday:tag]];
    [self makeDay:tag selected:selected refresh:NO];
  }
}

#pragma mark - Actions

- (IBAction)selectDay:(UIButton *)sender
{
  [self makeDay:sender.tag selected:!sender.isSelected refresh:YES];
}

@end
