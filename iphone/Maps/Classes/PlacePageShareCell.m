
#import "PlacePageShareCell.h"
#import "UIKitCategories.h"

@interface PlacePageShareCell ()

@property (nonatomic) UIButton * shareButton;
@property (nonatomic) UIButton * apiButton;
@property (nonatomic) UIButton * editButton;

@end

@implementation PlacePageShareCell

- (id)initWithStyle:(UITableViewCellStyle)style reuseIdentifier:(NSString *)reuseIdentifier
{
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  self.selectionStyle = UITableViewCellSelectionStyleNone;

  [self addSubview:self.shareButton];
  [self addSubview:self.apiButton];
  [self addSubview:self.editButton];

  return self;
}

#define BUTTON_HEIGHT 41

- (void)layoutSubviews
{
  CGFloat const xOffset = 14;
  CGFloat const betweenOffset = 10;
  if (self.apiAppTitle)
  {
    CGFloat const buttonWidth = (self.width - 2 * xOffset - betweenOffset) / 3;
    self.shareButton.frame = CGRectMake(xOffset, 0, buttonWidth, BUTTON_HEIGHT);
    self.apiButton.frame = CGRectMake(self.shareButton.maxX + betweenOffset, 0, buttonWidth, BUTTON_HEIGHT);
    self.editButton.frame = CGRectMake(self.apiButton.maxX + betweenOffset, 0, buttonWidth, BUTTON_HEIGHT);
    self.apiButton.hidden = NO;
    [self.apiButton setTitle:self.apiAppTitle forState:UIControlStateNormal];
  }
  else
  {
    CGFloat const buttonWidth = (self.width - 2 * xOffset) / 2;
    self.shareButton.frame = CGRectMake(xOffset, 0, buttonWidth, BUTTON_HEIGHT);
    self.editButton.frame = CGRectMake(self.shareButton.maxX + betweenOffset, 0, buttonWidth, BUTTON_HEIGHT);
    self.apiButton.hidden = YES;
  }
  self.apiButton.midY = self.shareButton.midY = self.editButton.midY = self.height / 2;
  self.backgroundColor = [UIColor clearColor];
}

+ (CGFloat)cellHeight
{
  return 54;
}

- (void)shareButtonPressed:(id)sender
{
  [self.delegate shareCellDidPressShareButton:self];
}

- (void)apiButtonPressed:(id)sender
{
  [self.delegate shareCellDidPressApiButton:self];
}

- (void)editButtonPressed:(id)sender
{
  [self.delegate shareCellDidPressEditButton:self];
}

- (UIButton *)shareButton
{
  if (!_shareButton)
  {
    UIImage * image = [[UIImage imageNamed:@"PlacePageButton"] resizableImageWithCapInsets:UIEdgeInsetsMake(6, 6, 6, 6)];
    _shareButton = [[UIButton alloc] initWithFrame:CGRectZero];
    _shareButton.titleLabel.font = [UIFont fontWithName:@"HelveticaNeue-Light" size:18];
    _shareButton.autoresizingMask = UIViewAutoresizingFlexibleLeftMargin | UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleRightMargin;
    [_shareButton setBackgroundImage:image forState:UIControlStateNormal];
    [_shareButton addTarget:self action:@selector(shareButtonPressed:) forControlEvents:UIControlEventTouchUpInside];
    [_shareButton setTitle:L(@"share") forState:UIControlStateNormal];
    [_shareButton setTitleColor:[UIColor colorWithColorCode:@"179E4D"] forState:UIControlStateNormal];
    [_shareButton setTitleColor:[UIColor grayColor] forState:UIControlStateHighlighted];
  }
  return _shareButton;
}

- (UIButton *)apiButton
{
  if (!_apiButton)
  {
    UIImage * image = [[UIImage imageNamed:@"PlacePageButton"] resizableImageWithCapInsets:UIEdgeInsetsMake(6, 6, 6, 6)];
    _apiButton = [[UIButton alloc] initWithFrame:CGRectZero];
    _apiButton.titleLabel.font = [UIFont fontWithName:@"HelveticaNeue-Light" size:18];
    _apiButton.autoresizingMask = UIViewAutoresizingFlexibleLeftMargin | UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleRightMargin;
    [_apiButton setBackgroundImage:image forState:UIControlStateNormal];
    [_apiButton addTarget:self action:@selector(apiButtonPressed:) forControlEvents:UIControlEventTouchUpInside];
    [_apiButton setTitleColor:[UIColor colorWithColorCode:@"179E4D"] forState:UIControlStateNormal];
    [_apiButton setTitleColor:[UIColor grayColor] forState:UIControlStateHighlighted];
  }
  return _apiButton;
}

- (UIButton *)editButton
{
  if (!_editButton)
  {
    UIImage * image = [[UIImage imageNamed:@"PlacePageButton"] resizableImageWithCapInsets:UIEdgeInsetsMake(6, 6, 6, 6)];
    _editButton = [[UIButton alloc] initWithFrame:CGRectZero];
    _editButton.titleLabel.font = [UIFont fontWithName:@"HelveticaNeue-Light" size:18];
    _editButton.autoresizingMask = UIViewAutoresizingFlexibleLeftMargin | UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleRightMargin;
    [_editButton setBackgroundImage:image forState:UIControlStateNormal];
    [_editButton addTarget:self action:@selector(editButtonPressed:) forControlEvents:UIControlEventTouchUpInside];
    [_editButton setTitle:L(@"edit") forState:UIControlStateNormal];
    [_editButton setTitleColor:[UIColor colorWithColorCode:@"179E4D"] forState:UIControlStateNormal];
    [_editButton setTitleColor:[UIColor grayColor] forState:UIControlStateHighlighted];
  }
  return _editButton;
}

@end
