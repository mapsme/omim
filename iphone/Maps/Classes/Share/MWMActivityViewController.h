@interface MWMActivityViewController : UIActivityViewController

+ (instancetype _Nullable)shareControllerForLocationTitle:(NSString *)title location:(CLLocationCoordinate2D)location
                                     myPosition:(BOOL)myPosition;
+ (instancetype _Nullable)shareControllerForPedestrianRoutesToast;

+ (instancetype _Nullable)shareControllerForEditorViral;

- (void)presentInParentViewController:(UIViewController *)parentVC anchorView:(UIView *)anchorView;

@end
