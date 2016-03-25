#import "MWMTableViewCell.h"

@class SwitchCell;
@protocol SwitchCellDelegate <NSObject>

- (void)switchCell:(SwitchCell *)cell didChangeValue:(BOOL)value;

@end

@interface SwitchCell : MWMTableViewCell

@property (weak, nonatomic) IBOutlet UILabel * _Nullable titleLabel;
@property (weak, nonatomic) IBOutlet UISwitch * _Nullable switchButton;

@property (weak) id<SwitchCellDelegate> _Nullable delegate;

@end
