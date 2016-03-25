#import "Macros.h"
#import "MWMSearchCategoryCell.h"
#import "UIColor+MapsMeColor.h"
#import "UIImageView+Coloring.h"

@interface MWMSearchCategoryCell ()

@property (weak, nonatomic) IBOutlet UIImageView * _Nullable icon;
@property (weak, nonatomic) IBOutlet UILabel * _Nullable label;

@end

@implementation MWMSearchCategoryCell

- (void)awakeFromNib
{
  [super awakeFromNib];
  if (IPAD)
    self.contentView.backgroundColor = [UIColor white];
  CALayer * sl = self.layer;
  sl.shouldRasterize = YES;
  sl.rasterizationScale = UIScreen.mainScreen.scale;
}

- (void)setCategory:(NSString *)category
{
  self.label.text = L(category);
  self.icon.mwm_name = [NSString stringWithFormat:@"ic_%@", category];
}

@end
