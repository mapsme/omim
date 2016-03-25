#import "MWMTableViewCell.h"

@interface MWMBookmarkColorCell : MWMTableViewCell

@property (weak, nonatomic) IBOutlet UIButton * _Nullable colorButton;
@property (weak, nonatomic) IBOutlet UILabel * _Nullable titleLabel;
@property (weak, nonatomic) IBOutlet UIImageView * _Nullable approveImageView;

- (void)configureWithColorString:(NSString *)colorString;

@end
