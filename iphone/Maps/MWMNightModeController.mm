#import "MWMNightModeController.h"
#import "MWMSettings.h"
#import "MapsAppDelegate.h"
#import "SelectableCell.h"
#import "Statistics.h"
#import "UIColor+MapsMeColor.h"

#include "Framework.h"

@interface MWMNightModeController ()

@property(weak, nonatomic) IBOutlet SelectableCell * autoSwitch;
@property(weak, nonatomic) IBOutlet SelectableCell * on;
@property(weak, nonatomic) IBOutlet SelectableCell * off;
@property(weak, nonatomic) SelectableCell * selectedCell;

@end

@implementation MWMNightModeController

- (void)viewDidLoad
{
  [super viewDidLoad];
  self.title = L(@"pref_map_style_title");
  if ([MWMSettings autoNightModeEnabled])
  {
    self.autoSwitch.accessoryType = UITableViewCellAccessoryCheckmark;
    _selectedCell = self.autoSwitch;
    return;
  }

  switch (GetFramework().GetMapStyle())
  {
  case MapStyleDark:
    self.on.accessoryType = UITableViewCellAccessoryCheckmark;
    _selectedCell = self.on;
    break;
  case MapStyleClear:
  case MapStyleLight:
    self.off.accessoryType = UITableViewCellAccessoryCheckmark;
    _selectedCell = self.off;
    break;
  case MapStyleMerged:
  case MapStyleCount: break;
  }
}

- (void)setSelectedCell:(SelectableCell *)cell
{
  if ([_selectedCell isEqual:cell])
    return;

  _selectedCell = cell;
  auto & f = GetFramework();
  auto const style = f.GetMapStyle();
  NSString * statValue = nil;
  if ([cell isEqual:self.on])
  {
    [MapsAppDelegate setAutoNightModeOff:YES];
    if (style == MapStyleDark)
      return;
    f.SetMapStyle(MapStyleDark);
    [UIColor setNightMode:YES];
    [self mwm_refreshUI];
    statValue = kStatOn;
  }
  else if ([cell isEqual:self.off])
  {
    [MapsAppDelegate setAutoNightModeOff:YES];
    if (style == MapStyleClear || style == MapStyleLight)
      return;
    f.SetMapStyle(MapStyleClear);
    [UIColor setNightMode:NO];
    [self mwm_refreshUI];
    statValue = kStatOff;
  }
  else if ([cell isEqual:self.autoSwitch])
  {
    [MapsAppDelegate setAutoNightModeOff:NO];
    [MapsAppDelegate changeMapStyleIfNedeed];
    if (style == MapStyleClear || style == MapStyleLight)
      return;
    [UIColor setNightMode:NO];
    f.SetMapStyle(MapStyleClear);
    [self mwm_refreshUI];
    statValue = kStatValue;
  }

  [Statistics logEvent:kStatNightMode withParameters:@{kStatValue : statValue}];
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
  SelectableCell * selectedCell = self.selectedCell;
  selectedCell.accessoryType = UITableViewCellAccessoryNone;
  selectedCell = [tableView cellForRowAtIndexPath:indexPath];
  selectedCell.accessoryType = UITableViewCellAccessoryCheckmark;
  selectedCell.selected = NO;
  self.selectedCell = selectedCell;
}

@end
