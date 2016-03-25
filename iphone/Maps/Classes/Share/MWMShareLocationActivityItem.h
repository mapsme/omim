@interface MWMShareLocationActivityItem : NSObject <UIActivityItemSource>

- (instancetype _Nullable)initWithTitle:(NSString *)title location:(CLLocationCoordinate2D)location myPosition:(BOOL)myPosition;

@end
