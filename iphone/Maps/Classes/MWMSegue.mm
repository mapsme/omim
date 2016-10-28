#import "Common.h"
#import "MWMSegue.h"

@implementation MWMSegue

+ (void)segueFrom:(UIViewController *)source to:(UIViewController *)destination
{
  [[[MWMSegue alloc] initWithIdentifier:@"" source:source destination:destination] perform];
}

- (void)perform
{
  UINavigationController * nc = self.sourceViewController.navigationController;
  UIViewController * dvc = self.destinationViewController;
  [nc showViewController:dvc sender:nil];
}

@end
