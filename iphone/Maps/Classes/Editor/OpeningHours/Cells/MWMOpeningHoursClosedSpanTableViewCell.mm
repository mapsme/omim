#import "MWMOpeningHoursClosedSpanTableViewCell.h"
#import "UIFont+MapsMeFonts.h"
#import "UIColor+MapsMeColor.h"

static NSString * const kLabelText = L(@"hours_closed");

CGFloat labelWidth()
{
  UILabel * label = [[UILabel alloc] initWithFrame:CGRectZero];
  label.font = [UIFont regular17];
  label.text = kLabelText;
  [label sizeToFit];
  return label.width;
}

static CGFloat const kLabelWidth = labelWidth();

BOOL isCompactForCellWidth(CGFloat width)
{
  CGFloat const widthOfAllElementsWithoutLabel = 234.0;
  CGFloat const maxLabelWidthForCompactCell = width - widthOfAllElementsWithoutLabel;
  return kLabelWidth < maxLabelWidthForCompactCell;
}

@interface MWMOpeningHoursClosedSpanTableViewCell ()

@property (weak, nonatomic) IBOutlet UILabel * hoursClosedLabel;
@property (weak, nonatomic) IBOutlet UILabel * timeSpanLabel;

@property (weak, nonatomic) IBOutlet NSLayoutConstraint * timeSpanHorizontalAlignment;
@property (weak, nonatomic) IBOutlet NSLayoutConstraint * timeSpanVerticalAlignment;
@property (weak, nonatomic) IBOutlet NSLayoutConstraint * hoursClosedTrailing;

@property (nonatomic) BOOL isCompact;

@end

@implementation MWMOpeningHoursClosedSpanTableViewCell

+ (CGFloat)heightForWidth:(CGFloat)width
{
  return isCompactForCellWidth(width) ? 44.0 : 64.0;
}

- (void)awakeFromNib
{
  [super awakeFromNib];
  self.hoursClosedLabel.text = kLabelText;
  self.isCompact = YES;
}

- (void)layoutSubviews
{
  if (self.isCompact != isCompactForCellWidth(self.width))
  {
    self.isCompact = !self.isCompact;
    if (self.isCompact)
    {
      self.hoursClosedLabel.font = [UIFont regular17];
      self.timeSpanLabel.textAlignment = NSTextAlignmentRight;
      self.timeSpanHorizontalAlignment.priority = UILayoutPriorityDefaultLow;
      self.timeSpanVerticalAlignment.priority = UILayoutPriorityDefaultLow;
      self.hoursClosedTrailing.priority = UILayoutPriorityDefaultLow;
    }
    else
    {
      self.hoursClosedLabel.font = [UIFont regular14];
      self.timeSpanLabel.textAlignment = NSTextAlignmentLeft;
      self.timeSpanHorizontalAlignment.priority = UILayoutPriorityDefaultHigh;
      self.timeSpanVerticalAlignment.priority = UILayoutPriorityDefaultHigh;
      self.hoursClosedTrailing.priority = UILayoutPriorityDefaultHigh;
    }
  }
  [super layoutSubviews];
}

- (void)refresh
{
  [super refresh];
  NSLocale * locale = [NSLocale currentLocale];
  NSCalendar * calendar = [NSCalendar currentCalendar];
  calendar.locale = locale;

  NSDateFormatter * dateFormatter = [[NSDateFormatter alloc] init];
  dateFormatter.locale = locale;
  dateFormatter.timeStyle = NSDateFormatterShortStyle;
  dateFormatter.dateStyle = NSDateFormatterNoStyle;

  MWMOpeningHoursSection * section = self.section;
  NSUInteger const row = self.row;
  NSDate * openDate = [calendar dateFromComponents:[section timeForRow:row isStart:YES]];
  NSDate * closeDate = [calendar dateFromComponents:[section timeForRow:row isStart:NO]];

  NSString * openString = [dateFormatter stringFromDate:openDate];
  NSString * closeString = [dateFormatter stringFromDate:closeDate];

  self.timeSpanLabel.text = [NSString stringWithFormat:@"%@-%@", openString, closeString];

  self.timeSpanLabel.textColor =
      [self.section isRowSelected:row] ? [UIColor linkBlue] : [UIColor blackSecondaryText];
}

#pragma mark - Actions

- (IBAction)cancelTap
{
  [self.section removeClosedTime:self.row];
}

- (IBAction)expandTap
{
  if (!self.isVisible)
    return;
  NSUInteger const row = self.row;
  self.section.selectedRow = [self.section isRowSelected:row] ? nil : @(row);
  [self.section refresh:NO];
}

@end
