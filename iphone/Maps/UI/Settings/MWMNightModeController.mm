#import "MWMNightModeController.h"
#import "MWMSettings.h"
#import "MapsAppDelegate.h"
#import "Statistics.h"
#import "SwiftBridge.h"

#include "Framework.h"

@interface MWMNightModeController ()

@property(weak, nonatomic) IBOutlet SettingsTableViewSelectableCell * autoSwitch;
@property(weak, nonatomic) IBOutlet SettingsTableViewSelectableCell * on;
@property(weak, nonatomic) IBOutlet SettingsTableViewSelectableCell * off;
@property(weak, nonatomic) SettingsTableViewSelectableCell * selectedCell;

@end

@implementation MWMNightModeController

- (void)viewDidLoad
{
  [super viewDidLoad];
  self.title = L(@"pref_map_style_title");
  SettingsTableViewSelectableCell * selectedCell = nil;
  switch ([MWMSettings theme])
  {
  case MWMThemeDay: selectedCell = self.off; break;
  case MWMThemeNight: selectedCell = self.on; break;
  case MWMThemeAuto: selectedCell = self.autoSwitch; break;
  }
  selectedCell.accessoryType = UITableViewCellAccessoryCheckmark;
  self.selectedCell = selectedCell;
}

- (void)setSelectedCell:(SettingsTableViewSelectableCell *)cell
{
  if ([_selectedCell isEqual:cell])
    return;

  BOOL const isNightMode = [UIColor isNightMode];
  _selectedCell = cell;
  NSString * statValue = nil;
  if ([cell isEqual:self.on])
  {
    [MWMSettings setTheme:MWMThemeNight];
    statValue = kStatOn;
  }
  else if ([cell isEqual:self.off])
  {
    [MWMSettings setTheme:MWMThemeDay];
    statValue = kStatOff;
  }
  else if ([cell isEqual:self.autoSwitch])
  {
    [MWMSettings setTheme:MWMThemeAuto];
    statValue = kStatValue;
  }
  if (isNightMode != [UIColor isNightMode])
    [self mwm_refreshUI];
  [Statistics logEvent:kStatNightMode withParameters:@{kStatValue : statValue}];
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
  SettingsTableViewSelectableCell * selectedCell = self.selectedCell;
  selectedCell.accessoryType = UITableViewCellAccessoryNone;
  selectedCell = [tableView cellForRowAtIndexPath:indexPath];
  selectedCell.accessoryType = UITableViewCellAccessoryCheckmark;
  selectedCell.selected = NO;
  self.selectedCell = selectedCell;
}

@end
