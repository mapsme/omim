#import "MWMPlacePageEntity.h"
#import "MWMPlacePageInfoCell.h"
#import "Statistics.h"
#import "UIFont+MapsMeFonts.h"
#import "UIKitCategories.h"

#include "platform/settings.hpp"
#include "platform/measurement_utils.hpp"

@interface MWMPlacePageInfoCell () <UITextViewDelegate>

@property (weak, nonatomic, readwrite) IBOutlet UIImageView * icon;
@property (weak, nonatomic, readwrite) IBOutlet id textContainer;

@property (weak, nonatomic) IBOutlet UIButton * upperButton;
@property (weak, nonatomic) IBOutlet UIImageView * toggleImage;

@property (nonatomic) MWMPlacePageMetadataField type;

@end

@implementation MWMPlacePageInfoCell

- (void)awakeFromNib
{
  [super awakeFromNib];
  if ([self.textContainer isKindOfClass:[UITextView class]])
    [(UITextView *)self.textContainer setTextContainerInset:{.top = 12, 0, 0, 0}];
}

- (void)configureWithType:(MWMPlacePageMetadataField)type info:(NSString *)info;
{
  NSString * typeName;
  switch (type)
  {
    case MWMPlacePageMetadataFieldURL:
    case MWMPlacePageMetadataFieldWebsite:
      self.toggleImage.hidden = YES;
      typeName = @"website";
      break;
    case MWMPlacePageMetadataFieldEmail:
      self.toggleImage.hidden = YES;
      typeName = @"email";
      break;
    case MWMPlacePageMetadataFieldPhoneNumber:
      self.toggleImage.hidden = YES;
      typeName = @"phone_number";
      break;
    case MWMPlacePageMetadataFieldCoordinate:
      self.toggleImage.hidden = NO;
      typeName = @"coordinate";
      break;
    case MWMPlacePageMetadataFieldPostcode:
      self.toggleImage.hidden = YES;
      typeName = @"postcode";
      break;
    case MWMPlacePageMetadataFieldWiFi:
      self.toggleImage.hidden = YES;
      typeName = @"wifi";
      break;
    case MWMPlacePageMetadataFieldBookmark:
    case MWMPlacePageMetadataFieldEditButton:
    case MWMPlacePageMetadataFieldOpenHours:
      NSAssert(false, @"Incorrect type!");
      break;
  }

  UIImage * image =
      [UIImage imageNamed:[NSString stringWithFormat:@"%@%@", @"ic_placepage_", typeName]];
  self.type = type;
  self.icon.image = image;

  [self changeText:info];
  UILongPressGestureRecognizer * longTap =
      [[UILongPressGestureRecognizer alloc] initWithTarget:self action:@selector(longTap:)];
  longTap.minimumPressDuration = 0.3;
  [self.upperButton addGestureRecognizer:longTap];
}

- (void)changeText:(NSString *)text
{
  if ([self.textContainer isKindOfClass:[UITextView class]])
  {
    UITextView * tv = (UITextView *)self.textContainer;
    [tv setAttributedText:[[NSAttributedString alloc]
                              initWithString:text
                                  attributes:@{NSFontAttributeName : [UIFont regular16]}]];
  }
  else
  {
    UILabel * lb = (UILabel *)self.textContainer;
    [lb setText:text];
    [lb sizeToIntegralFit];
    CGFloat const trailingOffset = self.width - lb.maxX;
    lb.font = trailingOffset < 32 ? [UIFont regular15] : [UIFont regular16];
  }
}

- (BOOL)textView:(UITextView *)textView shouldInteractWithURL:(NSURL *)URL inRange:(NSRange)characterRange
{
  return YES;
}

- (IBAction)cellTap
{
  switch (self.type)
  {
    case MWMPlacePageMetadataFieldURL:
    case MWMPlacePageMetadataFieldWebsite:
      [[Statistics instance] logEvent:kStatEventName(kStatPlacePage, kStatOpenSite)];
      break;
    case MWMPlacePageMetadataFieldEmail:
      [[Statistics instance] logEvent:kStatEventName(kStatPlacePage, kStatSendEmail)];
      break;
    case MWMPlacePageMetadataFieldPhoneNumber:
      [[Statistics instance] logEvent:kStatEventName(kStatPlacePage, kStatCallPhoneNumber)];
      break;
    case MWMPlacePageMetadataFieldCoordinate:
      [[Statistics instance] logEvent:kStatEventName(kStatPlacePage, kStatToggleCoordinates)];
      [self.currentEntity toggleCoordinateSystem];
      [self changeText:[self.currentEntity getFieldValue:MWMPlacePageMetadataFieldCoordinate]];
      break;
    default:
      break;
  }
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
