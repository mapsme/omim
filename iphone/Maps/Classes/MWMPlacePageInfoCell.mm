#import "MWMPlacePageEntity.h"
#import "MWMPlacePageInfoCell.h"
#import "Statistics.h"
#import "UIFont+MapsMeFonts.h"

#include "platform/settings.hpp"
#include "platform/measurement_utils.hpp"

extern NSString * const kUserDefaultsLatLonAsDMSKey;

@interface MWMPlacePageInfoCell () <UITextViewDelegate>

@property (weak, nonatomic, readwrite) IBOutlet UIImageView * icon;
@property (weak, nonatomic, readwrite) IBOutlet id textContainer;

@property (weak, nonatomic) IBOutlet UIButton * upperButton;
@property (nonatomic) MWMPlacePageMetadataType type;

@end

@implementation MWMPlacePageInfoCell

- (void)awakeFromNib
{
  [super awakeFromNib];
  if ([self.textContainer isKindOfClass:[UITextView class]])
  {
    CGFloat const topInset = 12.0;
    [self.textContainer setTextContainerInset:{topInset, 0, 0, 0}];
  }
}

- (void)configureWithType:(MWMPlacePageMetadataType)type info:(NSString *)info;
{
  NSString * typeName;
  switch (type)
  {
    case MWMPlacePageMetadataTypeURL:
    case MWMPlacePageMetadataTypeWebsite:
      typeName = @"website";
      break;
    case MWMPlacePageMetadataTypeEmail:
      typeName = @"email";
      break;
    case MWMPlacePageMetadataTypePhoneNumber:
      typeName = @"phone_number";
      break;
    case MWMPlacePageMetadataTypeCoordinate:
      typeName = @"coordinate";
      break;
    case MWMPlacePageMetadataTypePostcode:
      typeName = @"postcode";
      break;
    case MWMPlacePageMetadataTypeWiFi:
      typeName = @"wifi";
      break;
    case MWMPlacePageMetadataTypeBookmark:
    case MWMPlacePageMetadataTypeEditButton:
    case MWMPlacePageMetadataTypeOpenHours:
      NSAssert(false, @"Incorrect type!");
      break;
  }

  UIImage * image =
      [UIImage imageNamed:[NSString stringWithFormat:@"%@%@", @"ic_placepage_", typeName]];
  self.type = type;
  self.icon.image = image;

  if ([self.textContainer isKindOfClass:[UITextView class]])
    [self.textContainer
        setAttributedText:[[NSAttributedString alloc]
                              initWithString:info
                                  attributes:@{NSFontAttributeName : [UIFont regular17]}]];
  else
    [self.textContainer setText:info];

  UILongPressGestureRecognizer * longTap =
      [[UILongPressGestureRecognizer alloc] initWithTarget:self action:@selector(longTap:)];
  longTap.minimumPressDuration = 0.3;
  [self.upperButton addGestureRecognizer:longTap];
}

- (BOOL)textView:(UITextView *)textView shouldInteractWithURL:(NSURL *)URL inRange:(NSRange)characterRange
{
  return YES;
}

- (IBAction)cellTap
{
  switch (self.type)
  {
    case MWMPlacePageMetadataTypeURL:
    case MWMPlacePageMetadataTypeWebsite:
      [[Statistics instance] logEvent:kStatEventName(kStatPlacePage, kStatOpenSite)];
      break;
    case MWMPlacePageMetadataTypeEmail:
      [[Statistics instance] logEvent:kStatEventName(kStatPlacePage, kStatSendEmail)];
      break;
    case MWMPlacePageMetadataTypePhoneNumber:
      [[Statistics instance] logEvent:kStatEventName(kStatPlacePage, kStatCallPhoneNumber)];
      break;
    case MWMPlacePageMetadataTypeCoordinate:
      [[Statistics instance] logEvent:kStatEventName(kStatPlacePage, kStatToggleCoordinates)];
      break;
    default:
      break;
  }
  if (self.type != MWMPlacePageMetadataTypeCoordinate)
    return;
  NSUserDefaults * defaults = [NSUserDefaults standardUserDefaults];
  BOOL const showLatLonAsDMS = [defaults boolForKey:kUserDefaultsLatLonAsDMSKey];
  [defaults setBool:!showLatLonAsDMS forKey:kUserDefaultsLatLonAsDMSKey];
  [defaults synchronize];

  // @NOTE: coordinates method depends on kUserDefaultsLatLonAsDMSKey value above.
  [self.textContainer setText:[self.currentEntity coordinates]];
}

- (void)longTap:(UILongPressGestureRecognizer *)sender
{
  UIMenuController * menuController = [UIMenuController sharedMenuController];
  if (menuController.isMenuVisible)
    return;
  CGPoint const tapPoint = [sender locationInView:sender.view.superview];
  UIView * targetView = [self.textContainer isKindOfClass:[UITextView class]] ? sender.view : self.textContainer;
  [menuController setTargetRect:CGRectMake(tapPoint.x, targetView.minY, 0., 0.) inView:sender.view.superview];
  [menuController setMenuVisible:YES animated:YES];
  [targetView becomeFirstResponder];
  [menuController update];
}

@end
