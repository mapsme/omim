#import "MWMOpeningHoursModel.h"
#import "MWMOpeningHoursSection.h"

#include "editor/opening_hours_ui.hpp"

extern UITableViewRowAnimation const kMWMOpeningHoursEditorRowAnimation = UITableViewRowAnimationFade;

@interface MWMOpeningHoursModel () <MWMOpeningHoursSectionProtocol>

@property (weak, nonatomic, readwrite) UITableView * tableView;

@property (nonatomic) NSMutableArray<MWMOpeningHoursSection *> * sections;

@end

using namespace editor::ui;
using namespace osmoh;

@implementation MWMOpeningHoursModel
{
  TimeTableSet timeTableSet;
}

- (instancetype _Nullable)initWithDelegate:(id<MWMOpeningHoursModelProtocol> _Nonnull)delegate
{
  self = [super init];
  if (self)
  {
    _tableView = delegate.tableView;
    _sections = [NSMutableArray arrayWithCapacity:timeTableSet.Size()];
    while (self.sections.count < timeTableSet.Size())
      [self addSection];
  }
  return self;
}

- (void)addSection
{
  [self.sections addObject:[[MWMOpeningHoursSection alloc] initWithDelegate:self]];
  [self refreshSectionsIndexes];
  NSIndexSet * set = [[NSIndexSet alloc] initWithIndex:self.count - 1];
  [self.tableView reloadSections:set withRowAnimation:kMWMOpeningHoursEditorRowAnimation];
}

- (void)refreshSectionsIndexes
{
  [self.sections enumerateObjectsUsingBlock:^(MWMOpeningHoursSection * _Nonnull section,
                                              NSUInteger idx, BOOL * _Nonnull stop)
  {
    [section refreshIndex:idx];
  }];
}

- (void)addSchedule
{
  NSAssert(self.canAddSection, @"Can not add schedule");
  timeTableSet.Append(timeTableSet.GetComplementTimeTable());
  [self addSection];
  NSAssert(timeTableSet.Size() == self.sections.count, @"Inconsistent state");
  [self.sections[self.sections.count - 1] scrollIntoView];
}

- (void)deleteSchedule:(NSUInteger)index
{
  NSAssert(index < self.count, @"Invalid section index");
  BOOL const needRealDelete = self.canAddSection;
  timeTableSet.Remove(index);
  [self.sections removeObjectAtIndex:index];
  [self refreshSectionsIndexes];
  if (needRealDelete)
  {
    NSIndexSet * set = [[NSIndexSet alloc] initWithIndex:index];
    [self.tableView deleteSections:set withRowAnimation:kMWMOpeningHoursEditorRowAnimation];
  }
  else
  {
    NSIndexSet * set = [[NSIndexSet alloc] initWithIndexesInRange:{index, self.count - index + 1}];
    [self.tableView reloadSections:set withRowAnimation:kMWMOpeningHoursEditorRowAnimation];
  }
}

- (void)updateActiveSection:(NSUInteger)index
{
  for (MWMOpeningHoursSection * section in self.sections)
  {
    if (section.index != index)
      section.selectedRow = nil;
  }
}

- (TTimeTableProxy)getTimeTableProxy:(NSUInteger)index
{
  NSAssert(index < self.count, @"Invalid section index");
  return timeTableSet.Get(index);
}

- (MWMOpeningHoursEditorCells)cellKeyForIndexPath:(NSIndexPath * _Nonnull)indexPath
{
  NSUInteger const section = indexPath.section;
  NSAssert(section < self.count, @"Invalid section index");
  return [self.sections[section] cellKeyForRow:indexPath.row];
}

- (CGFloat)heightForIndexPath:(NSIndexPath * _Nonnull)indexPath withWidth:(CGFloat)width
{
  NSUInteger const section = indexPath.section;
  NSAssert(section < self.count, @"Invalid section index");
  return [self.sections[section] heightForRow:indexPath.row withWidth:width];
}

- (void)fillCell:(MWMOpeningHoursTableViewCell * _Nonnull)cell atIndexPath:(NSIndexPath * _Nonnull)indexPath
{
  NSUInteger const section = indexPath.section;
  NSAssert(section < self.count, @"Invalid section index");
  cell.section = self.sections[section];
}

- (NSUInteger)numberOfRowsInSection:(NSUInteger)section
{
  NSAssert(section < self.count, @"Invalid section index");
  return self.sections[section].numberOfRows;
}

#pragma mark - Properties

- (NSUInteger)count
{
  NSAssert(timeTableSet.Size() == self.sections.count, @"Inconsistent state");
  return self.sections.count;
}

- (BOOL)canAddSection
{
  return !timeTableSet.GetUnhandledDays().empty();
}

- (NSArray<NSString *> *)unhandledDays
{
  auto unhandledDays = timeTableSet.GetUnhandledDays();
  NSMutableArray<NSString *> * dayNames = [NSMutableArray arrayWithCapacity:unhandledDays.size()];
  for(auto & weekDay : unhandledDays)
  {
    switch (weekDay)
    {
      case Weekday::Sunday:
        [dayNames addObject:L(@"su")];
        break;
      case Weekday::Monday:
        [dayNames addObject:L(@"mo")];
        break;
      case Weekday::Tuesday:
        [dayNames addObject:L(@"tu")];
        break;
      case Weekday::Wednesday:
        [dayNames addObject:L(@"we")];
        break;
      case Weekday::Thursday:
        [dayNames addObject:L(@"th")];
        break;
      case Weekday::Friday:
        [dayNames addObject:L(@"fr")];
        break;
      case Weekday::Saturday:
        [dayNames addObject:L(@"sa")];
        break;
      default:
        NSAssert(false, @"Invalid case");
        break;
    }
  }
  return dayNames;
}

@end
