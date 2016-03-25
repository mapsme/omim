#import "MWMTableViewCell.h"
#import "UIColor+MapsMeColor.h"

@implementation MWMTableViewCell

- (instancetype _Nonnull)initWithStyle:(UITableViewCellStyle)style reuseIdentifier:(NSString *)reuseIdentifier
{
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self)
    [self configure];
  return self;
}

- (void)awakeFromNib
{
  [self configure];
}

- (void)configure
{
  self.backgroundColor = [UIColor white];
  self.textLabel.textColor = [UIColor blackPrimaryText];
  self.selectedBackgroundView = [[UIView alloc] init];
  self.selectedBackgroundView.backgroundColor = [UIColor pressBackground];
}

@end
