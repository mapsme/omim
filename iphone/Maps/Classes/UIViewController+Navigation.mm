#import "Common.h"
#import "UIViewController+Navigation.h"

@implementation UIViewController (Navigation)

- (UIButton *)backButton
{
  UIImage * backImage = [UIImage imageNamed:@"ic_nav_bar_back"];
  UIImage * backHighlightedImage = [UIImage imageNamed:@"ic_nav_bar_back_press"];
  CGFloat const buttonSide = 44.0;
  UIButton * button = [[UIButton alloc] initWithFrame:{{0, 0}, {buttonSide, buttonSide}}];
  [button setImage:backImage forState:UIControlStateNormal];
  [button setImage:backHighlightedImage forState:UIControlStateHighlighted];
  [button addTarget:self action:@selector(backTap) forControlEvents:UIControlEventTouchUpInside];
  button.imageEdgeInsets = {0, -buttonSide, 0, 0};
  return button;
}

- (void)showBackButton
{
  self.navigationItem.leftBarButtonItem = [[UIBarButtonItem alloc] initWithCustomView:[self backButton]];
}

- (void)backTap
{
  [self.navigationController popViewControllerAnimated:YES];
}

- (UIStoryboard *)mainStoryboard
{
  return [UIStoryboard storyboardWithName:@"Mapsme" bundle:nil];
}

@end
