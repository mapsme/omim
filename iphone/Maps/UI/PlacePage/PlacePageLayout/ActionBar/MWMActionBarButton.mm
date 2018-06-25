#import "MWMActionBarButton.h"
#import "MWMCommon.h"
#import "MWMButton.h"
#import "MWMCircularProgress.h"

NSString * titleForPartner(int partnerIndex)
{
  NSString * str = [NSString stringWithFormat:@"sponsored_partner%d_action", partnerIndex];
  NSString * localizedStr = L(str);
  NSCAssert(![str isEqualToString:localizedStr], @"Localization is absent.");
  return localizedStr;
}

NSString * titleForButton(EButton type, int partnerIndex, BOOL isSelected)
{
  switch (type)
  {
  case EButton::Download: return L(@"download");
  case EButton::Booking:
  case EButton::Opentable: return L(@"book_button");
  case EButton::BookingSearch: return L(@"booking_search");
  case EButton::Call: return L(@"placepage_call_button");
  case EButton::Bookmark: return L(isSelected ? @"delete" : @"save");
  case EButton::RouteFrom: return L(@"p2p_from_here");
  case EButton::RouteTo: return L(@"p2p_to_here");
  case EButton::Share: return L(@"share");
  case EButton::More: return L(@"placepage_more_button");
  case EButton::RouteAddStop: return L(@"placepage_add_stop");
  case EButton::RouteRemoveStop: return L(@"placepage_remove_stop");
  case EButton::Partner: return titleForPartner(partnerIndex);
  }
}

NSString * imageNameForPartner(int partnerIndex)
{
  return [NSString stringWithFormat:@"ic_28px_logo_partner%d", partnerIndex];
}

UIImage * imageForPartner(int partnerIndex)
{
  UIImage * img = [UIImage imageNamed:imageNameForPartner(partnerIndex)];
  NSCAssert(img != nil, @"Partner image is absent.");
  return img;
}

UIColor * textColorForPartner(int partnerIndex)
{
  NSString * textColor = [NSString stringWithFormat:@"partner%dTextColor", partnerIndex];
  UIColor * color = [UIColor colorWithName:textColor];
  NSCAssert(color != nil, @"Partner text color is absent.");
  return color;
}

UIColor * backgroundColorForPartner(int partnerIndex)
{
  NSString * colorName = [NSString stringWithFormat:@"partner%dBackground", partnerIndex];
  UIColor * color = [UIColor colorWithName:colorName];
  NSCAssert(color != nil, @"Partner background color is absent.");
  return color;
}

@interface MWMActionBarButton () <MWMCircularProgressProtocol>

@property(nonatomic) EButton type;
@property(nonatomic) MWMCircularProgress * mapDownloadProgress;
@property(nonatomic) int partnerIndex;
@property(weak, nonatomic) IBOutlet MWMButton * button;
@property(weak, nonatomic) IBOutlet UILabel * label;
@property(weak, nonatomic) IBOutlet UIView * extraBackground;
@property(weak, nonatomic) id<MWMActionBarButtonDelegate> delegate;

@end

@implementation MWMActionBarButton

