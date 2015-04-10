#import "EditorVC.h"
#import "UIKitCategories.h"


#define TEXTFIELD_TAG 100

@implementation EditorVC

- (id) initWithUserMark:(const UserMark *)mark
{
  self = [super initWithStyle:UITableViewStyleGrouped];
  if (self)
  {
    self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemSave target:self action:@selector(onSavePressed:)];
  }
  return self;
}

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation
{
  return YES;
}

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView
{
  return 4;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
  switch (section)
  {
  case 0: return 2; // Basic
  case 1: return 3; // Address
  case 2: return 3; // Contact
  case 3: return 4; // Other
  }
  return 0;
}

- (NSString *)tableView:(UITableView *)tableView titleForHeaderInSection:(NSInteger)section
{
  switch (section)
  {
    case 0: return nil;//L(@"basic_information");
    case 1: return L(@"address");
    case 2: return L(@"contacts");
    case 3: return L(@"other_information");
  }
  return nil;
}

- (UITableViewCell *)getTextFieldCellWithIdentifier:(NSString *)identifier andTitle:(NSString *)title
{
  UITableViewCell * cell = [self.tableView dequeueReusableCellWithIdentifier:identifier];
  if (!cell)
  {
    cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleValue1 reuseIdentifier:identifier];
    cell.textLabel.text = title;
    cell.selectionStyle = UITableViewCellSelectionStyleNone;
    // Temporary, to init font and color.
    cell.detailTextLabel.text = @"temp string";
    // Called to initialize frames and fonts.
    [cell layoutSubviews];
    CGRect const leftR = cell.textLabel.frame;
    CGFloat const padding = leftR.origin.x;
    CGRect r = CGRectMake(padding + leftR.size.width + padding, leftR.origin.y,
                          cell.contentView.frame.size.width - 3 * padding - leftR.size.width, leftR.size.height);
    UITextField * f = [[UITextField alloc] initWithFrame:r];
    f.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    f.enablesReturnKeyAutomatically = YES;
    f.returnKeyType = UIReturnKeyDone;
    f.clearButtonMode = UITextFieldViewModeWhileEditing;
    f.autocorrectionType = UITextAutocorrectionTypeNo;
    f.textAlignment = NSTextAlignmentRight;
    f.textColor = cell.detailTextLabel.textColor;
    f.font = [cell.detailTextLabel.font fontWithSize:[cell.detailTextLabel.font pointSize]];
    f.tag = TEXTFIELD_TAG;
    f.delegate = self;
    f.autocapitalizationType = UITextAutocapitalizationTypeNone;
    // Reset temporary font
    cell.detailTextLabel.text = nil;
    [cell.contentView addSubview:f];
  }
  return cell;
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
  UITableViewCell * cell = nil;
  switch (indexPath.section)
  {
    case 0: // First section, contains name and type.
    {
      switch (indexPath.row)
      {
        case 0: // Name.
          cell = [self getTextFieldCellWithIdentifier:@"EditVCNameCell" andTitle:@"name"];
          ((UITextField *)[cell.contentView viewWithTag:TEXTFIELD_TAG]).text = @"Some Name";
          break;
        case 1: // Type.
          cell = [self getTextFieldCellWithIdentifier:@"EditVCTypeCell" andTitle:@"type"];
          ((UITextField *)[cell.contentView viewWithTag:TEXTFIELD_TAG]).text = @"feature-type";
          break;
      }
    }
    break;
    case 1: // Second section, contains address.
    {
      switch (indexPath.row)
      {
        case 0: // House number.
          cell = [self getTextFieldCellWithIdentifier:@"EditVCHouseCell" andTitle:@"house_number"];
          ((UITextField *)[cell.contentView viewWithTag:TEXTFIELD_TAG]).text = @"43к1";
          break;
        case 1: // Street.
          cell = [self getTextFieldCellWithIdentifier:@"EditVCStreetCell" andTitle:@"street"];
          ((UITextField *)[cell.contentView viewWithTag:TEXTFIELD_TAG]).text = @"ул. Осмерская";
          break;
        case 2: // Post code.
          cell = [self getTextFieldCellWithIdentifier:@"EditVCPostCodeCell" andTitle:@"post_code"];
          ((UITextField *)[cell.contentView viewWithTag:TEXTFIELD_TAG]).text = @"BN1 3LJ";
          break;
      }
    }
    break;
    case 2: // Third section, contains contacts.
    {
      switch (indexPath.row)
      {
        case 0: // Phone.
          cell = [self getTextFieldCellWithIdentifier:@"EditVCPhoneCell" andTitle:@"phone"];
          ((UITextField *)[cell.contentView viewWithTag:TEXTFIELD_TAG]).text = @"8(800)OSMOSM";
          break;
        case 1: // Web site.
          cell = [self getTextFieldCellWithIdentifier:@"EditVCWebSiteCell" andTitle:@"web_site"];
          ((UITextField *)[cell.contentView viewWithTag:TEXTFIELD_TAG]).text = @"http://maps.me/";
          break;
        case 2: // Email.
          cell = [self getTextFieldCellWithIdentifier:@"EditVCEmailCell" andTitle:@"email"];
          ((UITextField *)[cell.contentView viewWithTag:TEXTFIELD_TAG]).text = @"bugs@maps.me";
          break;
      }
    }
    break;
    case 3: // Fourth section, contains other information.
    {
      switch (indexPath.row)
      {
        case 0: // Open hours.
          cell = [self getTextFieldCellWithIdentifier:@"EditVCOpeningHoursCell" andTitle:@"opening_hours"];
          ((UITextField *)[cell.contentView viewWithTag:TEXTFIELD_TAG]).text = @"8:00-21:00";
          break;
        case 1: // Cuisune.
          cell = [self getTextFieldCellWithIdentifier:@"EditVCCuisineCell" andTitle:@"cuisine"];
          ((UITextField *)[cell.contentView viewWithTag:TEXTFIELD_TAG]).text = @"осмерская кухня";
          break;
        case 2: // Operator.
          cell = [self getTextFieldCellWithIdentifier:@"EditVCOperatorCell" andTitle:@"operator"];
          ((UITextField *)[cell.contentView viewWithTag:TEXTFIELD_TAG]).text = @"Credit Suisse";
          break;
        case 3: // Elevation.
          cell = [self getTextFieldCellWithIdentifier:@"EditVCElevationCell" andTitle:@"elevation"];
          ((UITextField *)[cell.contentView viewWithTag:TEXTFIELD_TAG]).text = @"4125m";
          break;
      }
    }
    break;
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
  // TODO: Save possibly edited fields?
  [super viewWillDisappear:animated];
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
