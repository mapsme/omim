#import "MWMTableViewCell.h"

@class MWMPlacePageEntity;

@interface MWMPlacePageInfoCell : MWMTableViewCell

- (void)configureWithType:(MWMPlacePageCellType)type info:(NSString *)info;

@property (weak, nonatomic, readonly) IBOutlet UIImageView * _Nullable icon;
@property (weak, nonatomic, readonly) IBOutlet id _Nullable textContainer;
@property (nonatomic) MWMPlacePageEntity * currentEntity;

@end
