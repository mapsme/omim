#import "MWMTableViewCell.h"

@implementation MWMTableViewCell

- (instancetype)initWithStyle:(UITableViewCellStyle)style reuseIdentifier:(NSString *)reuseIdentifier
{
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self)
    [self configure];
  return self;
}

- (void)awakeFromNib
{
  [super awakeFromNib];
  [self configure];
}

- (void)configure
{
  self.backgroundColor = [UIColor white];
  self.textLabel.textColor = [UIColor blackPrimaryText];
  self.detailTextLabel.textColor = [UIColor blackSecondaryText];
  self.selectedBackgroundView = [[UIView alloc] init];
  self.selectedBackgroundView.backgroundColor = [UIColor pressBackground];
}

- (void)addSubview:(UIView *)view
{
  [super addSubview:view];
  if (self.isSeparatorHidden)
    [self hideSeparators];
}

- (void)setIsSeparatorHidden:(BOOL)isSeparatorHidden
{
  _isSeparatorHidden = isSeparatorHidden;
  if (isSeparatorHidden)
    [self hideSeparators];
}

- (void)hideSeparators
{
  for (UIView * view in self.subviews)
    view.hidden = [[[view class] className] isEqualToString:@"_UITableViewCellSeparatorView"];
}

- (void)layoutSubviews
{
  [super layoutSubviews];
  if (self.isSeparatorHidden)
    [self hideSeparators];
}

@end

@implementation MWMTableViewSubtitleCell

- (instancetype)initWithStyle:(UITableViewCellStyle)style reuseIdentifier:(NSString *)reuseIdentifier
{
  return [super initWithStyle:UITableViewCellStyleSubtitle reuseIdentifier:reuseIdentifier];
}

@end
