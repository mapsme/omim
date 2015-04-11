#import "AddressEditorVC.h"
#import "UIKitCategories.h"

#include "../../../map/user_mark.hpp"
#include "../../../feature_changeset/featurechangeset.hpp"

#define TEXTFIELD_TAG 100

extern NSString * kOSMLoginKey;
extern NSString * kOSMPasswordKey;

@interface AddressEditorVC()
{
  PoiMarkPoint const * m_mark;
}
@end

@implementation AddressEditorVC

- (id) initWithPoiMark:(PoiMarkPoint const *)mark
{
  self = [super initWithStyle:UITableViewStyleGrouped];
  if (self)
  {
    self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemSave target:self action:@selector(onSavePressed:)];
    m_mark = mark;
  }
  return self;
}

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation
{
  return YES;
}

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView
{
  return 2;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
  return 1;
}

- (NSString *)tableView:(UITableView *)tableView titleForHeaderInSection:(NSInteger)section
{
  switch (section)
  {
    case 0: return L(@"house_number");
    case 1: return L(@"street_name");
  }
  return nil;
}

- (UITableViewCell *)getTextFieldCellWithIdentifier:(NSString *)identifier
{
  UITableViewCell * cell = [self.tableView dequeueReusableCellWithIdentifier:identifier];
  if (!cell)
  {
    cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:identifier];
    cell.selectionStyle = UITableViewCellSelectionStyleNone;
    // Temporary, to init font and color.
    cell.textLabel.text = @"temp string";
    // Called to initialize frames and fonts.
    [cell layoutSubviews];
//    CGRect const leftR = cell.textLabel.frame;
//    CGFloat const padding = leftR.origin.x;
//    CGRect r = CGRectMake(padding + leftR.size.width + padding, leftR.origin.y,
//                          cell.contentView.frame.size.width - 3 * padding - leftR.size.width, leftR.size.height);
    UITextField * f = [[UITextField alloc] initWithFrame:cell.textLabel.frame];
    f.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    f.enablesReturnKeyAutomatically = YES;
    f.returnKeyType = UIReturnKeyDone;
    f.clearButtonMode = UITextFieldViewModeWhileEditing;
    f.autocorrectionType = UITextAutocorrectionTypeNo;
    f.textAlignment = NSTextAlignmentCenter;
    f.textColor = cell.detailTextLabel.textColor;
    f.font = [cell.detailTextLabel.font fontWithSize:[cell.detailTextLabel.font pointSize]];
    f.tag = TEXTFIELD_TAG;
    f.delegate = self;
    f.autocapitalizationType = UITextAutocapitalizationTypeNone;
    // Reset temporary font
    cell.textLabel.text = nil;
    [cell.contentView addSubview:f];
  }
  return cell;
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
  UITableViewCell * cell = nil;
  if (indexPath.section == 0)
  {
    cell = [self getTextFieldCellWithIdentifier:@"EditVCHouseCell"];
    ((UITextField *)[cell.contentView viewWithTag:TEXTFIELD_TAG]).text = [NSString stringWithUTF8String:m_mark->GetInfo().m_house.c_str()];
  }
  else
  {
    cell = [self getTextFieldCellWithIdentifier:@"EditVCStreetCell"];
    ((UITextField *)[cell.contentView viewWithTag:TEXTFIELD_TAG]).text = [NSString stringWithUTF8String:m_mark->GetInfo().m_street.c_str()];
  }
  return cell;
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
  // Remove cell selection.
  [self.tableView deselectRowAtIndexPath:indexPath animated:YES];
  // TODO.
}

- (void)viewDidLoad
{
  [super viewDidLoad];
  self.tableView.backgroundView = nil;
  self.tableView.backgroundColor = [UIColor applicationBackgroundColor];
}

- (void)viewWillDisappear:(BOOL)animated
{
  [super viewWillDisappear:animated];
  UITableViewCell * cell = [self.tableView cellForRowAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:0]];
  NSString * houseNumber = ((UITextField *)[cell.contentView viewWithTag:TEXTFIELD_TAG]).text;
  cell = [self.tableView cellForRowAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:1]];
  NSString * streetName = ((UITextField *)[cell.contentView viewWithTag:TEXTFIELD_TAG]).text;

  edit::TChanges changes;
  // TODO(AlexZ): Old value is always empty now (we always create, not modify).
  if (houseNumber.length)
    changes[edit::HOUSENUMBER] = {"", [houseNumber UTF8String]};
  if (streetName.length)
    changes[edit::STREET] = {"", [streetName UTF8String]};
  if (!changes.empty())
  {
    double lat, lon;
    m_mark->GetLatLon(lon, lat);
    edit::FeatureChangeset editedFields;
    // TODO: Temporarily hard-coded building type.
    uint32_t const buildingType = 70;
    editedFields.CreateChange(edit::MWMLink(lat, lon, buildingType), changes);
    // TODO: What if user does not have login and password in the settings?
    NSUserDefaults * settings = [NSUserDefaults standardUserDefaults];
    string const user = [[settings objectForKey:kOSMLoginKey] UTF8String];
    string const password = [[settings objectForKey:kOSMPasswordKey] UTF8String];
    editedFields.UploadChangeset(user, password);
  }
}

- (BOOL)textFieldShouldReturn:(UITextField *)textField
{
  if (textField.text.length == 0)
    return YES;
  // Hide keyboard.
  [textField resignFirstResponder];
  return NO;
}

- (void)onSavePressed:(id)button
{
  // TODO.
  [self.navigationController popToRootViewControllerAnimated:YES];
}

@end
