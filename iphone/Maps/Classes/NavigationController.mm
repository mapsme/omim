#import "MapsAppDelegate.h"
#import "NavigationController.h"
#import "TableViewController.h"
#import "UIViewController+Navigation.h"
#import "ViewController.h"

@interface NavigationController () <UINavigationControllerDelegate>

@end

@implementation NavigationController

- (UIStatusBarStyle)preferredStatusBarStyle
{
  return UIStatusBarStyleLightContent;
}

- (void)viewDidLoad
{
  [super viewDidLoad];

  self.delegate = self;
}

- (void)navigationController:(UINavigationController *)navigationController willShowViewController:(UIViewController *)viewController animated:(BOOL)animated
{
  NSAssert([viewController isKindOfClass:[ViewController class]] ||
               [viewController isKindOfClass:[TableViewController class]],
           @"Controller must inherit ViewController or TableViewController class");
  ViewController * vc = (ViewController *)viewController;
  [navigationController setNavigationBarHidden:!vc.hasNavigationBar animated:animated];

  if ([navigationController.viewControllers count] > 1)
    [viewController showBackButton];
}

- (BOOL)shouldAutorotate
{
  return YES;
}

@end
