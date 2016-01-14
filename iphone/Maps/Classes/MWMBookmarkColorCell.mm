#import "MWMBookmarkColorCell.h"

@interface MWMBookmarkColorCell ()

@property (copy, nonatomic) NSString * currentImageName;

@end

@implementation MWMBookmarkColorCell

- (void)configureWithColorString:(NSString *)colorString
{
  self.currentImageName = colorString;
  self.titleLabel.text = L([colorString stringByReplacingOccurrencesOfString:@"placemark-" withString:@""]);
}

- (void)setSelected:(BOOL)selected animated:(BOOL)animated
{
  [super setSelected:selected animated:animated];
  [self.colorButton setImage:[UIImage imageNamed:[NSString stringWithFormat:@"%@%@", self.currentImageName, selected ? @"-on" : @"-off"]] forState:UIControlStateNormal];
  self.approveImageView.hidden = !selected;
}

@end
