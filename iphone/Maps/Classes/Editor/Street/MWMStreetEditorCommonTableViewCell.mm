#import "MWMStreetEditorCommonTableViewCell.h"

@interface MWMStreetEditorCommonTableViewCell ()

@property (weak, nonatomic) IBOutlet UILabel * _Nullable label;
@property (weak, nonatomic) IBOutlet UIImageView * _Nullable selectedIcon;

@property (weak, nonatomic) id<MWMStreetEditorCommonTableViewCellProtocol> _Nullable delegate;

@end

@implementation MWMStreetEditorCommonTableViewCell

- (void)configWithDelegate:(id<MWMStreetEditorCommonTableViewCellProtocol>)delegate street:(NSString *)street selected:(BOOL)selected
{
  self.delegate = delegate;
  self.label.text = street;
  self.selectedIcon.hidden = !selected;
}

#pragma mark - Actions

- (IBAction)selectAction
{
  self.selectedIcon.hidden = !self.selectedIcon.hidden;
  [self.delegate selectCell:self];
}

@end
