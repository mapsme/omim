#import "MWMLocationButton.h"
#import "MWMLocationButtonView.h"

#include "Framework.h"

static NSString * const kMWMLocationButtonViewNibName = @"MWMLocationButton";

@interface MWMLocationButton()

@property (nonatomic) IBOutlet MWMLocationButtonView * button;

@end

@implementation MWMLocationButton

- (instancetype)initWithParentView:(UIView *)view
{
  self = [super init];
  if (self)
  {
    [[NSBundle mainBundle] loadNibNamed:kMWMLocationButtonViewNibName owner:self options:nil];
    [view addSubview:self.button];
  }
  return self;
}

- (void)setMyPositionMode:(location::EMyPositionMode)mode
{
  self.button.myPositionMode = mode;
}

- (IBAction)buttonPressed:(id)sender
{
  GetFramework().SwitchMyPositionNextMode();
}

#pragma mark - Properties

- (BOOL)hidden
{
  return self.button.hidden;
}

- (void)setHidden:(BOOL)hidden
{
  self.button.hidden = hidden;
}

@end