- (void)configButton:(BOOL)isSelected disabled:(BOOL)isDisabled
{
  self.label.text = titleForButton(self.type, self.partnerIndex, isSelected);
  self.extraBackground.hidden = YES;
  switch (self.type)
  {
  case EButton::Download:
  {
    if (self.mapDownloadProgress)
      return;

    self.mapDownloadProgress = [MWMCircularProgress downloaderProgressForParentView:self.button];
    self.mapDownloadProgress.delegate = self;

    MWMCircularProgressStateVec const affectedStates = {MWMCircularProgressStateNormal,
                                                        MWMCircularProgressStateSelected};

    [self.mapDownloadProgress setImageName:@"ic_download" forStates:affectedStates];
    [self.mapDownloadProgress setColoring:MWMButtonColoringBlue forStates:affectedStates];
    break;
    }
    case EButton::Booking:
      [self.button setImage:[UIImage imageNamed:@"ic_booking_logo"] forState:UIControlStateNormal];
      self.label.textColor = UIColor.whiteColor;
      self.backgroundColor = [UIColor bookingBackground];
      if (!IPAD)
      {
        self.extraBackground.backgroundColor = [UIColor bookingBackground];
        self.extraBackground.hidden = NO;
      }
      break;
    case EButton::BookingSearch:
      [self.button setImage:[UIImage imageNamed:@"ic_booking_search"]
                   forState:UIControlStateNormal];
      self.label.textColor = UIColor.whiteColor;
      self.backgroundColor = [UIColor bookingBackground];
      if (!IPAD)
      {
        self.extraBackground.backgroundColor = [UIColor bookingBackground];
        self.extraBackground.hidden = NO;
      }
      break;
    case EButton::Opentable:
      [self.button setImage:[UIImage imageNamed:@"ic_opentable"] forState:UIControlStateNormal];
      self.label.textColor = UIColor.whiteColor;
      self.backgroundColor = [UIColor opentableBackground];
      if (!IPAD)
      {
        self.extraBackground.backgroundColor = [UIColor opentableBackground];
        self.extraBackground.hidden = NO;
      }
      break;
    case EButton::Call:
      [self.button setImage:[UIImage imageNamed:@"ic_placepage_phone_number"]
                   forState:UIControlStateNormal];
      break;
    case EButton::Bookmark: [self setupBookmarkButton:isSelected]; break;
    case EButton::RouteFrom:
      [self.button setImage:[UIImage imageNamed:@"ic_route_from"] forState:UIControlStateNormal];
      break;
    case EButton::RouteTo:
      [self.button setImage:[UIImage imageNamed:@"ic_route_to"] forState:UIControlStateNormal];
      break;
    case EButton::Share:
      [self.button setImage:[UIImage imageNamed:@"ic_menu_share"] forState:UIControlStateNormal];
      break;
    case EButton::More:
      [self.button setImage:[UIImage imageNamed:@"ic_placepage_more"]
                   forState:UIControlStateNormal];
      break;
    case EButton::RouteAddStop:
      [self.button setImage:[UIImage imageNamed:@"ic_add_route_point"]
                   forState:UIControlStateNormal];
      break;
    case EButton::RouteRemoveStop:
      [self.button setImage:[UIImage imageNamed:@"ic_remove_route_point"]
                   forState:UIControlStateNormal];
      break;
    case EButton::Partner:
      [self.button setImage:imageForPartner(self.partnerIndex) forState:UIControlStateNormal];
      self.label.textColor = textColorForPartner(self.partnerIndex);
      self.backgroundColor = backgroundColorForPartner(self.partnerIndex);
      break;
  }
  self.button.enabled = !isDisabled;
}

+ (MWMActionBarButton *)buttonWithDelegate:(id<MWMActionBarButtonDelegate>)delegate
                                buttonType:(EButton)type
                              partnerIndex:(int)partnerIndex
                                isSelected:(BOOL)isSelected
                                isDisabled:(BOOL)isDisabled
{
  MWMActionBarButton * button =
      [NSBundle.mainBundle loadNibNamed:[self className] owner:nil options:nil].firstObject;
  button.delegate = delegate;
  button.type = type;
  button.partnerIndex = partnerIndex;
  [button configButton:isSelected disabled:isDisabled];
  return button;
}

- (void)progressButtonPressed:(MWMCircularProgress *)progress
{
  [self.delegate tapOnButtonWithType:EButton::Download];
}

- (IBAction)tap
{
  if (self.type == EButton::Bookmark)
    [self setBookmarkSelected:!self.button.isSelected];

  [self.delegate tapOnButtonWithType:self.type];
}

- (void)setBookmarkSelected:(BOOL)isSelected
{
  if (isSelected)
    [self.button.imageView startAnimating];

  self.button.selected = isSelected;
  self.label.text = L(isSelected ? @"delete" : @"save");
}

- (void)setupBookmarkButton:(BOOL)isSelected
{
  MWMButton * btn = self.button;
  [btn setImage:[UIImage imageNamed:@"ic_bookmarks_off"] forState:UIControlStateNormal];
  [btn setImage:[UIImage imageNamed:@"ic_bookmarks_on"] forState:UIControlStateSelected];
  [btn setImage:[UIImage imageNamed:@"ic_bookmarks_on"] forState:UIControlStateHighlighted];
  [btn setImage:[UIImage imageNamed:@"ic_bookmarks_on"] forState:UIControlStateDisabled];

  [self setBookmarkSelected:isSelected];

  NSUInteger const animationImagesCount = 11;
  NSMutableArray * animationImages = [NSMutableArray arrayWithCapacity:animationImagesCount];
  for (NSUInteger i = 0; i < animationImagesCount; ++i)
  {
    UIImage * image = [UIImage imageNamed:[NSString stringWithFormat:@"ic_bookmarks_%@", @(i + 1)]];
    animationImages[i] = image;
  }
  UIImageView * animationIV = btn.imageView;
  animationIV.animationImages = animationImages;
  animationIV.animationRepeatCount = 1;
}

@end
