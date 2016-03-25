#import "MWMEditorSelectTableViewCell.h"
#import "UIColor+MapsMeColor.h"
#import "UIImageView+Coloring.h"

@interface MWMEditorSelectTableViewCell ()

@property (weak, nonatomic) IBOutlet UIImageView * _Nullable icon;
@property (weak, nonatomic) IBOutlet UILabel * _Nullable label;
@property (weak, nonatomic) IBOutlet NSLayoutConstraint * _Nullable labelLeadingOffset;
@property (weak, nonatomic) IBOutlet NSLayoutConstraint * _Nullable labelTrailingOffset;

@property (weak, nonatomic) id<MWMEditorCellProtocol> _Nullable delegate;

@end

@implementation MWMEditorSelectTableViewCell

- (void)configWithDelegate:(id<MWMEditorCellProtocol>)delegate
                      icon:(UIImage *)icon
                      text:(NSString *)text
               placeholder:(NSString *)placeholder
{
  self.delegate = delegate;
  self.icon.hidden = NO;
  self.icon.image = icon;
  self.icon.mwm_coloring = MWMImageColoringBlack;
  if (text && text.length != 0)
  {
    self.label.text = text;
    self.label.textColor = [UIColor blackPrimaryText];
  }
  else
  {
    self.label.text = placeholder;
    self.label.textColor = [UIColor blackHintText];
  }
  self.label.preferredMaxLayoutWidth = self.width - self.labelLeadingOffset.constant - self.labelTrailingOffset.constant;
}

- (IBAction)selectAction
{
  [self.delegate cellSelect:self];
}

@end
