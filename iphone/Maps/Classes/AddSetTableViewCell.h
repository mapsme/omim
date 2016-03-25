#import "MWMTableViewCell.h"

@protocol AddSetTableViewCellProtocol <NSObject>

- (void)onDone:(NSString *)text;

@end

@interface AddSetTableViewCell : MWMTableViewCell

@property (weak, nonatomic) IBOutlet UITextField * _Nullable textField;

@property (weak, nonatomic) id<AddSetTableViewCellProtocol> _Nullable delegate;

@end
